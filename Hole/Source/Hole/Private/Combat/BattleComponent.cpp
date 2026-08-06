// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/BattleComponent.h"
#include "Combat/EnemyCombatAIComponent.h"
#include "UI/CombatHUDWidget.h"
#include "UI/BattleResultHUDWidget.h"
#include "Character/Role.h"
#include "Character/Enemy.h"
#include "Character/BaseCharacter.h"
#include "Component/AttributeComponent.h"
#include "Component/BossIntroComponent.h"
#include "Component/WeaponVisualComponent.h"
#include "Components/SphereComponent.h"
#include "Subsystem/CombatFormulaSubsystem.h"
#include "DataTable/CombatParamsTable.h"
#include "DataTable/CombatStageTable.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/BaseCharacterAnimInstance.h"
#include "Animation/AnimNotifies/Combat/AnimNotify_CombatDamage.h"
#include "Animation/AnimNotifies/Combat/AnimNotify_CombatMarker.h"
#include "DataTable/CombatAnimConfigTable.h"
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

	static ConstructorHelpers::FClassFinder<UBattleResultHUDWidget> ResultHUDClass(TEXT("/Game/UI/HUD/WBP_BattleResult"));
	if (ResultHUDClass.Succeeded())
	{
		BattleResultHUDClass = ResultHUDClass.Class;
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
	if (ResultHUD.IsValid())
	{
		ResultHUD->RemoveFromParent();
		ResultHUD.Reset();
	}

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
	HideResultHUD();
	SheathePlayerWeapon();
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
	if (Action == EBattleAction::BlueAttack && PlayerChargeStacks <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::PlayerChooseAction - 蓝攻需要至少 1 层蓄力"));
		return;
	}
	if (Action == EBattleAction::Charge && PlayerChargeStacks >= GetMaxChargeStacks(true))
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::PlayerChooseAction - 蓄力已达上限"));
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

	// 入场动画优先读 DT_CombatAnimConfig.Entry，BP 字段回退
	UAnimMontage* EntryMontage = PlayerEntryMontage;
	FName EntrySection = PlayerEntrySectionName;
	float EntryPlayRate = 1.0f;
	if (const FCombatAnimRow* AnimRow = GetCombatAnimRow(true))
	{
		if (!AnimRow->Entry.Montage.IsNull())
		{
			EntryMontage = AnimRow->Entry.Montage.LoadSynchronous();
			EntrySection = AnimRow->Entry.SectionName;
			EntryPlayRate = AnimRow->Entry.PlayRate;
		}
	}
	if (EntryMontage && PlayerRole.IsValid())
	{
		const float PlayLength = EntryMontage->GetPlayLength();
		PlayerRole->PlayAnimMontage(EntryMontage, EntryPlayRate, EntrySection);
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
	else
	{
		const int32 MaxStacks = GetMaxChargeStacks(false);
		TArray<EBattleAction> Options;
		if (bExtraTurn)
		{
			// 额外回合：只能 出蓝刀 或 继续蓄力
			if (EnemyChargeStacks > 0) Options.Add(EBattleAction::BlueAttack);
			if (EnemyChargeStacks < MaxStacks) Options.Add(EBattleAction::Charge);
			EnemyChosenAction = Options.Num() > 0
				? Options[FMath::RandRange(0, Options.Num() - 1)]
				: EBattleAction::RedDefense;
		}
		else
		{
			// 蓝攻需 ≥1 层蓄力；蓄力满上限后不能再蓄力
			Options.Add(EBattleAction::RedDefense);
			Options.Add(EBattleAction::WhiteAttack);
			if (EnemyChargeStacks > 0) Options.Add(EBattleAction::BlueAttack);
			if (EnemyChargeStacks < MaxStacks) Options.Add(EBattleAction::Charge);
			EnemyChosenAction = Options[FMath::RandRange(0, Options.Num() - 1)];
		}
	}
}

void UBattleComponent::StartPlayerExtraTurn()
{
	bPlayerChoseAction = false;
	UE_LOG(LogTemp, Log, TEXT("UBattleComponent::StartPlayerExtraTurn - 玩家额外回合触发"));
	SetPhase(EBattlePhase::ActionSelect);
	OnBattleStateChanged.Broadcast();
}

void UBattleComponent::StartEnemyExtraTurn()
{
	UE_LOG(LogTemp, Log, TEXT("UBattleComponent::StartEnemyExtraTurn - 敌人额外回合触发"));
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
			// 同色碰撞：不直接结算双方伤害，仅进入抵挡环节（敌方伤害待格挡/闪避判定）
			R.bClash = true;
			R.ClashType = EClashType::BlueClash;
			R.PlayerDamageTaken = GetEnemyBlueDamage(EnemyChargeStacks);
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
			// 同色碰撞：不直接结算双方伤害，仅进入抵挡环节（敌方伤害待格挡/闪避判定）
			R.bClash = true;
			R.ClashType = EClashType::WhiteClash;
			R.PlayerDamageTaken = GetEnemyWhiteDamage();
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

	// 攻击（白攻）与防御（红防）会清空自身蓄力层数，且不获得蓄力加成（加成只属于蓝攻）
	if (PlayerAction == EBattleAction::RedDefense || PlayerAction == EBattleAction::WhiteAttack)
	{
		PlayerChargeStacks = 0;
	}
	if (EnemyAction == EBattleAction::RedDefense || EnemyAction == EBattleAction::WhiteAttack)
	{
		EnemyChargeStacks = 0;
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

		// 同色碰撞：不直接结算伤害，仅进入抵挡环节（敌方伤害由格挡/闪避判定）
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

	// 伤害延迟到动画命中通知：注册与播放由 PlayResolutionAnimations 负责
	if (Phase != EBattlePhase::Ended)
	{
		PlayResolutionAnimations(Resolution);
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

	// 动画：敌方碰撞攻击 + 玩家进入准备姿态（Idle 切换）
	UAnimMontage* TelegraphMontage = nullptr;
	if (const FCombatAnimRow* Row = GetCombatAnimRow(false))
	{
		FAnimRef Telegraph = (ClashType == EClashType::BlueClash)
			? Row->ClashAttackBlue
			: Row->ClashAttackWhite;
		// 随机 Section：配置了竖线分隔列表时随机选一段播放
		const FString& Sections = (ClashType == EClashType::BlueClash)
			? Row->ClashAttackBlueSections
			: Row->ClashAttackWhiteSections;
		if (!Sections.IsEmpty())
		{
			TArray<FString> SectionParts;
			Sections.ParseIntoArray(SectionParts, TEXT("|"), true);
			if (SectionParts.Num() > 0)
			{
				Telegraph.SectionName = FName(*SectionParts[FMath::RandRange(0, SectionParts.Num() - 1)]);
				UE_LOG(LogTemp, Log, TEXT("UBattleComponent::StartClash - 随机碰撞攻击 Section: %s"), *Telegraph.SectionName.ToString());
			}
		}
		TelegraphMontage = Telegraph.Montage.IsNull() ? nullptr : Telegraph.Montage.LoadSynchronous();
		PlayCombatAnim(BossEnemy.Get(), Telegraph);
	}
	SetPlayerClashReady(true);

	// 命中时间锚点：优先取碰撞攻击 Montage 的 ClashAttackHit 通知时间，缺失回落 ClashAttackTime
	const float NotifyHitTime = GetNotifyTime(TelegraphMontage, FName(TEXT("ClashAttackHit")));
	ClashHitTime = NotifyHitTime > 0.0f ? NotifyHitTime : ClashAttackTime;
	LastClashInputTime = -1.0f;

	// 注册敌方碰撞攻击待命中事件（金额由 ResolveClash 决定；通知/回落二选一消费）
	RegisterPendingHit(false, FName(TEXT("ClashAttackHit")), PlayerRole.Get(), 0.0f, BossEnemy.Get(),
		FAnimRef(), nullptr, FAnimRef(), FAnimRef(), TelegraphMontage, false);

	const float BlockWindow = GetBlockWindow();
	const float DodgeWindow = GetDodgeWindow();
	const float OpenDelay = FMath::Max(0.0f, ClashHitTime - FMath::Max(BlockWindow, DodgeWindow));

	GetWorld()->GetTimerManager().SetTimer(ClashOpenTimer, this, &UBattleComponent::OpenClashWindow, OpenDelay, false);
	GetWorld()->GetTimerManager().SetTimer(ClashImpactTimer, this, &UBattleComponent::OnClashImpact, ClashHitTime, false);

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

	// 碰撞攻击无 ClashAttackHit 通知：由影响计时器回落结算（若通知已消费则跳过）
	if (EnemyPendingHit.bActive && EnemyPendingHit.EventName == FName(TEXT("ClashAttackHit")))
	{
		ApplyPendingHitNow(false);
	}
}

void UBattleComponent::ResolveClash(EClashResult Result)
{
	if (bClashResolved)
	{
		return;
	}
	bClashResolved = true;
	ClearClashTimers();
	SetPlayerClashReady(false);

	float Incoming = PendingIncomingDamage;

	switch (Result)
	{
	case EClashResult::BlockSuccess:
		Incoming = 0.0f;
		// 被格挡反馈：敌方立即混入 BlockedReaction + 停帧
		if (const FCombatAnimRow* EnemyRow = GetCombatAnimRow(false))
		{
			PlayCombatAnim(BossEnemy.Get(), EnemyRow->BlockedReaction);
		}
		{
			const FCombatParamsRow Defaults;
			const FCombatParamsRow* Params = GetCombatSubsystem() ? GetCombatSubsystem()->GetCombatParams() : nullptr;
			const FCombatParamsRow& P = Params ? *Params : Defaults;
			StartHitStop(P.HitStopDuration);
		}
		if (const FCombatAnimRow* Row = GetCombatAnimRow(true))
		{
			RegisterPendingHit(true, FName(TEXT("GoldCounterHit")), BossEnemy.Get(), GetPlayerGoldDamage(),
				PlayerRole.Get(), Row->Hurt, nullptr, FAnimRef(), FAnimRef(),
				Row->GoldCounter.Montage.IsNull() ? nullptr : Row->GoldCounter.Montage.LoadSynchronous(), false);
		}
		PlayBlockSuccessChain();
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
		{
			const FCombatParamsRow Defaults;
			const FCombatParamsRow* Params = GetCombatSubsystem() ? GetCombatSubsystem()->GetCombatParams() : nullptr;
			const FCombatParamsRow& P = Params ? *Params : Defaults;
			StartHitStop(P.HitStopDuration);
		}
		if (const FCombatAnimRow* Row = GetCombatAnimRow(true))
		{
			PlayCombatAnim(PlayerRole.Get(), Row->DodgeSuccess);
		}
		break;
	case EClashResult::DodgeFail:
	{
		UCombatFormulaSubsystem* Subsystem = GetCombatSubsystem();
		const FCombatParamsRow Defaults;
		const FCombatParamsRow* Params = Subsystem ? Subsystem->GetCombatParams() : nullptr;
		const FCombatParamsRow& P = Params ? *Params : Defaults;
		Incoming *= P.DodgeFailDamageScale;
		PlayClashFailReaction(EClashResult::DodgeFail);
		break;
	}
	case EClashResult::BlockFail:
	default:
		// 全额伤害
		PlayClashFailReaction(EClashResult::BlockFail);
		break;
	}

	// 伤害延迟到 ClashAttackHit 通知/回落触发；金额写回待命中事件
	if (EnemyPendingHit.bActive && EnemyPendingHit.EventName == FName(TEXT("ClashAttackHit")))
	{
		EnemyPendingHit.Amount = Incoming;
	}
	else if (Incoming > 0.0f)
	{
		// 通知已消费（或槽已被覆盖）：直接结算，保证伤害不丢
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
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float Cooldown = GetClashInputCooldown();
	if (LastClashInputTime >= 0.0f && Now - LastClashInputTime < Cooldown)
	{
		UE_LOG(LogTemp, Log, TEXT("UBattleComponent::OnBlockPressed - 输入冷却中，忽略"));
		return;
	}
	LastClashInputTime = Now;

	if (Now >= ClashHitTime - GetBlockWindow())
	{
		ResolveClash(EClashResult::BlockSuccess);
		return;
	}
	// 窗口外提前按下：先记录失败结果，窗口内再按可覆盖
	PendingClashResult = EClashResult::BlockFail;
}

void UBattleComponent::OnDodgePressed()
{
	if (Phase != EBattlePhase::Clash || bClashResolved)
	{
		return;
	}
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float Cooldown = GetClashInputCooldown();
	if (LastClashInputTime >= 0.0f && Now - LastClashInputTime < Cooldown)
	{
		UE_LOG(LogTemp, Log, TEXT("UBattleComponent::OnDodgePressed - 输入冷却中，忽略"));
		return;
	}
	LastClashInputTime = Now;

	if (Now >= ClashHitTime - GetDodgeWindow())
	{
		ResolveClash(EClashResult::DodgeSuccess);
		return;
	}
	// 窗口外提前按下：先记录失败结果，窗口内再按可覆盖
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

void UBattleComponent::ShowResultHUD(const FText& Text)
{
	if (ResultHUD.IsValid())
	{
		ResultHUD->ShowResult(Text);
		return;
	}
	if (!BattleResultHUDClass || !GetWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::ShowResultHUD - 未配置 WBP_BattleResult"));
		return;
	}

	ResultHUD = CreateWidget<UBattleResultHUDWidget>(GetWorld(), BattleResultHUDClass);
	if (!ResultHUD.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::ShowResultHUD - 创建 WBP_BattleResult 失败"));
		return;
	}

	ResultHUD->AddToViewport(20);
	ResultHUD->ShowResult(Text);
}

void UBattleComponent::HideResultHUD()
{
	if (ResultHUD.IsValid())
	{
		ResultHUD->RemoveFromParent();
		ResultHUD.Reset();
	}
}

const FCombatAnimRow* UBattleComponent::GetCombatAnimRow(bool bPlayer) const
{
	UCombatFormulaSubsystem* Subsystem = GetCombatSubsystem();
	if (!Subsystem)
	{
		return nullptr;
	}
	const FName EntityID = bPlayer
		? (PlayerRole.IsValid() ? PlayerRole->CharacterID : NAME_None)
		: (BossEnemy.IsValid() ? BossEnemy->EnemyID : NAME_None);
	return EntityID.IsNone() ? nullptr : Subsystem->GetCombatAnimRow(EntityID);
}

void UBattleComponent::PlayCombatAnim(ABaseCharacter* Character, const FAnimRef& AnimRef)
{
	if (!Character || AnimRef.Montage.IsNull())
	{
		return;
	}
	UAnimMontage* Montage = AnimRef.Montage.LoadSynchronous();
	if (!Montage)
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::PlayCombatAnim - 无法加载 Montage: %s"), *AnimRef.Montage.ToString());
		return;
	}
	if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
	{
		AnimInstance->OnMontageEnded.RemoveDynamic(this, &UBattleComponent::OnActionMontageEnded);
		AnimInstance->OnMontageEnded.AddDynamic(this, &UBattleComponent::OnActionMontageEnded);
	}
	Character->PlayAnimMontage(Montage, AnimRef.PlayRate, AnimRef.SectionName);
	UE_LOG(LogTemp, Log, TEXT("UBattleComponent::PlayCombatAnim - %s 播放 %s"), *GetNameSafe(Character), *Montage->GetName());
}

void UBattleComponent::PlayActionAnim(bool bPlayer, EBattleAction Action)
{
	const FCombatAnimRow* Row = GetCombatAnimRow(bPlayer);
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::PlayActionAnim - %s 未找到 DT_CombatAnimConfig 行（检查 EntityID 与表行名）"),
			bPlayer ? TEXT("玩家") : TEXT("敌人"));
		return;
	}
	const FAnimRef* Ref = nullptr;
	switch (Action)
	{
	case EBattleAction::RedDefense: Ref = &Row->RedDefense; break;
	case EBattleAction::BlueAttack: Ref = &Row->BlueAttack; break;
	case EBattleAction::WhiteAttack: Ref = &Row->WhiteAttack; break;
	case EBattleAction::Charge: Ref = &Row->Charge; break;
	default: return;
	}
	if (Ref->Montage.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::PlayActionAnim - %s 行动动画未配置（Action=%d）"),
			bPlayer ? TEXT("玩家") : TEXT("敌人"), (int32)Action);
		return;
	}
	ABaseCharacter* Target = bPlayer ? Cast<ABaseCharacter>(PlayerRole.Get()) : Cast<ABaseCharacter>(BossEnemy.Get());
	PlayCombatAnim(Target, *Ref);
}

const FAnimRef* UBattleComponent::GetActionRef(const FCombatAnimRow& Row, EBattleAction Action) const
{
	switch (Action)
	{
	case EBattleAction::RedDefense: return &Row.RedDefense;
	case EBattleAction::BlueAttack: return &Row.BlueAttack;
	case EBattleAction::WhiteAttack: return &Row.WhiteAttack;
	case EBattleAction::Charge: return &Row.Charge;
	default: return nullptr;
	}
}

float UBattleComponent::GetNotifyTime(UAnimMontage* Montage, FName EventName) const
{
	if (!Montage)
	{
		return -1.0f;
	}
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		if (!Event.Notify)
		{
			continue;
		}
		if (const UAnimNotify_CombatDamage* DamageNotify = Cast<UAnimNotify_CombatDamage>(Event.Notify))
		{
			if (DamageNotify->EventName == EventName)
			{
				return Event.GetTime();
			}
		}
		if (const UAnimNotify_CombatMarker* Marker = Cast<UAnimNotify_CombatMarker>(Event.Notify))
		{
			if (Marker->MarkerName == EventName)
			{
				return Event.GetTime();
			}
		}
	}
	return -1.0f;
}

