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
#include "GameFramework/WorldSettings.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

UBattleComponent::UBattleComponent()
{
	// 停帧需要组件 Tick 用 DeltaTime 累积固定时长；平时不启动
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

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

	// 绑定场景中所有 Tag=Boss 敌人的入场动画：谁播完就进谁的战斗
	// （同一测试关卡可同时存在 Satan 与序章教学怪；仅绑定第一个会导致教学怪序列播完无人监听）
	const TArray<AEnemy*> BossEnemies = FindBossEnemies();
	for (AEnemy* Enemy : BossEnemies)
	{
		if (UBossIntroComponent* Intro = Enemy ? Enemy->FindComponentByClass<UBossIntroComponent>() : nullptr)
		{
			Intro->OnIntroFinished.AddDynamic(this, &UBattleComponent::HandleIntroFinished);
		}
	}
	BossEnemy = BossEnemies.Num() > 0 ? BossEnemies[0] : nullptr;

	if (BossEnemies.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::BeginPlay - 场景中未找到 Tag=Boss 的 AEnemy"));
	}
	else if (BossEnemies.Num() > 1)
	{
		UE_LOG(LogTemp, Log, TEXT("UBattleComponent::BeginPlay - 场景中存在 %d 个 Tag=Boss 敌人，入场结束后按完成者开战"), BossEnemies.Num());
	}
}

void UBattleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearClashTimers();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(EndDelayTimer);
		GetWorld()->GetTimerManager().ClearTimer(EntryDelayTimer);
		GetWorld()->GetTimerManager().ClearTimer(ChargePoseTimer);
		GetWorld()->GetTimerManager().ClearTimer(BlueAttackDelayTimer);
	}
	EndHitStop();
	UnbindCombatInput();
	RemoveCombatMapping();
	if (ResultHUD.IsValid())
	{
		ResultHUD->RemoveFromParent();
		ResultHUD.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void UBattleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bHitStopActive)
	{
		return;
	}
	HitStopRemaining -= DeltaTime;
	if (HitStopRemaining <= 0.0f)
	{
		EndHitStop();
	}
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

	// 序章教学战：主角突袭魔法师 → 玩家先制（与敌方先制效果一致：先制方开局 1 层蓄力，无其它效果）
	if (IsTutorialBattle())
	{
		bEnemyFirstStrike = false;
		UE_LOG(LogTemp, Log, TEXT("UBattleComponent::StartBattle - 序章教学战：玩家先制（玩家 1 层 / 敌方 0 层）"));
	}
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
		GetWorld()->GetTimerManager().ClearTimer(ChargePoseTimer);
		GetWorld()->GetTimerManager().ClearTimer(BlueAttackDelayTimer);
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

bool UBattleComponent::PlayerChooseAction(EBattleAction Action)
{
	if (Phase != EBattlePhase::ActionSelect || bPlayerChoseAction)
	{
		return false;
	}
	if (Action == EBattleAction::Skill)
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::PlayerChooseAction - 技能系统未实装"));
		return false;
	}
	// 教学点锁定：条件教学点回合只能选择指定行动（HUD 同步禁用其它按钮）
	if (const EBattleAction LockedAction = GetTutorialLockedPlayerAction(); LockedAction != EBattleAction::None && Action != LockedAction)
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::PlayerChooseAction - 教学点锁定：本回合只能选择行动 %d，拒绝 %d"),
			(int32)LockedAction, (int32)Action);
		return false;
	}
	if (Action == EBattleAction::BlueAttack && PlayerChargeStacks <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::PlayerChooseAction - 蓝攻需要至少 1 层蓄力"));
		return false;
	}
	if (Action == EBattleAction::Charge && PlayerChargeStacks >= GetMaxChargeStacks(true))
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::PlayerChooseAction - 蓄力已达上限"));
		return false;
	}
	if (bPlayerExtraTurnPending && Action != EBattleAction::BlueAttack && Action != EBattleAction::Charge)
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::PlayerChooseAction - 额外回合只能蓝攻或蓄力"));
		return false;
	}

	// 教学战：玩家第一次使用白攻 → 敌方必定同步白攻，触发白白碰撞教学
	// （教学锁定回合玩家只能蓄力，因此该覆盖只会在自由回合生效）
	if (IsTutorialBattle() && Action == EBattleAction::WhiteAttack && !bTutorialFirstWhiteAttackUsed)
	{
		bTutorialFirstWhiteAttackUsed = true;
		if (EnemyChosenAction != EBattleAction::WhiteAttack)
		{
			EnemyChosenAction = EBattleAction::WhiteAttack;
			UE_LOG(LogTemp, Log, TEXT("UBattleComponent::PlayerChooseAction - 教学第一次白攻：强制敌方同步白攻，触发白白碰撞"));
		}
	}

	bPlayerChoseAction = true;
	PlayerLastAction = Action;
	if (IsTutorialBattle())
	{
		bTutorialInitialHintPending = false;
	}
	OnBattleStateChanged.Broadcast();

	if (bPlayerExtraTurnPending)
	{
		const FTurnResolution Resolution = ResolveExtraTurn(true, Action);
		bPlayerExtraTurnPending = false;
		ApplyResolution(Resolution);
		if (!bClashStarted)
		{
			TryAdvanceTurnIfGateDone();
		}
		return true;
	}

	const FTurnResolution Resolution = ResolveNormalTurn(Action, EnemyChosenAction);
	ApplyResolution(Resolution);
	if (!bClashStarted)
	{
		TryAdvanceTurnIfGateDone();
	}
	return true;
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

bool UBattleComponent::IsTutorialBattle() const
{
	return BossEnemy.IsValid() && BossEnemy->EnemyID == FName(TEXT("apprentice_cave"));
}

FText UBattleComponent::GetTutorialHintText() const
{
	if (!IsTutorialBattle())
	{
		return FText::GetEmpty();
	}
	// 碰撞阶段：对拼操作提示
	if (Phase == EBattlePhase::Clash && !bClashResolved)
	{
		return FText::FromString(TEXT("双方使用相同的攻击触发对拼，需要在敌方攻击将要击中的时候点击E格挡/Shift闪避，格挡成功可以造成反击"));
	}
	if (Phase != EBattlePhase::ActionSelect || bPlayerChoseAction || bPlayerExtraTurnPending)
	{
		return FText::GetEmpty();
	}
	// 进入战斗后的首条总提示（仅一次，玩家首次选择行动后清除）
	if (bTutorialInitialHintPending)
	{
		return FText::FromString(TEXT("触发先制攻击可以在战斗开始时直接获得一层蓄力，蓄力层数最高两层，拥有蓄力层数可以使用蓝色攻击，蓝色攻击克制白色攻击，白色攻击克制红色防御，红色防御克制蓝色攻击"));
	}
	return TutorialActiveHint;
}

EBattleAction UBattleComponent::GetTutorialLockedPlayerAction() const
{
	// 额外回合按标准规则（仅蓝攻/蓄力），教学锁定只作用于两个蓄力教学点回合
	if (!IsTutorialBattle() || bPlayerExtraTurnPending || !bTutorialChargeLockActive)
	{
		return EBattleAction::None;
	}
	// 教学点条件保证玩家蓄力为 0/1 层；兜底：蓄力已达上限时解锁，避免卡死
	if (PlayerChargeStacks >= GetMaxChargeStacks(true))
	{
		return EBattleAction::None;
	}
	return EBattleAction::Charge;
}

