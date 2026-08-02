// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/BattleComponent.h"
#include "Combat/EnemyCombatAIComponent.h"
#include "UI/CombatHUDWidget.h"
#include "Character/Role.h"
#include "Character/Enemy.h"
#include "Character/BaseCharacter.h"
#include "Component/AttributeComponent.h"
#include "Component/BossIntroComponent.h"
#include "Subsystem/CombatFormulaSubsystem.h"
#include "DataTable/CombatParamsTable.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

UBattleComponent::UBattleComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FClassFinder<UCombatHUDWidget> HUDClass(TEXT("/Game/UI/WBP_CombatHUD"));
	if (HUDClass.Succeeded())
	{
		CombatHUDClass = HUDClass.Class;
	}

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> CombatIMC(TEXT("/Game/Input/IMC_Combat"));
	if (CombatIMC.Succeeded())
	{
		CombatMappingContext = CombatIMC.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> RedDefense(TEXT("/Game/Input/IA_CombatRedDefense"));
	if (RedDefense.Succeeded()) RedDefenseAction = RedDefense.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> BlueAttack(TEXT("/Game/Input/IA_CombatBlueAttack"));
	if (BlueAttack.Succeeded()) BlueAttackAction = BlueAttack.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> WhiteAttack(TEXT("/Game/Input/IA_CombatWhiteAttack"));
	if (WhiteAttack.Succeeded()) WhiteAttackAction = WhiteAttack.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> Charge(TEXT("/Game/Input/IA_CombatCharge"));
	if (Charge.Succeeded()) ChargeAction = Charge.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> Skill(TEXT("/Game/Input/IA_CombatSkill"));
	if (Skill.Succeeded()) SkillAction = Skill.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> Block(TEXT("/Game/Input/IA_CombatBlock"));
	if (Block.Succeeded()) BlockAction = Block.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> Dodge(TEXT("/Game/Input/IA_CombatDodge"));
	if (Dodge.Succeeded()) DodgeAction = Dodge.Object;
}

void UBattleComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ARole* Role = Cast<ARole>(GetOwner()))
	{
		PlayerRole = Role;
	}

	BossEnemy = FindBossEnemy();
	if (BossEnemy.IsValid())
	{
		if (UBossIntroComponent* Intro = BossEnemy->FindComponentByClass<UBossIntroComponent>())
		{
			Intro->OnIntroFinished.AddDynamic(this, &UBattleComponent::HandleIntroFinished);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::BeginPlay - 场景中未找到 Tag=Boss 的 AEnemy"));
	}
}

void UBattleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearClashTimers();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(EndDelayTimer);
	}
	UnbindCombatInput();
	RemoveCombatMapping();

	Super::EndPlay(EndPlayReason);
}

// ==================== 公开接口 ====================

void UBattleComponent::StartBattle()
{
	if (Phase != EBattlePhase::Idle || bBossDefeated)
	{
		return;
	}

	if (!BossEnemy.IsValid())
	{
		BossEnemy = FindBossEnemy();
	}
	if (!BossEnemy.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::StartBattle - 未找到 Tag=Boss 的 AEnemy"));
		return;
	}

	PlayerRole = Cast<ARole>(GetOwner());
	if (!PlayerRole.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::StartBattle - Owner 不是 ARole"));
		return;
	}

	EnemyAI = BossEnemy->FindComponentByClass<UEnemyCombatAIComponent>();
	EnterBattle();
}

void UBattleComponent::EndBattle()
{
	if (Phase == EBattlePhase::Idle || Phase == EBattlePhase::Ended)
	{
		return;
	}

	ClearClashTimers();
	UnbindCombatInput();
	RemoveCombatMapping();
	if (CombatHUD.IsValid())
	{
		CombatHUD->HideClashPrompt();
	}
	HideHUD();
	RestoreExplorationState();
	UnlockPlayer();
	SetPhase(EBattlePhase::Idle);
	OnBattleStateChanged.Broadcast();
}

