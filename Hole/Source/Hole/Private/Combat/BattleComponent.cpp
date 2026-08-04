// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/BattleComponent.h"
#include "Combat/EnemyCombatAIComponent.h"
#include "UI/CombatHUDWidget.h"
#include "Character/Role.h"
#include "Character/Enemy.h"
#include "Character/BaseCharacter.h"
#include "Component/AttributeComponent.h"
#include "Component/BossIntroComponent.h"
#include "Components/SphereComponent.h"
#include "Subsystem/CombatFormulaSubsystem.h"
#include "DataTable/CombatParamsTable.h"
#include "DataTable/CombatStageTable.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
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

	static ConstructorHelpers::FClassFinder<UCombatHUDWidget> HUDClass(TEXT("/Game/UI/HUD/WBP_CombatHUD"));
	if (HUDClass.Succeeded())
	{
		CombatHUDClass = HUDClass.Class;
	}

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> CombatIMC(TEXT("/Game/Input/IMC_Combat"));
	if (CombatIMC.Succeeded())
	{
		CombatMappingContext = CombatIMC.Object;
	}

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
		GetWorld()->GetTimerManager().ClearTimer(EntryDelayTimer);
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
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(EntryDelayTimer);
	}
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

	// 玩家入场 Montage 占位：有动画则播放并等它播完再进入第 1 回合；没有则直接开始
	if (PlayerEntryMontage && PlayerRole.IsValid())
	{
		const float PlayLength = PlayerEntryMontage->GetPlayLength();
		PlayerRole->PlayAnimMontage(PlayerEntryMontage, 1.0f, PlayerEntrySectionName);
		if (PlayLength > 0.0f)
		{
			GetWorld()->GetTimerManager().SetTimer(EntryDelayTimer, this, &UBattleComponent::StartNewRound, PlayLength, false);
			return;
		}
	}
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
	ActiveClashType = ClashType;
	bClashResolved = false;
	bClashWindowOpen = false;
	PendingClashResult = EClashResult::None;

	SetPhase(EBattlePhase::Clash);

	const float BlockWindow = GetBlockWindow();
	const float DodgeWindow = GetDodgeWindow();
	const float OpenDelay = FMath::Max(0.0f, ClashTelegraphTime - FMath::Max(BlockWindow, DodgeWindow));

	GetWorld()->GetTimerManager().SetTimer(ClashOpenTimer, this, &UBattleComponent::OpenClashWindow, OpenDelay, false);
	GetWorld()->GetTimerManager().SetTimer(ClashImpactTimer, this, &UBattleComponent::OnClashImpact, ClashTelegraphTime, false);

	if (CombatHUD.IsValid())
	{
		const FText Prompt = (ClashType == EClashType::BlueClash)
			? FText::FromString(TEXT("蓝色碰撞！格挡(E) / 闪避(Shift)"))
			: FText::FromString(TEXT("白色碰撞！格挡(E) / 闪避(Shift)"));
		CombatHUD->ShowClashPrompt(Prompt, ClashTelegraphTime);
	}

	OnBattleStateChanged.Broadcast();
}

void UBattleComponent::OpenClashWindow()
{
	bClashWindowOpen = true;
}

void UBattleComponent::OnClashImpact()
{
	if (Phase != EBattlePhase::Clash || bClashResolved)
	{
		return;
	}

	EClashResult Result = PendingClashResult;
	if (Result == EClashResult::None)
	{
		// 未按任何键 → 按格挡失败处理（全额伤害）
		Result = EClashResult::BlockFail;
	}
	ResolveClash(Result);
}

void UBattleComponent::ResolveClash(EClashResult Result)
{
	if (bClashResolved)
	{
		return;
	}
	bClashResolved = true;
	ClearClashTimers();

	float Incoming = PendingIncomingDamage;

	switch (Result)
	{
	case EClashResult::BlockSuccess:
		Incoming = 0.0f;
		if (BossEnemy.IsValid())
		{
			ApplyDamageTo(BossEnemy.Get(), GetPlayerGoldDamage(), PlayerRole.Get());
		}
		break;
	case EClashResult::DodgeSuccess:
		Incoming = 0.0f;
		if (UAttributeComponent* PA = GetPlayerAttr())
		{
			UCombatFormulaSubsystem* Subsystem = GetCombatSubsystem();
			const FCombatParamsRow Defaults;
			const FCombatParamsRow* Params = Subsystem ? Subsystem->GetCombatParams() : nullptr;
			const FCombatParamsRow& P = Params ? *Params : Defaults;
			PA->AddModifier(
				AttributeNames::NextAttackDamageScale(),
				EModifierOp::Multiply,
				P.DodgeBuffDamageScale,
				P.DodgeBuffTurns,
				FName(TEXT("DodgeBuff")));
		}
		break;
	case EClashResult::DodgeFail:
	{
		UCombatFormulaSubsystem* Subsystem = GetCombatSubsystem();
		const FCombatParamsRow Defaults;
		const FCombatParamsRow* Params = Subsystem ? Subsystem->GetCombatParams() : nullptr;
		const FCombatParamsRow& P = Params ? *Params : Defaults;
		Incoming *= P.DodgeFailDamageScale;
		break;
	}
	case EClashResult::BlockFail:
	default:
		// 全额伤害
		break;
	}

	if (CombatHUD.IsValid())
	{
		CombatHUD->HideClashPrompt();
	}

	if (Incoming > 0.0f)
	{
		ApplyDamageTo(PlayerRole.Get(), Incoming, BossEnemy.Get());
	}

	if (Phase != EBattlePhase::Ended)
	{
		EndTurnAndAdvance();
	}
}