bool UBattleComponent::ShouldTriggerTutorialFlee() const
{
	if (!IsTutorialBattle() || !BossEnemy.IsValid() || bTutorialFleeTriggered)
	{
		return false;
	}
	// 两个蓄力教学点都演示过后才允许血量逃跑（引导不被打断）
	const bool bMomentsDone = bTutorialChargeCapTaught && bTutorialChargeResistTaught;
	const float MaxHP = BossEnemy->GetMaxHealth();
	const bool bLowHp = MaxHP > 0.0f && (BossEnemy->CurrentHealth / MaxHP) <= GetRunAwayHPThreshold();
	// 兜底：教学点迟迟未触发（如玩家层数始终不到 0/1）时按回合上限逃跑，避免软锁
	const bool bRoundCap = RoundNumber >= 15;
	return (bMomentsDone && bLowHp) || bRoundCap;
}

void UBattleComponent::UpdateTutorialDirector()
{
	// 教学点 A：玩家蓄力 1 层（首回合除外）→ 敌方红防，锁定蓄力，
	// 演示"蓄力到上限后若敌方红防，自动强化蓝攻破防"
	if (RoundNumber > 1 && !bTutorialChargeCapTaught && PlayerChargeStacks == 1)
	{
		TutorialForcedEnemyAction = EBattleAction::RedDefense;
		bTutorialChargeLockActive = true;
		bTutorialChargeCapTaught = true;
		TutorialActiveHint = FText::FromString(TEXT("使用蓄力使蓄力层数到达上限，若敌方此时使用红防，则会在完成蓄力后自动对敌方使用蓄力攻击"));
		UE_LOG(LogTemp, Log, TEXT("UBattleComponent::UpdateTutorialDirector - 教学点A：蓄力到上限破红防"));
		return;
	}
	// 教学点 B：玩家蓄力 0 层 → 敌方白攻，锁定蓄力，
	// 演示"蓄力到达 1 层时若敌方白攻，减少受伤并获得额外回合"
	if (!bTutorialChargeResistTaught && PlayerChargeStacks == 0)
	{
		TutorialForcedEnemyAction = EBattleAction::WhiteAttack;
		bTutorialChargeLockActive = true;
		bTutorialChargeResistTaught = true;
		TutorialActiveHint = FText::FromString(TEXT("使用蓄力使蓄力到达一层时，若敌方使用白攻，则我方减少受到伤害并获得额外回合"));
		UE_LOG(LogTemp, Log, TEXT("UBattleComponent::UpdateTutorialDirector - 教学点B：蓄力抵抗白攻"));
		return;
	}
	// 非教学点回合：随机 AI、不锁定、无提示
	TutorialForcedEnemyAction = EBattleAction::None;
	bTutorialChargeLockActive = false;
	TutorialActiveHint = FText::GetEmpty();
}

void UBattleComponent::ResetTutorialDirector()
{
	bTutorialInitialHintPending = false;
	bTutorialChargeCapTaught = false;
	bTutorialChargeResistTaught = false;
	bTutorialFirstWhiteAttackUsed = false;
	bTutorialChargeLockActive = false;
	TutorialForcedEnemyAction = EBattleAction::None;
	TutorialActiveHint = FText::GetEmpty();
}

// ==================== 内部流程（Task 6/7/8 补全） ====================

AEnemy* UBattleComponent::FindBossEnemy() const
{
	const TArray<AEnemy*> All = FindBossEnemies();
	return All.Num() > 0 ? All[0] : nullptr;
}

TArray<AEnemy*> UBattleComponent::FindBossEnemies() const
{
	TArray<AEnemy*> Result;
	if (!GetWorld())
	{
		return Result;
	}
	for (TActorIterator<AEnemy> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(FName(TEXT("Boss"))))
		{
			Result.Add(*It);
		}
	}
	return Result;
}

void UBattleComponent::HandleIntroFinished(AActor* FinishedEnemy)
{
	AEnemy* Enemy = Cast<AEnemy>(FinishedEnemy);
	if (!Enemy || Phase != EBattlePhase::Idle)
	{
		return;
	}
	// 同一关卡可能放置多个 Tag=Boss 敌人：以实际播完入场动画的敌人作为本场战斗目标
	BossEnemy = Enemy;
	bBossDefeated = false;
	UE_LOG(LogTemp, Log, TEXT("UBattleComponent::HandleIntroFinished - 敌人 %s 入场动画结束，开始战斗"), *Enemy->EnemyID.ToString());
	StartBattle();
}