void UBattleComponent::PlayerChooseAction(EBattleAction Action)
{
	if (Phase != EBattlePhase::ActionSelect || bPlayerChoseAction)
	{
		return;
	}
	if (Action == EBattleAction::Skill)
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::PlayerChooseAction - 技能系统未实装"));
		return;
	}
	if (bPlayerExtraTurnPending && Action != EBattleAction::BlueAttack && Action != EBattleAction::Charge)
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::PlayerChooseAction - 额外回合只能蓝攻或蓄力"));
		return;
	}

	bPlayerChoseAction = true;
	PlayerLastAction = Action;
	OnBattleStateChanged.Broadcast();

	if (bPlayerExtraTurnPending)
	{
		const FTurnResolution Resolution = ResolveExtraTurn(true, Action);
		bPlayerExtraTurnPending = false;
		ApplyResolution(Resolution);
		if (!bClashStarted)
		{
			EndTurnAndAdvance();
		}
		return;
	}

	const FTurnResolution Resolution = ResolveNormalTurn(Action, EnemyChosenAction);
	ApplyResolution(Resolution);
	if (!bClashStarted)
	{
		EndTurnAndAdvance();
	}
}

void UBattleComponent::SetEnemyForcedAction(EBattleAction Action, bool bEnabled)
{
	ForcedEnemyAction = Action;
	bForcedEnemyActionEnabled = bEnabled;
}

void UBattleComponent::DebugSetPlayerHealth(float Value)
{
	if (PlayerRole.IsValid())
	{
		PlayerRole->CurrentHealth = FMath::Clamp(Value, 0.0f, PlayerRole->GetMaxHealth());
	}
	OnBattleStateChanged.Broadcast();
}

void UBattleComponent::DebugSetEnemyHealth(float Value)
{
	if (BossEnemy.IsValid())
	{
		BossEnemy->CurrentHealth = FMath::Clamp(Value, 0.0f, BossEnemy->GetMaxHealth());
	}
	OnBattleStateChanged.Broadcast();
}

ABaseCharacter* UBattleComponent::GetPlayerCharacter() const
{
	return PlayerRole.Get();
}

AEnemy* UBattleComponent::GetBossEnemy() const
{
	return BossEnemy.Get();
}

// ==================== 内部流程（Task 6/7/8 补全） ====================

AEnemy* UBattleComponent::FindBossEnemy() const
{
	if (!GetWorld())
	{
		return nullptr;
	}
	for (TActorIterator<AEnemy> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(FName(TEXT("Boss"))))
		{
			return *It;
		}
	}
	return nullptr;
}

void UBattleComponent::HandleIntroFinished()
{
	if (Phase == EBattlePhase::Idle && BossEnemy.IsValid())
	{
		StartBattle();
	}
}

void UBattleComponent::EnterBattle()
{
	PositionBattleActors();
	LockPlayer();
	AddCombatMapping();
	SetupCombatInput();
	ShowHUD();
	SetPhase(EBattlePhase::Entering);
	OnBattleStateChanged.Broadcast();
	StartNewRound();
}

void UBattleComponent::StartNewRound()
{
	RoundNumber++;
	bPlayerChoseAction = false;
	EnemyChosenAction = EBattleAction::None;
	bPlayerExtraTurnPending = false;
	bEnemyExtraTurnPending = false;
	ChooseEnemyAction(false);
	SetPhase(EBattlePhase::ActionSelect);
	OnBattleStateChanged.Broadcast();
}

void UBattleComponent::ChooseEnemyAction(bool bExtraTurn)
{
	if (!bExtraTurn && bForcedEnemyActionEnabled)
	{
		EnemyChosenAction = ForcedEnemyAction;
		return;
	}

	if (EnemyAI.IsValid())
	{
		EnemyChosenAction = EnemyAI->ChooseAction(RoundNumber, PlayerLastAction, bExtraTurn, EnemyChargeStacks);
	}
	else if (bExtraTurn)
	{
		EnemyChosenAction = FMath::RandBool() ? EBattleAction::BlueAttack : EBattleAction::Charge;
	}
	else
	{
		switch (FMath::RandRange(0, 3))
		{
		case 0: EnemyChosenAction = EBattleAction::RedDefense; break;
		case 1: EnemyChosenAction = EBattleAction::BlueAttack; break;
		case 2: EnemyChosenAction = EBattleAction::WhiteAttack; break;
		default: EnemyChosenAction = EBattleAction::Charge; break;
		}
	}
}