void UBattleComponent::ClearClashTimers()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ClashOpenTimer);
		GetWorld()->GetTimerManager().ClearTimer(ClashImpactTimer);
	}
	bClashWindowOpen = false;
}

void UBattleComponent::OnBlockPressed()
{
	if (Phase != EBattlePhase::Clash || bClashResolved)
	{
		return;
	}
	if (bClashWindowOpen)
	{
		ResolveClash(EClashResult::BlockSuccess);
		return;
	}
	// 窗口外按下：先记录失败结果，窗口内再按可覆盖
	PendingClashResult = EClashResult::BlockFail;
}

void UBattleComponent::OnDodgePressed()
{
	if (Phase != EBattlePhase::Clash || bClashResolved)
	{
		return;
	}
	if (bClashWindowOpen)
	{
		ResolveClash(EClashResult::DodgeSuccess);
		return;
	}
	PendingClashResult = EClashResult::DodgeFail;
}

void UBattleComponent::SetupCombatInput()
{
	if (bCombatInputBound || !PlayerRole.IsValid())
	{
		return;
	}

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerRole->InputComponent);
	if (!EIC)
	{
		return;
	}

	if (BlockAction) EIC->BindAction(BlockAction, ETriggerEvent::Started, this, &UBattleComponent::OnBlockPressed);
	if (DodgeAction) EIC->BindAction(DodgeAction, ETriggerEvent::Started, this, &UBattleComponent::OnDodgePressed);

	bCombatInputBound = true;
}

void UBattleComponent::UnbindCombatInput()
{
	if (!bCombatInputBound || !PlayerRole.IsValid())
	{
		return;
	}

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerRole->InputComponent))
	{
		EIC->ClearBindingsForObject(this);
	}
	bCombatInputBound = false;
}

void UBattleComponent::AddCombatMapping()
{
	if (bCombatMappingAdded || !CombatMappingContext)
	{
		return;
	}

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->GetLocalPlayer())
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		// 优先级 10 > 探索 IMC 的 0，战斗期间 Shift 等键被战斗映射覆盖
		Subsystem->AddMappingContext(CombatMappingContext, 10);
		bCombatMappingAdded = true;
	}
}

void UBattleComponent::RemoveCombatMapping()
{
	if (!bCombatMappingAdded || !CombatMappingContext)
	{
		return;
	}

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (PC && PC->GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->RemoveMappingContext(CombatMappingContext);
		}
	}
	bCombatMappingAdded = false;
}