void UBattleComponent::RegisterPendingHit(bool bPlayerAttacker, FName EventName, ABaseCharacter* Target, float Amount,
	AActor* Causer, const FAnimRef& HitReaction, ABaseCharacter* Defender,
	const FAnimRef& DefenderReaction, const FAnimRef& DefenderFollowUp, UAnimMontage* FallbackMontage,
	bool bDefenderBlocked)
{
	FPendingHitEvent& Hit = bPlayerAttacker ? PlayerPendingHit : EnemyPendingHit;
	ClearPendingHit(bPlayerAttacker);
	Hit.bActive = true;
	Hit.EventName = EventName;
	Hit.Target = Target;
	Hit.Amount = Amount;
	Hit.Causer = Causer;
	Hit.HitReaction = HitReaction;
	Hit.Defender = Defender;
	Hit.DefenderReaction = DefenderReaction;
	Hit.DefenderFollowUp = DefenderFollowUp;
	Hit.FallbackMontage = FallbackMontage;
	Hit.bDefenderBlocked = bDefenderBlocked;
}

void UBattleComponent::ClearPendingHit(bool bPlayerAttacker)
{
	FPendingHitEvent& Hit = bPlayerAttacker ? PlayerPendingHit : EnemyPendingHit;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(bPlayerAttacker ? PlayerDefenderTimer : EnemyDefenderTimer);
	}
	Hit = FPendingHitEvent();
}