void UBattleComponent::StartPlayerExtraTurn()
{
	bPlayerChoseAction = false;
	SetPhase(EBattlePhase::ActionSelect);
	OnBattleStateChanged.Broadcast();
}

void UBattleComponent::StartEnemyExtraTurn()
{
	ChooseEnemyAction(true);
	SetPhase(EBattlePhase::Resolving);
	OnBattleStateChanged.Broadcast();
}

// ---- 以下方法在 Task 6 / Task 7 / Task 8 中替换为完整实现 ----

FTurnResolution UBattleComponent::ResolveNormalTurn(EBattleAction PlayerAction, EBattleAction EnemyAction)
{
	FTurnResolution R;
	const int32 MaxStacks = GetMaxChargeStacks(true);

	switch (PlayerAction)
	{
	case EBattleAction::RedDefense:
	{
		switch (EnemyAction)
		{
		case EBattleAction::RedDefense:
			// 红 vs 红：本回合跳过
			break;
		case EBattleAction::BlueAttack:
			// 红防克蓝攻 → 金色反击；敌方 2 层蓄力出蓝刀时红防正面承受强化蓝攻
			if (EnemyChargeStacks >= 2)
			{
				R.PlayerDamageTaken = GetEnemyBlueDamage(EnemyChargeStacks);
			}
			else
			{
				R.EnemyDamageTaken = GetPlayerGoldDamage();
			}
			EnemyChargeStacks = 0;
			break;
		case EBattleAction::WhiteAttack:
			// 红防被白克 → 全额受伤
			R.PlayerDamageTaken = GetEnemyWhiteDamage();
			break;
		case EBattleAction::Charge:
			// 敌方蓄力：1 层无事；蓄满 2 层视为直接发动强化蓝攻
		{
			const int32 NewStacks = FMath::Min(EnemyChargeStacks + 1, MaxStacks);
			if (NewStacks >= MaxStacks && MaxStacks >= 2)
			{
				R.PlayerDamageTaken = GetEnemyBlueDamage(NewStacks);
				EnemyChargeStacks = 0;
			}
			else
			{
				EnemyChargeStacks = NewStacks;
			}
			break;
		}
		default:
			break;
		}
		break;
	}
	case EBattleAction::BlueAttack:
	{
		switch (EnemyAction)
		{
		case EBattleAction::RedDefense:
			// 蓝攻被红防克制 → 玩家吃金色反击伤害
			R.PlayerDamageTaken = GetEnemyGoldDamage();
			PlayerChargeStacks = 0;
			break;
		case EBattleAction::BlueAttack:
			// 同色碰撞
			R.bClash = true;
			R.ClashType = EClashType::BlueClash;
			R.PlayerDamageTaken = GetEnemyBlueDamage(EnemyChargeStacks);
			R.EnemyDamageTaken = GetPlayerBlueDamage(PlayerChargeStacks);
			PlayerChargeStacks = 0;
			EnemyChargeStacks = 0;
			break;
		case EBattleAction::WhiteAttack:
			// 蓝克白 → 玩家优势
			R.EnemyDamageTaken = GetPlayerBlueDamage(PlayerChargeStacks);
			PlayerChargeStacks = 0;
			break;
		case EBattleAction::Charge:
			// 蓝攻打断蓄力，全额伤害
			R.EnemyDamageTaken = GetPlayerBlueDamage(PlayerChargeStacks);
			R.bEnemyChargeInterrupted = true;
			PlayerChargeStacks = 0;
			EnemyChargeStacks = 0;
			break;
		default:
			break;
		}
		break;
	}
	case EBattleAction::WhiteAttack:
	{
		switch (EnemyAction)
		{
		case EBattleAction::RedDefense:
			// 白克红 → 玩家优势
			R.EnemyDamageTaken = GetPlayerWhiteDamage();
			break;
		case EBattleAction::BlueAttack:
			// 白被蓝克 → 玩家吃全额蓝攻
			R.PlayerDamageTaken = GetEnemyBlueDamage(EnemyChargeStacks);
			EnemyChargeStacks = 0;
			break;
		case EBattleAction::WhiteAttack:
			// 同色碰撞
			R.bClash = true;
			R.ClashType = EClashType::WhiteClash;
			R.PlayerDamageTaken = GetEnemyWhiteDamage();
			R.EnemyDamageTaken = GetPlayerWhiteDamage();
			break;
		case EBattleAction::Charge:
			// 蓄力抵抗白攻：微量伤害 + 额外回合，不打断蓄力
			R.EnemyDamageTaken = GetPlayerWhiteDamage() * GetChargeResistScale();
			EnemyChargeStacks = FMath::Min(EnemyChargeStacks + 1, MaxStacks);
			R.bEnemyExtraTurn = true;
			break;
		default:
			break;
		}
		break;
	}
	case EBattleAction::Charge:
	{
		switch (EnemyAction)
		{
		case EBattleAction::RedDefense:
			// 玩家蓄力对红防：1 层无事；蓄满 2 层直接发动强化蓝攻
		{
			const int32 NewStacks = FMath::Min(PlayerChargeStacks + 1, MaxStacks);
			if (NewStacks >= MaxStacks && MaxStacks >= 2)
			{
				R.EnemyDamageTaken = GetPlayerBlueDamage(NewStacks);
				PlayerChargeStacks = 0;
			}
			else
			{
				PlayerChargeStacks = NewStacks;
			}
			break;
		}
		case EBattleAction::BlueAttack:
			// 蓄力被蓝攻打断，玩家吃全额蓝攻
			R.PlayerDamageTaken = GetEnemyBlueDamage(EnemyChargeStacks);
			R.bPlayerChargeInterrupted = true;
			PlayerChargeStacks = 0;
			EnemyChargeStacks = 0;
			break;
		case EBattleAction::WhiteAttack:
			// 蓄力抵抗白攻：微量伤害 + 额外回合
			R.PlayerDamageTaken = GetEnemyWhiteDamage() * GetChargeResistScale();
			PlayerChargeStacks = FMath::Min(PlayerChargeStacks + 1, MaxStacks);
			R.bPlayerExtraTurn = true;
			break;
		case EBattleAction::Charge:
			// 双方蓄力：都 +1 层，无事发生
			PlayerChargeStacks = FMath::Min(PlayerChargeStacks + 1, MaxStacks);
			EnemyChargeStacks = FMath::Min(EnemyChargeStacks + 1, MaxStacks);
			break;
		default:
			break;
		}
		break;
	}
	default:
		break;
	}

	return R;
}