void UBattleComponent::PositionBattleActors()
{
	ARole* Role = PlayerRole.Get();
	AEnemy* Boss = BossEnemy.Get();
	if (!Role || !Boss)
	{
		return;
	}

	UCombatFormulaSubsystem* Subsystem = GetCombatSubsystem();
	const FCombatStageRow Defaults;
	const FCombatStageRow* StageRow = Subsystem ? Subsystem->GetBattleStageRow() : nullptr;
	const FCombatStageRow& Stage = StageRow ? *StageRow : Defaults;

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;

	// 保存探索状态（战斗结束后恢复）
	PlayerStartLocation = Role->GetActorLocation();
	PlayerStartActorRotation = Role->GetActorRotation();
	BossStartLocation = Boss->GetActorLocation();
	BossStartRotation = Boss->GetActorRotation();
	if (PC)
	{
		PlayerStartControlRotation = PC->GetControlRotation();
	}
	if (Role->SpringArm)
	{
		OriginalArmLength = Role->SpringArm->TargetArmLength;
		bOriginalCameraLag = Role->SpringArm->bEnableCameraRotationLag;
		OriginalLagSpeed = Role->SpringArm->CameraRotationLagSpeed;
		OriginalSocketOffset = Role->SpringArm->SocketOffset;
		OriginalTargetOffset = Role->SpringArm->TargetOffset;
	}
	if (Role->FollowCamera)
	{
		OriginalFOV = Role->FollowCamera->FieldOfView;
	}

	// 玩家固定站位（相对 Boss 的策划偏移；支持世界空间或 Boss 本地空间）
	FVector PlayerSpot = Stage.bPlayerOffsetInBossLocalSpace
		? BossStartLocation + BossStartRotation.RotateVector(Stage.PlayerBattleOffset)
		: BossStartLocation + Stage.PlayerBattleOffset;

	// 清除移动残留速度，避免传送后带着旧速度继续移动/下落
	if (UCharacterMovementComponent* MoveComp = Role->GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
	}

	// 落点地面检测：把玩家放到地面上（Boss 根节点 Z 可能远高于地面，直接复制会“从天而降”）
	{
		const FVector TraceStart = PlayerSpot + FVector(0.0f, 0.0f, 800.0f);
		const FVector TraceEnd = PlayerSpot - FVector(0.0f, 0.0f, 1500.0f);
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(Role);
		Params.AddIgnoredActor(Boss);
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
		{
			PlayerSpot.Z = Hit.ImpactPoint.Z;
			if (UCapsuleComponent* Capsule = Role->GetCapsuleComponent())
			{
				PlayerSpot.Z += Capsule->GetScaledCapsuleHalfHeight();
			}
		}
	}

	Role->SetActorLocation(PlayerSpot, false, nullptr, ETeleportType::TeleportPhysics);

	// Boss 面向玩家
	if (Stage.bBossFacePlayer)
	{
		FVector DirToPlayer = Role->GetActorLocation() - BossStartLocation;
		DirToPlayer.Z = 0.0f;
		if (!DirToPlayer.IsNearlyZero())
		{
			DirToPlayer.Normalize();
		}
		Boss->SetActorRotation(FRotator(0.0f, DirToPlayer.Rotation().Yaw + Stage.BossFacingYawOffset, 0.0f));
	}

	// 玩家面向 Boss（固定角度）
	FVector DirToBoss = BossStartLocation - Role->GetActorLocation();
	DirToBoss.Z = 0.0f;
	if (!DirToBoss.IsNearlyZero())
	{
		DirToBoss.Normalize();
	}
	const FRotator PlayerFaceRot = DirToBoss.Rotation();
	if (Stage.bPlayerFaceBoss)
	{
		Role->SetActorRotation(FRotator(0.0f, PlayerFaceRot.Yaw + Stage.PlayerFacingYawOffset, 0.0f));
	}

	// 固定玩家摄像机：锁定控制旋转 + 应用 Spring 参数（数值来自 DT_BattleStage）
	if (PC)
	{
		PC->SetControlRotation(FRotator(Stage.CameraPitch, PlayerFaceRot.Yaw + Stage.CameraYawOffset, 0.0f));
	}
	if (Role->SpringArm)
	{
		Role->SpringArm->TargetArmLength = Stage.CameraArmLength;
		Role->SpringArm->SocketOffset = Stage.SpringSocketOffset;
		Role->SpringArm->TargetOffset = Stage.SpringTargetOffset;
		Role->SpringArm->bEnableCameraRotationLag = Stage.bSpringEnableCameraLag;
		if (Stage.bSpringEnableCameraLag)
		{
			Role->SpringArm->CameraRotationLagSpeed = Stage.SpringCameraLagSpeed;
		}
	}
	if (Role->FollowCamera)
	{
		Role->FollowCamera->SetFieldOfView(Stage.CameraFOV);
	}
}

void UBattleComponent::RestoreExplorationState()
{
	ARole* Role = PlayerRole.Get();
	AEnemy* Boss = BossEnemy.Get();

	if (Role)
	{
		Role->SetActorLocation(PlayerStartLocation, false, nullptr, ETeleportType::TeleportPhysics);
		Role->SetActorRotation(PlayerStartActorRotation);
		if (Role->SpringArm)
		{
			Role->SpringArm->TargetArmLength = OriginalArmLength;
			Role->SpringArm->bEnableCameraRotationLag = bOriginalCameraLag;
			Role->SpringArm->CameraRotationLagSpeed = OriginalLagSpeed;
			Role->SpringArm->SocketOffset = OriginalSocketOffset;
			Role->SpringArm->TargetOffset = OriginalTargetOffset;
		}
		if (Role->FollowCamera)
		{
			Role->FollowCamera->SetFieldOfView(OriginalFOV);
		}
	}
	if (Boss)
	{
		Boss->SetActorLocation(BossStartLocation, false, nullptr, ETeleportType::TeleportPhysics);
		Boss->SetActorRotation(BossStartRotation);
	}
	if (GetWorld())
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			PC->SetControlRotation(PlayerStartControlRotation);
		}
	}
}