void UBattleComponent::ClearPendingHits()
{
	ClearPendingHit(true);
	ClearPendingHit(false);
}

void UBattleComponent::ApplyPendingHitNow(bool bPlayerAttacker)
{
	FPendingHitEvent& Hit = bPlayerAttacker ? PlayerPendingHit : EnemyPendingHit;
	if (!Hit.bActive)
	{
		return;
	}
	ABaseCharacter* Target = Hit.Target;
	const FAnimRef HitReaction = Hit.HitReaction;
	const float Amount = Hit.Amount;
	AActor* Causer = Hit.Causer;
	const bool bBlocked = Hit.bDefenderBlocked;
	ClearPendingHit(bPlayerAttacker);

	if (Target && Amount > 0.0f)
	{
		ApplyDamageTo(Target, Amount, Causer);
		if (!Target->IsDead() && !HitReaction.Montage.IsNull())
		{
			PlayCombatAnim(Target, HitReaction);
		}
	}

	// 被格挡：攻击者立即混入 BlockedReaction + 停帧
	if (bBlocked && Causer)
	{
		if (ABaseCharacter* Attacker = Cast<ABaseCharacter>(Causer))
		{
			if (!Attacker->IsDead())
			{
				const bool bAttackerPlayer = (Attacker == PlayerRole.Get());
				if (const FCombatAnimRow* Row = GetCombatAnimRow(bAttackerPlayer))
				{
					PlayCombatAnim(Attacker, Row->BlockedReaction);
				}
			}
		}
		const FCombatParamsRow Defaults;
		const FCombatParamsRow* Params = GetCombatSubsystem() ? GetCombatSubsystem()->GetCombatParams() : nullptr;
		const FCombatParamsRow& P = Params ? *Params : Defaults;
		StartHitStop(P.HitStopDuration);
	}
}