FTurnResolution UBattleComponent::ResolveExtraTurn(bool bPlayerTurn, EBattleAction Action)
{
	FTurnResolution R;

	if (Action == EBattleAction::BlueAttack)
	{
		// 额外回合出蓝刀：必定命中，出刀后解除蓄力
		if (bPlayerTurn)
		{
			R.EnemyDamageTaken = GetPlayerBlueDamage(PlayerChargeStacks);
			PlayerChargeStacks = 0;
		}
		else
		{
			R.PlayerDamageTaken = GetEnemyBlueDamage(EnemyChargeStacks);
			EnemyChargeStacks = 0;
		}
	}
	else if (Action == EBattleAction::Charge)
	{
		const int32 MaxStacks = GetMaxChargeStacks(bPlayerTurn);
		if (bPlayerTurn)
		{
			PlayerChargeStacks = FMath::Min(PlayerChargeStacks + 1, MaxStacks);
		}
		else
		{
			EnemyChargeStacks = FMath::Min(EnemyChargeStacks + 1, MaxStacks);
		}
	}

	return R;
}

void UBattleComponent::ApplyResolution(const FTurnResolution& Resolution)
{
	bClashStarted = false;
	bPlayerExtraTurnPending = Resolution.bPlayerExtraTurn;
	bEnemyExtraTurnPending = Resolution.bEnemyExtraTurn;

	if (Resolution.bClash)
	{
		PendingIncomingDamage = Resolution.PlayerDamageTaken;
		PendingOutgoingDamage = Resolution.EnemyDamageTaken;

		// 同色碰撞：玩家这刀先命中敌人，敌人这刀由实时防御决定
		if (Resolution.EnemyDamageTaken > 0.0f)
		{
			ApplyDamageTo(BossEnemy.Get(), Resolution.EnemyDamageTaken, PlayerRole.Get());
		}
		if (Phase != EBattlePhase::Ended)
		{
			StartClash(Resolution.ClashType);
			bClashStarted = true;
		}
		return;
	}

	if (Resolution.PlayerDamageTaken > 0.0f)
	{
		ApplyDamageTo(PlayerRole.Get(), Resolution.PlayerDamageTaken, BossEnemy.Get());
	}
	if (Resolution.EnemyDamageTaken > 0.0f)
	{
		ApplyDamageTo(BossEnemy.Get(), Resolution.EnemyDamageTaken, PlayerRole.Get());
	}
}