void UBattleComponent::LockPlayer()
{
	if (PlayerRole.IsValid())
	{
		PlayerRole->SetCinematicLocked(true);
	}
}

void UBattleComponent::UnlockPlayer()
{
	if (PlayerRole.IsValid())
	{
		PlayerRole->SetCinematicLocked(false);
	}
}

void UBattleComponent::ShowHUD()
{
	if (CombatHUD.IsValid() || !CombatHUDClass)
	{
		return;
	}

	CombatHUD = CreateWidget<UCombatHUDWidget>(GetWorld(), CombatHUDClass);
	if (!CombatHUD.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::ShowHUD - 创建 WBP_CombatHUD 失败"));
		return;
	}

	CombatHUD->AddToViewport(10);
	CombatHUD->BindToBattle(this);

	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
		bMouseCursorShown = true;
	}
}

void UBattleComponent::HideHUD()
{
	if (CombatHUD.IsValid())
	{
		CombatHUD->RemoveFromParent();
		CombatHUD.Reset();
	}

	if (bMouseCursorShown)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			PC->bShowMouseCursor = false;
			PC->SetInputMode(FInputModeGameOnly());
		}
		bMouseCursorShown = false;
	}
}

void UBattleComponent::FinishBattle(bool bPlayerWon)
{
	if (Phase == EBattlePhase::Ended)
	{
		return;
	}

	SetPhase(EBattlePhase::Ended);
	ClearClashTimers();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(EntryDelayTimer);
	}
	UnbindCombatInput();
	RemoveCombatMapping();
	if (CombatHUD.IsValid())
	{
		CombatHUD->HideClashPrompt();
		CombatHUD->ShowResult(bPlayerWon ? FText::FromString(TEXT("战斗胜利")) : FText::FromString(TEXT("战斗失败")));
	}
	OnBattleStateChanged.Broadcast();

	const float Delay = bPlayerWon ? 2.0f : DefeatRestartDelay;
	if (bPlayerWon)
	{
		GetWorld()->GetTimerManager().SetTimer(EndDelayTimer, this, &UBattleComponent::HandleVictoryCleanup, Delay, false);
	}
	else
	{
		GetWorld()->GetTimerManager().SetTimer(EndDelayTimer, this, &UBattleComponent::HandleDefeatRestart, Delay, false);
	}
}

void UBattleComponent::HandleVictoryCleanup()
{
	bBossDefeated = true;
	HideHUD();
	RestoreExplorationState();
	UnlockPlayer();
	SetPhase(EBattlePhase::Idle);
	OnBattleStateChanged.Broadcast();
}

void UBattleComponent::HandleDefeatRestart()
{
	HideHUD();
	RestoreExplorationState();
	UnlockPlayer();
	ResetForRetry();

	// 回到 Boss 触发点：玩家已恢复为战斗前位置（触发球内），重置触发后强制刷新重叠 → 直接重播 Boss 动画
	if (BossEnemy.IsValid())
	{
		if (UBossIntroComponent* Intro = BossEnemy->FindComponentByClass<UBossIntroComponent>())
		{
			Intro->ResetIntro();
			Intro->TriggerSphere->UpdateOverlaps();
		}
	}

	SetPhase(EBattlePhase::Idle);
	OnBattleStateChanged.Broadcast();
}

void UBattleComponent::ResetForRetry()
{
	if (PlayerRole.IsValid())
	{
		PlayerRole->CurrentHealth = PlayerRole->GetMaxHealth();
		if (UAttributeComponent* PA = GetPlayerAttr())
		{
			PA->RemoveAllTemporaryModifiers();
		}
	}
	if (BossEnemy.IsValid())
	{
		BossEnemy->CurrentHealth = BossEnemy->GetMaxHealth();
		if (UAttributeComponent* EA = GetEnemyAttr())
		{
			EA->RemoveAllTemporaryModifiers();
		}
	}

	PlayerChargeStacks = 0;
	EnemyChargeStacks = 0;
	RoundNumber = 0;
	bPlayerChoseAction = false;
	bPlayerExtraTurnPending = false;
	bEnemyExtraTurnPending = false;
	bClashResolved = false;
	bClashWindowOpen = false;
	PendingClashResult = EClashResult::None;
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
	UAttributeComponent* Attr = GetPlayerAttr();
	if (Attr)
	{
		return Attr->GetFinal(AttributeNames::BlockWindow());
	}
	return 0.25f;
}

float UBattleComponent::GetDodgeWindow() const
{
	UAttributeComponent* Attr = GetPlayerAttr();
	if (Attr)
	{
		return Attr->GetFinal(AttributeNames::DodgeWindow());
	}
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