void UBattleComponent::OnHitNotify(ABaseCharacter* Attacker, FName EventName)
{
	if (!Attacker)
	{
		return;
	}
	const bool bPlayerAttacker = (Attacker == PlayerRole.Get());
	FPendingHitEvent& Hit = bPlayerAttacker ? PlayerPendingHit : EnemyPendingHit;
	if (!Hit.bActive || Hit.EventName != EventName)
	{
		return;
	}
	ApplyPendingHitNow(bPlayerAttacker);
}

void UBattleComponent::StartHitStop(float Duration)
{
	UWorld* World = GetWorld();
	if (Duration <= 0.0f || !World)
	{
		return;
	}
	TArray<UAnimInstance*> PausedInstances;
	TArray<UAnimMontage*> PausedMontages;
	if (PlayerRole.IsValid())
	{
		if (UAnimInstance* AI = PlayerRole->GetMesh()->GetAnimInstance())
		{
			if (UAnimMontage* Active = AI->GetCurrentActiveMontage())
			{
				AI->Montage_Pause(Active);
				PausedInstances.Add(AI);
				PausedMontages.Add(Active);
			}
		}
	}
	if (BossEnemy.IsValid())
	{
		if (UAnimInstance* AI = BossEnemy->GetMesh()->GetAnimInstance())
		{
			if (UAnimMontage* Active = AI->GetCurrentActiveMontage())
			{
				AI->Montage_Pause(Active);
				PausedInstances.Add(AI);
				PausedMontages.Add(Active);
			}
		}
	}
	World->GetTimerManager().SetTimer(HitStopTimer, [PausedInstances, PausedMontages]()
	{
		for (int32 i = 0; i < PausedInstances.Num() && i < PausedMontages.Num(); ++i)
		{
			if (PausedInstances[i] && PausedMontages[i])
			{
				PausedInstances[i]->Montage_Resume(PausedMontages[i]);
			}
		}
	}, Duration, false);
}