void UBattleComponent::ApplyDamageTo(ABaseCharacter* Target, float Amount, AActor* Causer)
{
	if (!Target || Amount <= 0.0f || Target->IsDead())
	{
		return;
	}

	Target->ReceiveDamage(Amount, Causer);
	OnBattleStateChanged.Broadcast();

	if (Target->IsDead())
	{
		FinishBattle(Target == BossEnemy.Get());
	}
}

void UBattleComponent::EndTurnAndAdvance()
{
	if (Phase == EBattlePhase::Ended)
	{
		return;
	}

	// 回合结束：buff 倒计时（含闪避Buff）
	if (UAttributeComponent* PA = GetPlayerAttr())
	{
		PA->TickTurn();
	}
	if (UAttributeComponent* EA = GetEnemyAttr())
	{
		EA->TickTurn();
	}

	if (bPlayerExtraTurnPending)
	{
		StartPlayerExtraTurn();
		return;
	}
	if (bEnemyExtraTurnPending)
	{
		bEnemyExtraTurnPending = false;
		StartEnemyExtraTurn();
		SetPhase(EBattlePhase::Resolving);
		const FTurnResolution Resolution = ResolveExtraTurn(false, EnemyChosenAction);
		ApplyResolution(Resolution);
		if (!bClashStarted)
		{
			EndTurnAndAdvance();
		}
		return;
	}

	StartNewRound();
}

void UBattleComponent::StartClash(EClashType ClashType)
{
	// TODO-TASK7
}

void UBattleComponent::OpenClashWindow()
{
	// TODO-TASK7
}

void UBattleComponent::OnClashImpact()
{
	// TODO-TASK7
}

void UBattleComponent::ResolveClash(EClashResult Result)
{
	// TODO-TASK7
}

void UBattleComponent::ClearClashTimers()
{
	// TODO-TASK7
}

void UBattleComponent::OnRedDefensePressed()
{
	PlayerChooseAction(EBattleAction::RedDefense);
}

void UBattleComponent::OnBlueAttackPressed()
{
	PlayerChooseAction(EBattleAction::BlueAttack);
}

void UBattleComponent::OnWhiteAttackPressed()
{
	PlayerChooseAction(EBattleAction::WhiteAttack);
}

void UBattleComponent::OnChargePressed()
{
	PlayerChooseAction(EBattleAction::Charge);
}

void UBattleComponent::OnSkillPressed()
{
	PlayerChooseAction(EBattleAction::Skill);
}

void UBattleComponent::OnBlockPressed()
{
	// TODO-TASK7
}

void UBattleComponent::OnDodgePressed()
{
	// TODO-TASK7
}

void UBattleComponent::SetupCombatInput()
{
	// TODO-TASK8
}

void UBattleComponent::UnbindCombatInput()
{
	// TODO-TASK8
}

void UBattleComponent::AddCombatMapping()
{
	// TODO-TASK8
}

void UBattleComponent::RemoveCombatMapping()
{
	// TODO-TASK8
}

void UBattleComponent::PositionBattleActors()
{
	// TODO-TASK8
}

void UBattleComponent::RestoreExplorationState()
{
	// TODO-TASK8
}