void UBattleComponent::EnterBattle()
{
	PositionBattleActors();
	// 先制攻击效果：开场拥有一层蓄力（默认敌方先制 → 敌方 1 层、我方 0 层）
	PlayerChargeStacks = bEnemyFirstStrike ? 0 : 1;
	EnemyChargeStacks = bEnemyFirstStrike ? 1 : 0;
	LockPlayer();
	AddCombatMapping();
	SetupCombatInput();
	ShowHUD();
	if (IsTutorialBattle())
	{
		// 进入战斗后直接给出一次总提示（玩家首次选择行动后清除）
		bTutorialInitialHintPending = true;
	}
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
	// 教学导演：条件教学点优先覆盖敌方行动（蓄力破红防 / 蓄力抵抗白攻）
	if (!bExtraTurn && IsTutorialBattle())
	{
		UpdateTutorialDirector();
		if (TutorialForcedEnemyAction != EBattleAction::None)
		{
			EnemyChosenAction = TutorialForcedEnemyAction;
			return;
		}
	}

	if (!bExtraTurn && bForcedEnemyActionEnabled)
	{
		EnemyChosenAction = ForcedEnemyAction;
		return;
	}

	if (EnemyAI.IsValid())
	{
		EnemyChosenAction = EnemyAI->ChooseAction(RoundNumber, PlayerLastAction, bExtraTurn, EnemyChargeStacks, PlayerChargeStacks);
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
			// 蓝攻需 ≥1 层蓄力；蓄力满上限后不能再蓄力；玩家 0 层蓄力时禁用红防
			if (PlayerChargeStacks > 0)
			{
				Options.Add(EBattleAction::RedDefense);
			}
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
			// 红防克蓝攻 → 金色反击；2 层蓝攻同样不破防（只有蓄力对红防的自动强化蓝攻例外）
			R.EnemyDamageTaken = GetPlayerGoldDamage();
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
			// 蓝攻打断蓄力，全额伤害（攻击方的蓄力奖励由末尾"正常造成伤害 +1 层"统一处理）
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
			// 白攻 vs 蓄力：白攻从不打断蓄力，蓄力方始终保持蓄力姿态、不播受击动画；
			// 0 层蓄力 → 抵抗（0.3 白伤 + 1 层 + 额外回合）；
			// 1 层蓄力 → 全额白伤 + 1 层（到 2 层），无额外回合
			R.bEnemyChargeResisted = true;
			if (EnemyChargeStacks == 0)
			{
				R.EnemyDamageTaken = GetPlayerWhiteDamage() * GetChargeResistScale();
				R.bEnemyExtraTurn = true;
			}
			else
			{
				R.EnemyDamageTaken = GetPlayerWhiteDamage();
			}
			EnemyChargeStacks = FMath::Min(EnemyChargeStacks + 1, MaxStacks);
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
			// 蓄力被蓝攻打断，玩家吃全额蓝攻（敌方的蓄力奖励由末尾统一处理）
			R.PlayerDamageTaken = GetEnemyBlueDamage(EnemyChargeStacks);
			R.bPlayerChargeInterrupted = true;
			PlayerChargeStacks = 0;
			EnemyChargeStacks = 0;
			break;
		case EBattleAction::WhiteAttack:
			// 白攻 vs 蓄力（镜像）：白攻从不打断蓄力，蓄力方始终保持蓄力姿态、不播受击动画；
			// 0 层蓄力 → 抵抗（0.3 白伤 + 1 层 + 额外回合）；
			// 1 层蓄力 → 全额白伤 + 1 层（到 2 层），无额外回合
			R.bPlayerChargeResisted = true;
			if (PlayerChargeStacks == 0)
			{
				R.PlayerDamageTaken = GetEnemyWhiteDamage() * GetChargeResistScale();
				R.bPlayerExtraTurn = true;
			}
			else
			{
				R.PlayerDamageTaken = GetEnemyWhiteDamage();
			}
			PlayerChargeStacks = FMath::Min(PlayerChargeStacks + 1, MaxStacks);
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
	// 统一蓄力奖励：正常对敌方造成伤害 → 自身 +1 层（白攻 vs 蓄力除外，蓄力方抵抗不算攻击方奖励）。
	// 同色碰撞不在此结算：R.PlayerDamageTaken 只是待判定伤害，尚未真正造成，
	// 蓄力奖励完全由 ResolveClash 的格挡/闪避结果决定，避免"敌方造成伤害奖励"与"格挡/闪避失败奖励"叠加。
	if (!R.bClash)
	{
		const bool bPlayerWhiteVsCharge = (PlayerAction == EBattleAction::WhiteAttack && EnemyAction == EBattleAction::Charge);
		const bool bEnemyWhiteVsCharge = (EnemyAction == EBattleAction::WhiteAttack && PlayerAction == EBattleAction::Charge);
		if (R.EnemyDamageTaken > 0.0f && !bPlayerWhiteVsCharge)
		{
			PlayerChargeStacks = FMath::Min(PlayerChargeStacks + 1, MaxStacks);
		}
		if (R.PlayerDamageTaken > 0.0f && !bEnemyWhiteVsCharge)
		{
			EnemyChargeStacks = FMath::Min(EnemyChargeStacks + 1, MaxStacks);
		}
	}

	return R;
}

FTurnResolution UBattleComponent::ResolveExtraTurn(bool bPlayerTurn, EBattleAction Action)
{
	FTurnResolution R;
	// 额外回合只结算获得额外回合的一方，另一方不行动
	R.bPlayerOnlyAction = bPlayerTurn;
	R.bEnemyOnlyAction = !bPlayerTurn;

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

	// 统一蓄力奖励：额外回合蓝刀命中同样视为"正常造成伤害 → 自身 +1 层"
	if (R.EnemyDamageTaken > 0.0f)
	{
		PlayerChargeStacks = FMath::Min(PlayerChargeStacks + 1, GetMaxChargeStacks(true));
	}
	if (R.PlayerDamageTaken > 0.0f)
	{
		EnemyChargeStacks = FMath::Min(EnemyChargeStacks + 1, GetMaxChargeStacks(false));
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

	// 伤害延迟到动画命中通知：注册与播放由 PlayResolutionAnimations 负责；回合推进等完整播出链
	if (Phase != EBattlePhase::Ended)
	{
		bTurnGateOpen = true;
		GatedMontages.Reset();
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

	// 教学敌人锁血 1：必逃设计，避免魔法师在逃跑剧情前被击杀
	if (IsTutorialBattle() && Target == BossEnemy.Get() && Target->IsDead())
	{
		Target->CurrentHealth = 1.0f;
		UE_LOG(LogTemp, Log, TEXT("UBattleComponent::ApplyDamageTo - 教学敌人锁血 1（保证必逃）"));
	}

	if (Target->IsDead())
	{
		FinishBattle(Target == BossEnemy.Get());
	}
}

void UBattleComponent::TryAdvanceTurnIfGateDone()
{
	if (!bTurnGateOpen)
	{
		return;
	}
	// 同色对抗阶段：未结算（bClashResolved=false）时禁止推进，
	// 防止碰撞通知提前清掉待命中后，动画播完把 Phase 推出 Clash，导致命中结算被跳过；
	// ResolveClash 结算完成后（bClashResolved=true）仍保持 Clash 阶段，由本函数正常推进到下一回合
	if (Phase == EBattlePhase::Clash && !bClashResolved)
	{
		return;
	}
	if (GatedMontages.Num() > 0)
	{
		return;
	}
	if (PlayerPendingHit.bActive || EnemyPendingHit.bActive)
	{
		return;
	}
	if (bPlayerReactionPending || bEnemyReactionPending)
	{
		return;
	}

	bTurnGateOpen = false;
	GatedMontages.Reset();
	EndTurnAndAdvance();
}

void UBattleComponent::EndTurnAndAdvance()
{
	if (Phase == EBattlePhase::Ended || bTurnGateOpen)
	{
		return;
	}

	// 教学必逃：脚本播完（T7 碰撞结算后）或血量低于阈值 → 敌方逃跑，不再开新回合
	if (ShouldTriggerTutorialFlee())
	{
		bTutorialFleeTriggered = true;
		FinishBattle(true, true);
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
			TryAdvanceTurnIfGateDone();
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
	LastBlockFailTime = -1.0f;

	SetPhase(EBattlePhase::Clash);
	bTurnGateOpen = true;
	GatedMontages.Reset();

	// 动画：敌方碰撞攻击 + 玩家进入准备姿态（Idle 切换）
	UAnimMontage* TelegraphMontage = nullptr;
	FAnimRef Telegraph;
	if (const FCombatAnimRow* Row = GetCombatAnimRow(false))
	{
		Telegraph = (ClashType == EClashType::BlueClash)
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

	// 命中时间锚点：用真实播放秒数（扣除随机 Section 起点并按 PlayRate 折算）
	ClashStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	float NotifyHitTime = GetNotifyRealTime(TelegraphMontage, FName(TEXT("ClashAttackHit")), Telegraph);
	if (NotifyHitTime < 0.0f && TelegraphMontage)
	{
		// 所选 Section 缺少 ClashAttackHit 通知：用该 Section 的结束时间（真实秒数）作为命中锚点，
		// 避免全局 ClashAttackTime(0.8s) 比攻击动画打击帧更早触发（无输入时"提前受击"）
		const int32 SectionIndex = TelegraphMontage->GetSectionIndex(Telegraph.SectionName);
		if (SectionIndex != INDEX_NONE)
		{
			float SectionStart = 0.0f;
			float SectionEnd = 0.0f;
			TelegraphMontage->GetSectionStartAndEndTime(SectionIndex, SectionStart, SectionEnd);
			NotifyHitTime = (SectionEnd - SectionStart) / FMath::Max(0.01f, Telegraph.PlayRate);
			UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::StartClash - Section %s 无 ClashAttackHit 通知，用 Section 结束时间回落 %.3f"),
				*Telegraph.SectionName.ToString(), NotifyHitTime);
		}
	}
	ClashHitTime = NotifyHitTime >= 0.0f ? NotifyHitTime : ClashAttackTime;
	if (ClashHitTime <= 0.0f)
	{
		// 避免 0 延时：UE SetTimer 对 Rate<=0 直接失效，影响定时器会永不触发
		ClashHitTime = ClashAttackTime;
	}
	const float RawNotifyTime = GetNotifyTime(TelegraphMontage, FName(TEXT("ClashAttackHit")));
	UE_LOG(LogTemp, Log, TEXT("UBattleComponent::StartClash - ClashHitTime=%.3f (Section=%s RawNotify=%.3f SectionStart=%.3f Rate=%.2f)"),
		ClashHitTime, *Telegraph.SectionName.ToString(), RawNotifyTime,
		GetSectionStartTime(TelegraphMontage, Telegraph.SectionName), Telegraph.PlayRate);
	LastClashInputTime = -1.0f;

	// 注册敌方碰撞攻击待命中事件（金额由 ResolveClash 决定；通知/回落二选一消费）
	RegisterPendingHit(false, FName(TEXT("ClashAttackHit")), PlayerRole.Get(), 0.0f, BossEnemy.Get(),
		FAnimRef(), nullptr, FAnimRef(), FAnimRef(), TelegraphMontage, false);

	const float BlockWindow = GetBlockWindow();
	const float DodgeWindow = GetDodgeWindow();
	// 慢放起点 = 两个窗口中较晚打开的那个（min）：慢放一出现，格挡/闪避窗口均已开放，
	// 立即点击按原 Elapsed 窗口判定即为成功，无需修改输入逻辑
	const float OpenDelay = FMath::Max(0.0f, ClashHitTime - FMath::Min(BlockWindow, DodgeWindow));

	GetWorld()->GetTimerManager().SetTimer(ClashOpenTimer, this, &UBattleComponent::OpenClashWindow, OpenDelay, false);
	GetWorld()->GetTimerManager().SetTimer(ClashImpactTimer, this, &UBattleComponent::OnClashImpact, ClashHitTime, false);

	OnBattleStateChanged.Broadcast();
}

void UBattleComponent::OpenClashWindow()
{
	bClashWindowOpen = true;
	ApplyClashTimeDilation();
}

void UBattleComponent::OnClashImpact()
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	UE_LOG(LogTemp, Log, TEXT("UBattleComponent::OnClashImpact - Phase=%d bClashResolved=%d PendingResult=%d Elapsed=%.3f HitTime=%.3f"),
		(int32)Phase, bClashResolved ? 1 : 0, (int32)PendingClashResult, Now - ClashStartTime, ClashHitTime);
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
	UE_LOG(LogTemp, Log, TEXT("UBattleComponent::ResolveClash - Result=%d bClashResolved=%d"),
		(int32)Result, bClashResolved ? 1 : 0);
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
	{
		Incoming = 0.0f;
		// 格挡成功：玩家直接获得 1 层蓄力
		PlayerChargeStacks = FMath::Min(PlayerChargeStacks + 1, GetMaxChargeStacks(true));
		// 被格挡反馈：敌方立即混入 BlockedReaction + 停帧
		const FCombatAnimRow* EnemyRow = GetCombatAnimRow(false);
		const FCombatAnimRow* PlayerRow = GetCombatAnimRow(true);
		if (EnemyRow)
		{
			PlayCombatAnim(BossEnemy.Get(), EnemyRow->BlockedReaction);
		}
		{
			const FCombatParamsRow Defaults;
			const FCombatParamsRow* Params = GetCombatSubsystem() ? GetCombatSubsystem()->GetCombatParams() : nullptr;
			const FCombatParamsRow& P = Params ? *Params : Defaults;
			StartHitStop(P.HitStopDuration);
		}
		if (EnemyRow && PlayerRow)
		{
			RegisterPendingHit(true, FName(TEXT("GoldCounterHit")), BossEnemy.Get(), GetPlayerGoldDamage(),
				PlayerRole.Get(), EnemyRow->Hurt, nullptr, FAnimRef(), FAnimRef(),
				PlayerRow->GoldCounter.Montage.IsNull() ? nullptr : PlayerRow->GoldCounter.Montage.LoadSynchronous(), false);
		}
		PlayBlockSuccessChain();
		break;
	}
	case EClashResult::DodgeSuccess:
		Incoming = 0.0f;
		// 闪避成功：玩家直接获得 1 层蓄力
		PlayerChargeStacks = FMath::Min(PlayerChargeStacks + 1, GetMaxChargeStacks(true));
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
		// 闪避失败：敌方获得 1 层蓄力
		EnemyChargeStacks = FMath::Min(EnemyChargeStacks + 1, GetMaxChargeStacks(false));
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
		// 格挡失败：敌方获得 1 层蓄力
		EnemyChargeStacks = FMath::Min(EnemyChargeStacks + 1, GetMaxChargeStacks(false));
		// 受击：立刻混入受击动画（格挡动画只有红防姿态一种，不使用 BlockFail）
		if (const FCombatAnimRow* Row = GetCombatAnimRow(true))
		{
			PlayCombatAnim(PlayerRole.Get(), Row->Hurt);
		}
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
		TryAdvanceTurnIfGateDone();
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
	RestoreTimeDilation();
}

void UBattleComponent::OnBlockPressed()
{
	if (Phase != EBattlePhase::Clash || bClashResolved)
	{
		return;
	}
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	// 用"相对碰撞开始的经过时间"与命中时间比较，不能拿世界绝对时间与相对时间比
	const float Elapsed = Now - ClashStartTime;

	// 格挡失败锁定：失败后 BlockFailLockoutSeconds 内不可再次格挡（优先于窗口判定）
	if (LastBlockFailTime >= 0.0f && Elapsed - LastBlockFailTime < GetBlockFailLockout())
	{
		UE_LOG(LogTemp, Log, TEXT("UBattleComponent::OnBlockPressed - 格挡失败锁定中（%.1fs 后可再格挡），忽略"),
			GetBlockFailLockout() - (Elapsed - LastBlockFailTime));
		return;
	}

	const float Cooldown = GetClashInputCooldown();
	if (LastClashInputTime >= 0.0f && Now - LastClashInputTime < Cooldown)
	{
		UE_LOG(LogTemp, Log, TEXT("UBattleComponent::OnBlockPressed - 输入冷却中，忽略"));
		return;
	}
	LastClashInputTime = Now;
	// 玩家点击格挡/闪避：立即恢复正常时间流速（慢放结束）
	RestoreTimeDilation();

	UE_LOG(LogTemp, Log, TEXT("UBattleComponent::OnBlockPressed - Elapsed=%.3f HitTime=%.3f Window=%.3f"),
		Elapsed, ClashHitTime, GetBlockWindow());
	if (Elapsed >= ClashHitTime - GetBlockWindow())
	{
		ResolveClash(EClashResult::BlockSuccess);
		return;
	}

	// 窗口外按下 = 格挡失败：立即播放格挡动画（红防姿态），并锁定 1s 内不可再次格挡
	PendingClashResult = EClashResult::BlockFail;
	LastBlockFailTime = Elapsed;
	PlayBlockAnimNow();
	UE_LOG(LogTemp, Log, TEXT("UBattleComponent::OnBlockPressed - 格挡失败，立即播放格挡动画，锁定 %.1fs"), GetBlockFailLockout());
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
	// 玩家点击格挡/闪避：立即恢复正常时间流速（慢放结束）
	RestoreTimeDilation();

	// 用"相对碰撞开始的经过时间"与命中时间比较，不能拿世界绝对时间与相对时间比
	const float Elapsed = Now - ClashStartTime;
	UE_LOG(LogTemp, Log, TEXT("UBattleComponent::OnDodgePressed - Elapsed=%.3f HitTime=%.3f Window=%.3f"),
		Elapsed, ClashHitTime, GetDodgeWindow());
	if (Elapsed >= ClashHitTime - GetDodgeWindow())
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

	// 通用回合闸门：本结算播放的非循环蒙太奇计入播出链（循环姿态如蓄力不阻塞推进）
	if (bTurnGateOpen && !Montage->bLoop)
	{
		GatedMontages.Add(Montage);
	}
}

void UBattleComponent::PlayChargeResistPose(ABaseCharacter* Character, const FAnimRef& AnimRef, bool bBlockGate)
{
	if (!Character || AnimRef.Montage.IsNull())
	{
		return;
	}
	UAnimMontage* Montage = AnimRef.Montage.LoadSynchronous();
	if (!Montage)
	{
		return;
	}

	UAnimInstance* AnimInstance = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	const bool bAlreadyActive = AnimInstance && AnimInstance->Montage_IsActive(Montage);
	if (!bAlreadyActive)
	{
		// 蓄力姿态未在播放（例如动作动画缺失/已自然结束）才重播；已在播放则直接延续，避免白刀命中时重播
		PlayCombatAnim(Character, AnimRef);
		AnimInstance = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	}

	// 有额外回合时：循环蓄力姿态不会自然结束，计入回合闸门并在当前循环播完后主动停止，
	// 由 OnActionMontageEnded 正常推进到额外回合（额外回合动画不得打断蓄力）。
	if (bBlockGate && bTurnGateOpen && Montage->bLoop && AnimInstance && AnimInstance->Montage_IsActive(Montage))
	{
		GatedMontages.Add(Montage);
		float PlayLength = Montage->GetPlayLength() / FMath::Max(0.01f, AnimRef.PlayRate);
		if (bAlreadyActive)
		{
			// 姿态从本回合开始时就在播：只等当前循环播完，不再整段重播
			const float Position = AnimInstance->Montage_GetPosition(Montage);
			PlayLength = FMath::Max(0.01f, (Montage->GetPlayLength() - Position) / FMath::Max(0.01f, AnimRef.PlayRate));
		}
		UE_LOG(LogTemp, Log, TEXT("UBattleComponent::PlayChargeResistPose - %s 蓄力抵抗计入回合闸门，%.2fs 后结束"),
			*GetNameSafe(Character), PlayLength);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ChargePoseTimer);
			World->GetTimerManager().SetTimer(ChargePoseTimer, [this, Character, Montage]()
			{
				if (Phase == EBattlePhase::Ended || !Character)
				{
					return;
				}
				if (UAnimInstance* AI = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
				{
					if (AI->Montage_IsActive(Montage))
					{
						AI->Montage_Stop(Montage->BlendOut.GetBlendTime(), Montage);
					}
				}
			}, PlayLength, false);
		}
	}
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

float UBattleComponent::GetNotifyRealTime(UAnimMontage* Montage, FName EventName, const FAnimRef& AnimRef) const
{
	if (!Montage)
	{
		return -1.0f;
	}
	float SectionStart = 0.0f;
	float SectionEnd = -1.0f;
	const int32 SectionIndex = AnimRef.SectionName.IsNone() ? INDEX_NONE : Montage->GetSectionIndex(AnimRef.SectionName);
	if (SectionIndex != INDEX_NONE)
	{
		Montage->GetSectionStartAndEndTime(SectionIndex, SectionStart, SectionEnd);
	}
	// 指定了 Section 时只取该 Section 时间范围内的通知（多个同名通知按当前 Section 匹配），
	// 否则会把其它 Section 的通知时间误当成本段命中锚点
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		if (!Event.Notify)
		{
			continue;
		}
		bool bMatch = false;
		if (const UAnimNotify_CombatDamage* DamageNotify = Cast<UAnimNotify_CombatDamage>(Event.Notify))
		{
			bMatch = (DamageNotify->EventName == EventName);
		}
		else if (const UAnimNotify_CombatMarker* Marker = Cast<UAnimNotify_CombatMarker>(Event.Notify))
		{
			bMatch = (Marker->MarkerName == EventName);
		}
		if (!bMatch)
		{
			continue;
		}
		const float NotifyTime = Event.GetTime();
		if (SectionIndex == INDEX_NONE
			|| (NotifyTime >= SectionStart - UE_KINDA_SMALL_NUMBER && NotifyTime <= SectionEnd + UE_KINDA_SMALL_NUMBER))
		{
			return (NotifyTime - SectionStart) / FMath::Max(0.01f, AnimRef.PlayRate);
		}
	}
	return -1.0f;
}

float UBattleComponent::GetSectionStartTime(UAnimMontage* Montage, FName SectionName) const
{
	if (!Montage || SectionName.IsNone())
	{
		return 0.0f;
	}
	const int32 SectionIndex = Montage->GetSectionIndex(SectionName);
	if (SectionIndex == INDEX_NONE)
	{
		return 0.0f;
	}
	float SectionStart = 0.0f;
	float SectionEnd = 0.0f;
	Montage->GetSectionStartAndEndTime(SectionIndex, SectionStart, SectionEnd);
	return SectionStart;
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

	// 无蒙太奇承载命中通知时立即结算，避免回合闸门死锁
	if (Hit.bActive && Hit.Amount > 0.0f && Hit.FallbackMontage == nullptr)
	{
		ApplyPendingHitNow(bPlayerAttacker);
	}
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
			UAnimMontage* HitMontage = HitReaction.Montage.LoadSynchronous();

			// 命中反应强制打断当前动作（如被防动画 BlockedReaction）；
			// 但蓄力抵抗的反应与当前蓄力姿态是同一蒙太奇，不能打断重播，否则白刀命中时会看到蓄力重播一次
			if (UAnimInstance* AnimInstance = Target->GetMesh()->GetAnimInstance())
			{
				if (UAnimMontage* Active = AnimInstance->GetCurrentActiveMontage())
				{
					if (Active != HitMontage)
					{
						AnimInstance->Montage_Stop(0.1f, Active);
					}
				}
			}

			// 蓄力抵抗（白攻 vs 蓄力）：受击方保持蓄力姿态，不打断不重播；
			// 有额外回合时姿态计入回合闸门，完整播完当前循环后才进入额外回合
			const bool bTargetIsPlayer = (Target == PlayerRole.Get());
			const bool bTargetHasExtraTurn = bTargetIsPlayer ? bPlayerExtraTurnPending : bEnemyExtraTurnPending;
			const FCombatAnimRow* TargetRow = GetCombatAnimRow(bTargetIsPlayer);
			const bool bResistChargePose = TargetRow
				&& !TargetRow->Charge.Montage.IsNull()
				&& HitMontage && HitMontage == TargetRow->Charge.Montage.LoadSynchronous();
			if (bResistChargePose)
			{
				PlayChargeResistPose(Target, HitReaction, bTargetHasExtraTurn);
			}
			else
			{
				PlayCombatAnim(Target, HitReaction);
			}
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
	if (EventName == FName(TEXT("ClashAttackHit")))
	{
		const bool bPlayerAttacker = (Attacker == PlayerRole.Get());
		UE_LOG(LogTemp, Log, TEXT("UBattleComponent::OnHitNotify - ClashAttackHit bPlayerAttacker=%d bActive=%d Amount=%.1f"),
			bPlayerAttacker ? 1 : 0, (bPlayerAttacker ? PlayerPendingHit : EnemyPendingHit).bActive ? 1 : 0,
			bPlayerAttacker ? PlayerPendingHit.Amount : EnemyPendingHit.Amount);
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

	if (!bHitStopActive)
	{
		if (PlayerRole.IsValid())
		{
			if (UAnimInstance* AI = PlayerRole->GetMesh()->GetAnimInstance())
			{
				if (UAnimMontage* Active = AI->GetCurrentActiveMontage())
				{
					AI->Montage_Pause(Active);
					HitStopInstances.Add(AI);
					HitStopMontages.Add(Active);
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
					HitStopInstances.Add(AI);
					HitStopMontages.Add(Active);
				}
			}
		}
		bHitStopActive = true;
		SetComponentTickEnabled(true);
	}

	// 固定时长：以帧 DeltaTime 累积倒计时；重叠触发时保留更长的剩余时间
	HitStopRemaining = FMath::Max(HitStopRemaining, Duration);
}

void UBattleComponent::EndHitStop()
{
	if (!bHitStopActive && HitStopInstances.Num() == 0)
	{
		return;
	}
	bHitStopActive = false;
	HitStopRemaining = 0.0f;
	for (int32 i = 0; i < HitStopInstances.Num() && i < HitStopMontages.Num(); ++i)
	{
		if (UAnimInstance* AI = HitStopInstances[i].Get())
		{
			if (UAnimMontage* Montage = HitStopMontages[i])
			{
				// 停帧期间 Montage 可能已被受击/打断停止：仅恢复仍在播放的
				if (AI->Montage_IsActive(Montage))
				{
					AI->Montage_Resume(Montage);
				}
			}
		}
	}
	HitStopInstances.Reset();
	HitStopMontages.Reset();
	SetComponentTickEnabled(false);
}

void UBattleComponent::PlayResolutionAnimations(const FTurnResolution& Resolution)
{
	if (!PlayerRole.IsValid() || !BossEnemy.IsValid())
	{
		return;
	}

	const EBattleAction PlayerAction = PlayerLastAction;
	const EBattleAction EnemyAction = EnemyChosenAction;
	UE_LOG(LogTemp, Log, TEXT("UBattleComponent::PlayResolutionAnimations - PlayerAction=%d EnemyAction=%d PlayerDmg=%.1f EnemyDmg=%.1f PlayerStacks=%d EnemyStacks=%d"),
		(int32)PlayerAction, (int32)EnemyAction, Resolution.PlayerDamageTaken, Resolution.EnemyDamageTaken,
		PlayerChargeStacks, EnemyChargeStacks);

	// 额外回合：只有获得额外回合的一方行动；另一方保留的 LastAction 是上一回合的旧值，不能播
	if (Resolution.bPlayerOnlyAction)
	{
		RegisterSideHit(true, Resolution);
		return;
	}
	if (Resolution.bEnemyOnlyAction)
	{
		RegisterSideHit(false, Resolution);
		return;
	}

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

bool UBattleComponent::IsActionSuppressed(bool bPlayerSide, EBattleAction OtherAction) const
{
	const EBattleAction OwnAction = bPlayerSide ? PlayerLastAction : EnemyChosenAction;
	switch (OwnAction)
	{
	case EBattleAction::WhiteAttack:
		// 白攻被蓝攻克制：不播白攻动画
		return OtherAction == EBattleAction::BlueAttack;
	case EBattleAction::RedDefense:
		// 红防被白攻克制：不播红防动画
		return OtherAction == EBattleAction::WhiteAttack;
	case EBattleAction::Charge:
		// 蓄力被蓝攻打断：不播蓄力动画
		return OtherAction == EBattleAction::BlueAttack;
	default:
		return false;
	}
}

void UBattleComponent::RegisterSideHit(bool bPlayerAttacker, const FTurnResolution& Resolution)
{
	const EBattleAction AttackerAction = bPlayerAttacker ? PlayerLastAction : EnemyChosenAction;
	// 额外回合中另一方没有行动：OtherAction 视为 None，避免旧行动误触发克制/打断判定
	const bool bSingleSideResolution = Resolution.bPlayerOnlyAction || Resolution.bEnemyOnlyAction;
	const EBattleAction OtherAction = bSingleSideResolution
		? EBattleAction::None
		: (bPlayerAttacker ? EnemyChosenAction : PlayerLastAction);
	const bool bTargetInterrupted = bPlayerAttacker ? Resolution.bEnemyChargeInterrupted : Resolution.bPlayerChargeInterrupted;
	const bool bTargetResist = bPlayerAttacker ? Resolution.bEnemyChargeResisted : Resolution.bPlayerChargeResisted;
	const float Amount = bPlayerAttacker ? Resolution.EnemyDamageTaken : Resolution.PlayerDamageTaken;

	// 结算层播放权：被克制/被打断的侧不播行动动画（白攻被蓝攻克、红防被白攻克、蓄力被蓝攻打断）
	if (IsActionSuppressed(bPlayerAttacker, OtherAction))
	{
		return;
	}

	ABaseCharacter* Attacker = bPlayerAttacker ? Cast<ABaseCharacter>(PlayerRole.Get()) : Cast<ABaseCharacter>(BossEnemy.Get());
	ABaseCharacter* Target = bPlayerAttacker ? Cast<ABaseCharacter>(BossEnemy.Get()) : Cast<ABaseCharacter>(PlayerRole.Get());
	const FCombatAnimRow* AttackerRow = GetCombatAnimRow(bPlayerAttacker);
	const FCombatAnimRow* TargetRow = GetCombatAnimRow(!bPlayerAttacker);
	if (!Attacker || !Target || !AttackerRow || !TargetRow)
	{
		return;
	}

	// 满蓄力 vs 红防：蓄力自动发动强化蓝攻，按蓝攻处理（事件/动画）
	EBattleAction EffectiveAction = AttackerAction;
	if (EffectiveAction == EBattleAction::Charge && Amount > 0.0f)
	{
		EffectiveAction = EBattleAction::BlueAttack;
	}

	const FAnimRef* ActionRef = GetActionRef(*AttackerRow, EffectiveAction);
	const FAnimRef* ChargeRef = GetActionRef(*AttackerRow, EBattleAction::Charge);
	const bool bAutoEnhancedCharge = (AttackerAction == EBattleAction::Charge && Amount > 0.0f
		&& ChargeRef && !ChargeRef->Montage.IsNull()
		&& ActionRef && !ActionRef->Montage.IsNull());
	if (bAutoEnhancedCharge)
	{
		// 满蓄力自动强化蓝攻：先完整播放蓄力动画，播完立即接蓝攻（蓝攻不打断蓄力）
		UAnimMontage* ChargeMontage = ChargeRef->Montage.LoadSynchronous();
		if (ChargeMontage && ChargeMontage->bLoop)
		{
			// 循环蓄力姿态：播完一个循环时长后立即接蓝攻，避免永远等不到 Montage 结束
			PlayCombatAnim(Attacker, *ChargeRef);
			FTimerHandle ChainTimer;
			GetWorld()->GetTimerManager().SetTimer(ChainTimer, [this, Attacker, ActionRef = *ActionRef]()
			{
				if (Phase != EBattlePhase::Ended && Attacker)
				{
					PlayCombatAnim(Attacker, ActionRef);
				}
			}, ChargeMontage->GetPlayLength(), false);
		}
		else
		{
			PlayAnimThenReaction(Attacker, *ChargeRef, *ActionRef);
		}
	}
	else if (ActionRef)
	{
		PlayCombatAnim(Attacker, *ActionRef);
	}

	FName EventName = NAME_None;
	switch (EffectiveAction)
	{
	case EBattleAction::BlueAttack: EventName = FName(TEXT("BlueAttackHit")); break;
	case EBattleAction::WhiteAttack: EventName = FName(TEXT("WhiteAttackHit")); break;
	default: return; // 非攻击行动（红防/蓄力姿态）无命中事件
	}

	// 无伤害时只播行动（蓄力抵抗/无事发生），不注册命中事件
	if (Amount <= 0.0f)
	{
		return;
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
	UE_LOG(LogTemp, Log, TEXT("UBattleComponent::RegisterBlueVsRedHit - bAttackerPlayer=%d Incoming=%.1f Counter=%d"),
		bAttackerPlayer ? 1 : 0, IncomingAmount, bCounterSucceeds ? 1 : 0);

	ABaseCharacter* Attacker = bAttackerPlayer ? Cast<ABaseCharacter>(PlayerRole.Get()) : Cast<ABaseCharacter>(BossEnemy.Get());
	ABaseCharacter* Defender = bAttackerPlayer ? Cast<ABaseCharacter>(BossEnemy.Get()) : Cast<ABaseCharacter>(PlayerRole.Get());
	const FCombatAnimRow* AttackerRow = GetCombatAnimRow(bAttackerPlayer);
	const FCombatAnimRow* DefenderRow = GetCombatAnimRow(!bAttackerPlayer);
	if (!Attacker || !Defender || !AttackerRow || !DefenderRow)
	{
		return;
	}

	const FAnimRef* BlueRef = GetActionRef(*AttackerRow, EBattleAction::BlueAttack);
	// 双向错峰预排：红防开始 = max(0, HitReal - GuardReal)，蓝攻开始 = max(0, GuardReal - HitReal)。
	// 当 GuardReady 晚于命中帧时，红防立即起手、蓝攻延后出刀，保证举剑帧与命中帧重合。
	UAnimMontage* BlueMontage = nullptr;
	float HitReal = -1.0f;
	if (BlueRef)
	{
		BlueMontage = BlueRef->Montage.LoadSynchronous();
		HitReal = GetNotifyRealTime(BlueMontage, FName(TEXT("BlueAttackHit")), *BlueRef);
	}
	const float GuardReal = GetNotifyRealTime(
		DefenderRow->RedDefense.Montage.LoadSynchronous(), FName(TEXT("GuardReady")), DefenderRow->RedDefense);
	const float BlueStart = (HitReal >= 0.0f && GuardReal >= 0.0f) ? FMath::Max(0.0f, GuardReal - HitReal) : 0.0f;
	UE_LOG(LogTemp, Log, TEXT("UBattleComponent::RegisterBlueVsRedHit - 错峰预排 BlueStart=%.3f (HitReal=%.3f GuardReal=%.3f)"),
		BlueStart, HitReal, GuardReal);
	if (BlueRef)
	{
		if (BlueStart > 0.0f)
		{
			// 红防先起手：蓝攻延后 BlueStart 秒再播（正延时定时器可正常触发）
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().ClearTimer(BlueAttackDelayTimer);
				World->GetTimerManager().SetTimer(BlueAttackDelayTimer, [this, Attacker, BlueRefCopy = *BlueRef]()
				{
					if (Phase != EBattlePhase::Ended)
					{
						PlayCombatAnim(Attacker, BlueRefCopy);
					}
				}, BlueStart, false);
			}
		}
		else
		{
			PlayCombatAnim(Attacker, *BlueRef);
		}
	}

	if (bCounterSucceeds)
	{
		const float GoldAmount = bAttackerPlayer ? GetEnemyGoldDamage() : GetPlayerGoldDamage();
		const FAnimRef& GoldCounterRef = DefenderRow->GoldCounter;
		RegisterPendingHit(!bAttackerPlayer, FName(TEXT("GoldCounterHit")), Attacker, GoldAmount, Defender,
			AttackerRow->Hurt, nullptr, FAnimRef(), FAnimRef(),
			GoldCounterRef.Montage.IsNull() ? nullptr : GoldCounterRef.Montage.LoadSynchronous(), false);
	}

	// 先注册金色反击（会清理对方侧槽），再注册蓝攻事件并最后预排红防，
	// 避免后续 RegisterPendingHit 的 ClearPendingHit 清掉刚排好的防御定时器
	FAnimRef DefenderFollowUp = bCounterSucceeds ? DefenderRow->GoldCounter : DefenderRow->Hurt;
	RegisterPendingHit(bAttackerPlayer, FName(TEXT("BlueAttackHit")), Defender, IncomingAmount, Attacker,
		FAnimRef(), Defender, DefenderRow->RedDefense, DefenderFollowUp,
		BlueRef ? BlueRef->Montage.LoadSynchronous() : nullptr, bCounterSucceeds);
	if (BlueRef)
	{
		ScheduleDefenderReaction(bAttackerPlayer ? PlayerPendingHit : EnemyPendingHit, *BlueRef);
	}
}

void UBattleComponent::ScheduleDefenderReaction(const FPendingHitEvent& Hit, const FAnimRef& AttackRef)
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

	// 统一换算为"从 Montage 播放起点开始的真实秒数"：通知绝对时间先减 Section 起点，再按 PlayRate 折算，
	// 否则任一 Montage 的 PlayRate != 1 或从 Section 起播时，红防会提前/延后，举剑帧无法与蓝攻命中点重合
	UAnimMontage* HitMontage = Hit.FallbackMontage;
	UAnimMontage* GuardMontage = Hit.DefenderReaction.Montage.LoadSynchronous();
	const float HitNotify = GetNotifyTime(HitMontage, FName(TEXT("BlueAttackHit")));
	const float GuardNotify = GetNotifyTime(GuardMontage, FName(TEXT("GuardReady")));
	const float HitSectionStart = GetSectionStartTime(HitMontage, AttackRef.SectionName);
	const float GuardSectionStart = GetSectionStartTime(GuardMontage, Hit.DefenderReaction.SectionName);
	const float HitTime = HitNotify >= 0.0f
		? (HitNotify - HitSectionStart) / FMath::Max(0.01f, AttackRef.PlayRate)
		: -1.0f;
	const float GuardReadyTime = GuardNotify >= 0.0f
		? (GuardNotify - GuardSectionStart) / FMath::Max(0.01f, Hit.DefenderReaction.PlayRate)
		: -1.0f;
	float Delay = 0.0f;
	if (HitTime >= 0.0f && GuardReadyTime >= 0.0f)
	{
		Delay = FMath::Max(0.0f, HitTime - GuardReadyTime);
	}
	else if (HitTime >= 0.0f)
	{
		const FCombatParamsRow Defaults;
		const FCombatParamsRow* Params = GetCombatSubsystem() ? GetCombatSubsystem()->GetCombatParams() : nullptr;
		const FCombatParamsRow& P = Params ? *Params : Defaults;
		Delay = FMath::Max(0.0f, HitTime - P.RedDefenseLeadTime);
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::ScheduleDefenderReaction - 红防无 GuardReady 标记（或标记位于播放 Section 之外），使用 RedDefenseLeadTime 回落"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::ScheduleDefenderReaction - 蓝攻无 BlueAttackHit 通知，红防立即启动"));
	}
	// 安全钳制：预排不能晚于蓝攻动作播完，且必须为有限值，否则红防永不播放 → 金色反击待命中挂起 → 回合卡死
	const float MaxDelay = HitMontage
		? FMath::Max(0.0f, HitMontage->GetPlayLength() / FMath::Max(0.01f, AttackRef.PlayRate))
		: 0.0f;
	if (!FMath::IsFinite(Delay))
	{
		Delay = 0.0f;
	}
	Delay = FMath::Clamp(Delay, 0.0f, MaxDelay);
	UE_LOG(LogTemp, Log, TEXT("UBattleComponent::ScheduleDefenderReaction - 红防预排 Delay=%.3f (HitNotify=%.3f HitSectionStart=%.3f HitRate=%.2f | GuardNotify=%.3f GuardSectionStart=%.3f GuardRate=%.2f | 钳制上限=%.3f)"),
		Delay, HitNotify, HitSectionStart, AttackRef.PlayRate, GuardNotify, GuardSectionStart, Hit.DefenderReaction.PlayRate, MaxDelay);

	auto PlayDefenderChain = [this, Defender = Hit.Defender,
		Reaction = Hit.DefenderReaction, FollowUp = Hit.DefenderFollowUp]()
	{
		if (Phase != EBattlePhase::Ended)
		{
			PlayAnimThenReaction(Defender, Reaction, FollowUp);
		}
	};

	if (Delay > 0.0f)
	{
		World->GetTimerManager().SetTimer(Timer, MoveTemp(PlayDefenderChain), Delay, false);
	}
	else
	{
		// UE 定时器不接受 0 速率（SetTimer 会直接失效），Delay<=0 表示必须立即启动红防链
		PlayDefenderChain();
	}
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
	PlayAnimThenReaction(PlayerRole.Get(), Row->Block, Row->GoldCounter);
}

void UBattleComponent::PlayClashFailReaction(EClashResult Result)
{
	const FCombatAnimRow* Row = GetCombatAnimRow(true);
	if (!Row)
	{
		return;
	}
	const FAnimRef& Ref = (Result == EClashResult::DodgeFail) ? Row->DodgeFail : Row->Hurt;
	PlayCombatAnim(PlayerRole.Get(), Ref.Montage.IsNull() ? Row->Hurt : Ref);
}

void UBattleComponent::PlayBlockAnimNow()
{
	if (!PlayerRole.IsValid())
	{
		return;
	}
	const FCombatAnimRow* Row = GetCombatAnimRow(true);
	if (!Row)
	{
		return;
	}
	// 格挡动画只有一种：红防姿态（不使用 BlockFail）
	PlayCombatAnim(PlayerRole.Get(), Row->RedDefense);
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
	// OnMontageEnded 由 PlayCombatAnim 统一绑定（先移除再添加，避免重复绑定 ensure）
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

bool UBattleComponent::IsMontageActiveOnCombatants(UAnimMontage* Montage) const
{
	if (!Montage)
	{
		return false;
	}
	auto IsActiveOn = [Montage](const ABaseCharacter* Character)
	{
		return Character && Character->GetMesh() && Character->GetMesh()->GetAnimInstance()
			&& Character->GetMesh()->GetAnimInstance()->Montage_IsActive(Montage);
	};
	return IsActiveOn(PlayerRole.Get()) || IsActiveOn(BossEnemy.Get());
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
	}
	else if (bEnemyReactionPending && Montage == EnemyPendingActionMontage)
	{
		const FAnimRef ReactionRef = EnemyPendingReactionRef;
		ClearPendingReactionSide(false);
		if (BossEnemy.IsValid() && Phase != EBattlePhase::Ended)
		{
			PlayCombatAnim(BossEnemy.Get(), ReactionRef);
		}
	}

	// 通用回合闸门：结束的蒙太奇移出播出链。
	// 蓄力抵抗会"停止旧实例 + 重播同一蒙太奇资产"，旧实例的结束回调带同一资产指针，
	// 不能把新实例仍在播放的闸门条目删掉，因此先确认该蒙太奇已无活动实例。
	if (bTurnGateOpen && !IsMontageActiveOnCombatants(Montage))
	{
		GatedMontages.Remove(Montage);
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

	// 完整播出链播完且无待命中/待接反应 → 推进回合
	TryAdvanceTurnIfGateDone();
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
		bTurnGateOpen = false;
		GatedMontages.Reset();
		EndHitStop();
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

void UBattleComponent::FinishBattle(bool bPlayerWon, bool bFlee)
{
	if (Phase == EBattlePhase::Ended)
	{
		return;
	}

	SetPhase(EBattlePhase::Ended);
	ClearClashTimers();
	if (!bFlee)
	{
		PlayDeathAnimations(bPlayerWon);
	}
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(EntryDelayTimer);
		GetWorld()->GetTimerManager().ClearTimer(ChargePoseTimer);
		GetWorld()->GetTimerManager().ClearTimer(BlueAttackDelayTimer);
	}
	UnbindCombatInput();
	RemoveCombatMapping();
	FText ResultText = bPlayerWon ? FText::FromString(TEXT("战斗胜利")) : FText::FromString(TEXT("战斗失败"));
	if (bFlee)
	{
		ResultText = FText::FromString(TEXT("敌方逃跑了"));
	}
	ShowResultHUD(ResultText);
	OnBattleStateChanged.Broadcast();

	const float Delay = (bPlayerWon || bFlee) ? 2.0f : DefeatRestartDelay;
	if (bPlayerWon || bFlee)
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
	bTutorialFleeTriggered = false;
	bPlayerChoseAction = false;
	bPlayerExtraTurnPending = false;
	bEnemyExtraTurnPending = false;
	bClashResolved = false;
	bClashWindowOpen = false;
	PendingClashResult = EClashResult::None;
	ClearPendingHits();
	bTurnGateOpen = false;
	GatedMontages.Reset();
	ResetTutorialDirector();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ChargePoseTimer);
		GetWorld()->GetTimerManager().ClearTimer(BlueAttackDelayTimer);
	}
	LastClashInputTime = -1.0f;
	EndHitStop();
	RestoreTimeDilation();
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

float UBattleComponent::GetBlockFailLockout() const
{
	const FCombatParamsRow Defaults;
	const FCombatParamsRow* Params = GetCombatSubsystem() ? GetCombatSubsystem()->GetCombatParams() : nullptr;
	const FCombatParamsRow& P = Params ? *Params : Defaults;
	return P.BlockFailLockoutSeconds;
}

float UBattleComponent::GetRunAwayHPThreshold() const
{
	const FCombatParamsRow Defaults;
	const FCombatParamsRow* Params = GetCombatSubsystem() ? GetCombatSubsystem()->GetCombatParams() : nullptr;
	const FCombatParamsRow& P = Params ? *Params : Defaults;
	return P.RunAwayHPThreshold;
}

float UBattleComponent::GetClashTimeDilation() const
{
	const FCombatParamsRow Defaults;
	const FCombatParamsRow* Params = GetCombatSubsystem() ? GetCombatSubsystem()->GetCombatParams() : nullptr;
	const FCombatParamsRow& P = Params ? *Params : Defaults;
	return P.ClashTimeDilation;
}

void UBattleComponent::ApplyClashTimeDilation()
{
	// 慢放仅序章教学战生效：正式战斗（如 Satan）不使用时间流速修正
	if (!IsTutorialBattle())
	{
		return;
	}
	const float Dilation = GetClashTimeDilation();
	if (Dilation <= 0.0f || bTimeDilationApplied)
	{
		return;
	}
	UWorld* World = GetWorld();
	AWorldSettings* Settings = World ? World->GetWorldSettings() : nullptr;
	if (!Settings)
	{
		return;
	}
	OriginalTimeDilation = Settings->TimeDilation;
	Settings->SetTimeDilation(Dilation);
	bTimeDilationApplied = true;
	UE_LOG(LogTemp, Log, TEXT("UBattleComponent::ApplyClashTimeDilation - 世界时间流速 %.3f（同色碰撞可格挡慢放）"), Dilation);
}

void UBattleComponent::RestoreTimeDilation()
{
	if (!bTimeDilationApplied)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (AWorldSettings* Settings = World->GetWorldSettings())
		{
			Settings->SetTimeDilation(OriginalTimeDilation);
		}
	}
	bTimeDilationApplied = false;
	OriginalTimeDilation = 1.0f;
	UE_LOG(LogTemp, Log, TEXT("UBattleComponent::RestoreTimeDilation - 世界时间流速恢复正常"));
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