void UBattleComponent::PlayResolutionAnimations(const FTurnResolution& Resolution)
{
	if (!PlayerRole.IsValid() || !BossEnemy.IsValid())
	{
		return;
	}

	const EBattleAction PlayerAction = PlayerLastAction;
	const EBattleAction EnemyAction = EnemyChosenAction;

	// 蓝 vs 红（含红防反击/2 层正面承受）：专用注册路径
	if (PlayerAction == EBattleAction::RedDefense && EnemyAction == EBattleAction::BlueAttack)
	{
		RegisterBlueVsRedHit(false, Resolution.PlayerDamageTaken, Resolution.PlayerDamageTaken <= 0.0f);
		return;
	}
	if (EnemyAction == EBattleAction::RedDefense && PlayerAction == EBattleAction::BlueAttack)
	{
		RegisterBlueVsRedHit(true, Resolution.PlayerDamageTaken, Resolution.EnemyDamageTaken <= 0.0f);
		return;
	}

	// 普通回合：按攻击方注册命中事件并播行动动画（受击/蓄力反应在命中帧播放）
	RegisterSideHit(true, Resolution);
	RegisterSideHit(false, Resolution);
}

void UBattleComponent::RegisterSideHit(bool bPlayerAttacker, const FTurnResolution& Resolution)
{
	const EBattleAction AttackerAction = bPlayerAttacker ? PlayerLastAction : EnemyChosenAction;
	const bool bTargetInterrupted = bPlayerAttacker ? Resolution.bEnemyChargeInterrupted : Resolution.bPlayerChargeInterrupted;
	const bool bTargetResist = bPlayerAttacker ? Resolution.bEnemyExtraTurn : Resolution.bPlayerExtraTurn;
	const float Amount = bPlayerAttacker ? Resolution.EnemyDamageTaken : Resolution.PlayerDamageTaken;

	ABaseCharacter* Attacker = bPlayerAttacker ? Cast<ABaseCharacter>(PlayerRole.Get()) : Cast<ABaseCharacter>(BossEnemy.Get());
	ABaseCharacter* Target = bPlayerAttacker ? Cast<ABaseCharacter>(BossEnemy.Get()) : Cast<ABaseCharacter>(PlayerRole.Get());
	const FCombatAnimRow* AttackerRow = GetCombatAnimRow(bPlayerAttacker);
	const FCombatAnimRow* TargetRow = GetCombatAnimRow(!bPlayerAttacker);
	if (!Attacker || !Target || !AttackerRow || !TargetRow)
	{
		return;
	}

	FName EventName = NAME_None;
	switch (AttackerAction)
	{
	case EBattleAction::BlueAttack: EventName = FName(TEXT("BlueAttackHit")); break;
	case EBattleAction::WhiteAttack: EventName = FName(TEXT("WhiteAttackHit")); break;
	default: return;
	}

	const FAnimRef* ActionRef = GetActionRef(*AttackerRow, AttackerAction);
	if (ActionRef)
	{
		PlayCombatAnim(Attacker, *ActionRef);
	}

	FAnimRef HitReaction;
	if (bTargetInterrupted)
	{
		HitReaction = TargetRow->ChargeInterrupted;
	}
	else if (bTargetResist)
	{
		HitReaction = TargetRow->Charge;
	}
	else
	{
		HitReaction = TargetRow->Hurt;
	}

	RegisterPendingHit(bPlayerAttacker, EventName, Target, Amount, Attacker, HitReaction,
		nullptr, FAnimRef(), FAnimRef(),
		ActionRef ? ActionRef->Montage.LoadSynchronous() : nullptr, false);
}