void UBattleComponent::LockPlayer()
{
	// TODO-TASK8
}

void UBattleComponent::UnlockPlayer()
{
	// TODO-TASK8
}

void UBattleComponent::ShowHUD()
{
	// TODO-TASK8
}

void UBattleComponent::HideHUD()
{
	// TODO-TASK8
}

void UBattleComponent::FinishBattle(bool bPlayerWon)
{
	// TODO-TASK8
}

void UBattleComponent::HandleVictoryCleanup()
{
	// TODO-TASK8
}

void UBattleComponent::HandleDefeatRestart()
{
	// TODO-TASK8
}

void UBattleComponent::ResetForRetry()
{
	// TODO-TASK8
}

float UBattleComponent::GetPlayerWhiteDamage() const
{
	UCombatFormulaSubsystem* Subsystem = GetCombatSubsystem();
	return Subsystem ? Subsystem->CalculateWhiteDamage(GetPlayerAttr(), 0.0f) : 0.0f;
}

float UBattleComponent::GetPlayerBlueDamage(int32 Stacks) const
{
	UCombatFormulaSubsystem* Subsystem = GetCombatSubsystem();
	return Subsystem ? Subsystem->CalculateBlueDamage(GetPlayerAttr(), 0.0f, Stacks) : 0.0f;
}

float UBattleComponent::GetEnemyWhiteDamage() const
{
	UCombatFormulaSubsystem* Subsystem = GetCombatSubsystem();
	return Subsystem ? Subsystem->CalculateWhiteDamage(GetEnemyAttr(), 0.0f) : 0.0f;
}

float UBattleComponent::GetEnemyBlueDamage(int32 Stacks) const
{
	UCombatFormulaSubsystem* Subsystem = GetCombatSubsystem();
	return Subsystem ? Subsystem->CalculateBlueDamage(GetEnemyAttr(), 0.0f, Stacks) : 0.0f;
}

float UBattleComponent::GetPlayerGoldDamage() const
{
	UCombatFormulaSubsystem* Subsystem = GetCombatSubsystem();
	return Subsystem ? Subsystem->CalculateGoldDamage(GetPlayerAttr()) : 0.0f;
}

float UBattleComponent::GetEnemyGoldDamage() const
{
	UCombatFormulaSubsystem* Subsystem = GetCombatSubsystem();
	return Subsystem ? Subsystem->CalculateGoldDamage(GetEnemyAttr()) : 0.0f;
}

float UBattleComponent::GetChargeResistScale() const
{
	UCombatFormulaSubsystem* Subsystem = GetCombatSubsystem();
	if (!Subsystem)
	{
		return 0.3f;
	}
	const FCombatParamsRow Defaults;
	const FCombatParamsRow* Params = Subsystem->GetCombatParams();
	return Params ? Params->WhiteInterruptChargeDamageScale : Defaults.WhiteInterruptChargeDamageScale;
}

int32 UBattleComponent::GetMaxChargeStacks(bool bPlayer) const
{
	UAttributeComponent* Attr = bPlayer ? GetPlayerAttr() : GetEnemyAttr();
	if (Attr)
	{
		return FMath::Max(1, FMath::RoundToInt(Attr->GetFinal(AttributeNames::MaxChargeStacks())));
	}
	return 2;
}

float UBattleComponent::GetBlockWindow() const
{
	// TODO-TASK7
	return 0.25f;
}

float UBattleComponent::GetDodgeWindow() const
{
	// TODO-TASK7
	return 0.35f;
}

UCombatFormulaSubsystem* UBattleComponent::GetCombatSubsystem() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	UGameInstance* GameInstance = World->GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UCombatFormulaSubsystem>() : nullptr;
}

UAttributeComponent* UBattleComponent::GetPlayerAttr() const
{
	return PlayerRole.IsValid() ? PlayerRole->AttributeComponent : nullptr;
}

UAttributeComponent* UBattleComponent::GetEnemyAttr() const
{
	return BossEnemy.IsValid() ? BossEnemy->AttributeComponent : nullptr;
}

void UBattleComponent::SetPhase(EBattlePhase NewPhase)
{
	Phase = NewPhase;
}