void UBattleComponent::RegisterBlueVsRedHit(bool bAttackerPlayer, float IncomingAmount, bool bCounterSucceeds)
{
	ABaseCharacter* Attacker = bAttackerPlayer ? Cast<ABaseCharacter>(PlayerRole.Get()) : Cast<ABaseCharacter>(BossEnemy.Get());
	ABaseCharacter* Defender = bAttackerPlayer ? Cast<ABaseCharacter>(BossEnemy.Get()) : Cast<ABaseCharacter>(PlayerRole.Get());
	const FCombatAnimRow* AttackerRow = GetCombatAnimRow(bAttackerPlayer);
	const FCombatAnimRow* DefenderRow = GetCombatAnimRow(!bAttackerPlayer);
	if (!Attacker || !Defender || !AttackerRow || !DefenderRow)
	{
		return;
	}

	const FAnimRef* BlueRef = GetActionRef(*AttackerRow, EBattleAction::BlueAttack);
	if (BlueRef)
	{
		PlayCombatAnim(Attacker, *BlueRef);
	}

	FAnimRef DefenderFollowUp = bCounterSucceeds ? DefenderRow->GoldCounter : DefenderRow->Hurt;
	RegisterPendingHit(bAttackerPlayer, FName(TEXT("BlueAttackHit")), Defender, IncomingAmount, Attacker,
		FAnimRef(), Defender, DefenderRow->RedDefense, DefenderFollowUp,
		BlueRef ? BlueRef->Montage.LoadSynchronous() : nullptr, bCounterSucceeds);
	ScheduleDefenderReaction(bAttackerPlayer ? PlayerPendingHit : EnemyPendingHit);

	if (bCounterSucceeds)
	{
		const float GoldAmount = bAttackerPlayer ? GetEnemyGoldDamage() : GetPlayerGoldDamage();
		const FAnimRef& GoldCounterRef = DefenderRow->GoldCounter;
		RegisterPendingHit(!bAttackerPlayer, FName(TEXT("GoldCounterHit")), Attacker, GoldAmount, Defender,
			AttackerRow->Hurt, nullptr, FAnimRef(), FAnimRef(),
			GoldCounterRef.Montage.IsNull() ? nullptr : GoldCounterRef.Montage.LoadSynchronous(), false);
	}
}

void UBattleComponent::ScheduleDefenderReaction(const FPendingHitEvent& Hit)
{
	if (!Hit.Defender || Hit.DefenderReaction.Montage.IsNull())
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const bool bPlayerDefender = (Hit.Defender == PlayerRole.Get());
	FTimerHandle& Timer = bPlayerDefender ? PlayerDefenderTimer : EnemyDefenderTimer;
	World->GetTimerManager().ClearTimer(Timer);

	const float HitTime = GetNotifyTime(Hit.FallbackMontage, FName(TEXT("BlueAttackHit")));
	const float GuardReadyTime = GetNotifyTime(Hit.DefenderReaction.Montage.LoadSynchronous(), FName(TEXT("GuardReady")));
	float Delay = 0.0f;
	if (HitTime > 0.0f && GuardReadyTime >= 0.0f)
	{
		Delay = FMath::Max(0.0f, HitTime - GuardReadyTime);
	}
	else if (HitTime > 0.0f)
	{
		const FCombatParamsRow Defaults;
		const FCombatParamsRow* Params = GetCombatSubsystem() ? GetCombatSubsystem()->GetCombatParams() : nullptr;
		const FCombatParamsRow& P = Params ? *Params : Defaults;
		Delay = FMath::Max(0.0f, HitTime - P.RedDefenseLeadTime);
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::ScheduleDefenderReaction - 红防无 GuardReady 标记，使用 RedDefenseLeadTime 回落"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::ScheduleDefenderReaction - 蓝攻无 BlueAttackHit 通知，红防立即启动"));
	}

	World->GetTimerManager().SetTimer(Timer, [this, Defender = Hit.Defender,
		Reaction = Hit.DefenderReaction, FollowUp = Hit.DefenderFollowUp]()
	{
		if (Phase != EBattlePhase::Ended)
		{
			PlayAnimThenReaction(Defender, Reaction, FollowUp);
		}
	}, Delay, false);
}

void UBattleComponent::SetPlayerClashReady(bool bReady)
{
	if (!PlayerRole.IsValid())
	{
		return;
	}
	if (UAnimInstance* AnimInstance = PlayerRole->GetMesh()->GetAnimInstance())
	{
		if (UBaseCharacterAnimInstance* BaseAnim = Cast<UBaseCharacterAnimInstance>(AnimInstance))
		{
			BaseAnim->SetClashReady(bReady);
		}
	}
}

void UBattleComponent::PlayBlockSuccessChain()
{
	const FCombatAnimRow* Row = GetCombatAnimRow(true);
	if (!Row)
	{
		return;
	}
	PlayAnimThenReaction(PlayerRole.Get(), Row->BlockSuccess, Row->GoldCounter);
}

void UBattleComponent::PlayClashFailReaction(EClashResult Result)
{
	const FCombatAnimRow* Row = GetCombatAnimRow(true);
	if (!Row)
	{
		return;
	}
	const FAnimRef& Ref = (Result == EClashResult::DodgeFail) ? Row->DodgeFail : Row->BlockFail;
	PlayCombatAnim(PlayerRole.Get(), Ref.Montage.IsNull() ? Row->Hurt : Ref);
}

void UBattleComponent::PlayAnimThenReaction(ABaseCharacter* Character, const FAnimRef& ActionRef, const FAnimRef& ReactionRef)
{
	if (!Character)
	{
		return;
	}
	if (ActionRef.Montage.IsNull())
	{
		PlayCombatAnim(Character, ReactionRef);
		return;
	}
	UAnimMontage* ActionMontage = ActionRef.Montage.LoadSynchronous();
	if (!ActionMontage)
	{
		PlayCombatAnim(Character, ReactionRef);
		return;
	}

	const bool bPlayerSide = (Character == PlayerRole.Get());
	bool* bPending = bPlayerSide ? &bPlayerReactionPending : &bEnemyReactionPending;
	UAnimMontage** PendingMontage = bPlayerSide ? &PlayerPendingActionMontage : &EnemyPendingActionMontage;
	FAnimRef* PendingRef = bPlayerSide ? &PlayerPendingReactionRef : &EnemyPendingReactionRef;

	if (*bPending)
	{
		// 该侧前一个动作尚未播完：先补播其反应，再开始新动作
		const FAnimRef OldReaction = *PendingRef;
		ClearPendingReactionSide(bPlayerSide);
		PlayCombatAnim(Character, OldReaction);
	}

	*bPending = true;
	*PendingMontage = ActionMontage;
	*PendingRef = ReactionRef;
	if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
	{
		AnimInstance->OnMontageEnded.AddDynamic(this, &UBattleComponent::OnActionMontageEnded);
	}
	PlayCombatAnim(Character, ActionRef);
}

void UBattleComponent::ClearPendingReactionSide(bool bPlayer)
{
	ABaseCharacter* Character = bPlayer ? Cast<ABaseCharacter>(PlayerRole.Get()) : Cast<ABaseCharacter>(BossEnemy.Get());
	if (bPlayer)
	{
		if (bPlayerReactionPending && Character)
		{
			if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
			{
				AnimInstance->OnMontageEnded.RemoveDynamic(this, &UBattleComponent::OnActionMontageEnded);
			}
		}
		bPlayerReactionPending = false;
		PlayerPendingActionMontage = nullptr;
		PlayerPendingReactionRef = FAnimRef();
	}
	else
	{
		if (bEnemyReactionPending && Character)
		{
			if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
			{
				AnimInstance->OnMontageEnded.RemoveDynamic(this, &UBattleComponent::OnActionMontageEnded);
			}
		}
		bEnemyReactionPending = false;
		EnemyPendingActionMontage = nullptr;
		EnemyPendingReactionRef = FAnimRef();
	}
}

void UBattleComponent::ClearPendingReactions()
{
	ClearPendingReactionSide(true);
	ClearPendingReactionSide(false);
}

void UBattleComponent::OnActionMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bPlayerReactionPending && Montage == PlayerPendingActionMontage)
	{
		const FAnimRef ReactionRef = PlayerPendingReactionRef;
		ClearPendingReactionSide(true);
		if (PlayerRole.IsValid() && Phase != EBattlePhase::Ended)
		{
			PlayCombatAnim(PlayerRole.Get(), ReactionRef);
		}
		return;
	}
	if (bEnemyReactionPending && Montage == EnemyPendingActionMontage)
	{
		const FAnimRef ReactionRef = EnemyPendingReactionRef;
		ClearPendingReactionSide(false);
		if (BossEnemy.IsValid() && Phase != EBattlePhase::Ended)
		{
			PlayCombatAnim(BossEnemy.Get(), ReactionRef);
		}
		return;
	}

	// 待命中事件回落：动作蒙太奇播完（含被打断）仍未触发通知 → 结算，保证伤害不丢
	if (PlayerPendingHit.bActive && PlayerPendingHit.FallbackMontage == Montage)
	{
		ApplyPendingHitNow(true);
	}
	if (EnemyPendingHit.bActive && EnemyPendingHit.FallbackMontage == Montage)
	{
		ApplyPendingHitNow(false);
	}
}

void UBattleComponent::PlayDeathAnimations(bool bPlayerWon)
{
	if (const FCombatAnimRow* LoserRow = GetCombatAnimRow(!bPlayerWon))
	{
		ABaseCharacter* Loser = bPlayerWon ? Cast<ABaseCharacter>(BossEnemy.Get()) : Cast<ABaseCharacter>(PlayerRole.Get());
		PlayCombatAnim(Loser, LoserRow->Death);
	}
	if (const FCombatAnimRow* WinnerRow = GetCombatAnimRow(bPlayerWon))
	{
		ABaseCharacter* Winner = bPlayerWon ? Cast<ABaseCharacter>(PlayerRole.Get()) : Cast<ABaseCharacter>(BossEnemy.Get());
		PlayCombatAnim(Winner, WinnerRow->Victory);
	}
}

void UBattleComponent::SheathePlayerWeapon()
{
	if (PlayerRole.IsValid())
	{
		// 停止全部 Montage（入场/动作/碰撞残留），避免通知回调再次拔刀或连播反击
		ClearPendingReactions();
		ClearPendingHits();
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(HitStopTimer);
		}
		if (UAnimInstance* AnimInstance = PlayerRole->GetMesh()->GetAnimInstance())
		{
			AnimInstance->StopAllMontages(0.0f);
		}
		SetPlayerClashReady(false);

		// 武器收回背部 socket；AttachWeaponToSocket 会把 IsWeaponDrawn 置回 false，
		// 动画实例逐帧镜像后玩家自动回到未拔刀 Idle
		if (PlayerRole->WeaponVisualComponent)
		{
			PlayerRole->WeaponVisualComponent->AttachWeaponToSocket(
				PlayerRole->WeaponVisualComponent->BackSocketName);
		}
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
	PlayDeathAnimations(bPlayerWon);
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(EntryDelayTimer);
	}
	UnbindCombatInput();
	RemoveCombatMapping();
	ShowResultHUD(bPlayerWon ? FText::FromString(TEXT("战斗胜利")) : FText::FromString(TEXT("战斗失败")));
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
	HideResultHUD();
	SheathePlayerWeapon();
	HideHUD();
	RestoreExplorationState();
	UnlockPlayer();
	SetPhase(EBattlePhase::Idle);
	OnBattleStateChanged.Broadcast();
}

void UBattleComponent::HandleDefeatRestart()
{
	HideResultHUD();
	SheathePlayerWeapon();
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
	ClearPendingHits();
	LastClashInputTime = -1.0f;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(HitStopTimer);
	}
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

float UBattleComponent::GetClashInputCooldown() const
{
	const FCombatParamsRow Defaults;
	const FCombatParamsRow* Params = GetCombatSubsystem() ? GetCombatSubsystem()->GetCombatParams() : nullptr;
	const FCombatParamsRow& P = Params ? *Params : Defaults;
	return P.ClashInputCooldown;
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
