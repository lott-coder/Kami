// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/TutorialDirectorComponent.h"
#include "Character/Enemy.h"

UTutorialDirectorComponent::UTutorialDirectorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTutorialDirectorComponent::BeginPlay()
{
	Super::BeginPlay();

	// 序章教学敌人：按行 ID 自动启用教学导演（编辑器侧无需手动勾选）
	if (const AEnemy* Enemy = Cast<AEnemy>(GetOwner()))
	{
		if (Enemy->EnemyID == FName(TEXT("apprentice_cave")))
		{
			bActive = true;
			UE_LOG(LogTemp, Log, TEXT("UTutorialDirectorComponent::BeginPlay - 检测到序章教学敌人，教学导演启用"));
		}
	}
}

void UTutorialDirectorComponent::OnBattleEntered()
{
	if (bActive)
	{
		bInitialHintPending = true;
	}
}

void UTutorialDirectorComponent::UpdateForRound(int32 RoundNumber, int32 PlayerChargeStacks)
{
	// 教学点 A：玩家蓄力 1 层（首回合除外）→ 敌方红防，锁定蓄力，
	// 演示"蓄力到上限后若敌方红防，自动强化蓝攻破防"
	if (RoundNumber > 1 && !bChargeCapTaught && PlayerChargeStacks == 1)
	{
		ForcedEnemyAction = EBattleAction::RedDefense;
		bChargeLockActive = true;
		bChargeCapTaught = true;
		ActiveHint = FText::FromString(TEXT("使用蓄力使蓄力层数到达上限。\n\n若敌方此时使用防御，则会自动对敌方使用蓄力攻击"));
		UE_LOG(LogTemp, Log, TEXT("UTutorialDirectorComponent::UpdateForRound - 教学点A：蓄力到上限破红防"));
		return;
	}
	// 教学点 B：玩家蓄力 0 层 → 敌方白攻，锁定蓄力，
	// 演示"蓄力到达 1 层时若敌方普通攻击，减伤并获得额外回合"
	if (!bChargeResistTaught && PlayerChargeStacks == 0)
	{
		ForcedEnemyAction = EBattleAction::WhiteAttack;
		bChargeLockActive = true;
		bChargeResistTaught = true;
		ActiveHint = FText::FromString(TEXT("使用蓄力使蓄力到达一层时，\n\n若敌方使用普通攻击，则我方减少受到伤害并获得额外回合"));
		UE_LOG(LogTemp, Log, TEXT("UTutorialDirectorComponent::UpdateForRound - 教学点B：蓄力抵抗白攻"));
		return;
	}
	// 非教学点回合：随机 AI、不锁定、无提示
	ForcedEnemyAction = EBattleAction::None;
	bChargeLockActive = false;
	ActiveHint = FText::GetEmpty();
}

EBattleAction UTutorialDirectorComponent::GetLockedPlayerAction(int32 PlayerChargeStacks, int32 MaxChargeStacks) const
{
	if (!bActive || !bChargeLockActive)
	{
		return EBattleAction::None;
	}
	// 教学点条件保证玩家蓄力为 0/1 层；兜底：蓄力已达上限时解锁，避免卡死
	if (PlayerChargeStacks >= MaxChargeStacks)
	{
		return EBattleAction::None;
	}
	return EBattleAction::Charge;
}

FText UTutorialDirectorComponent::GetTutorialHintText(EBattlePhase Phase, bool bClashResolved, bool bPlayerChoseAction, bool bPlayerExtraTurnPending) const
{
	if (!bActive)
	{
		return FText::GetEmpty();
	}
	// 碰撞阶段：对拼操作提示
	if (Phase == EBattlePhase::Clash && !bClashResolved)
	{
		return FText::FromString(TEXT("双方使用相同的攻击触发对拼。\n\n在敌方攻击将要击中的时候点击E格挡/Shift闪避。\n\n格挡成功可以造成反击"));
	}
	if (Phase != EBattlePhase::ActionSelect || bPlayerChoseAction || bPlayerExtraTurnPending)
	{
		return FText::GetEmpty();
	}
	// 进入战斗后的首条总提示（仅一次，玩家首次选择行动后清除）
	if (bInitialHintPending)
	{
		return FText::FromString(TEXT("触发先制攻击在战斗开始时直接获得一层蓄力。\n\n蓄力层数最高两层。\n\n拥有蓄力层数可以使用蓄力攻击。\n\n蓄力攻击会打断敌方的普通攻击。\n\n普通攻击无视防御。\n\n防御可以抵挡并反击蓄力攻击"));
	}
	return ActiveHint;
}

bool UTutorialDirectorComponent::OnPlayerChoseAction(EBattleAction Action, bool bExtraTurn)
{
	bInitialHintPending = false;
	if (!bActive || bExtraTurn || Action != EBattleAction::WhiteAttack || bFirstWhiteAttackUsed)
	{
		return false;
	}
	// 教学战第一次白攻：战斗组件需把敌方行动覆盖为白攻，触发白白碰撞教学
	bFirstWhiteAttackUsed = true;
	return true;
}

bool UTutorialDirectorComponent::ShouldTriggerFlee(int32 RoundNumber, float CurrentHealth, float MaxHealth, float RunAwayHPThreshold) const
{
	if (!bActive)
	{
		return false;
	}
	// 两个蓄力教学点都演示过后才允许血量逃跑（引导不被打断）
	const bool bMomentsDone = bChargeCapTaught && bChargeResistTaught;
	const bool bLowHp = MaxHealth > 0.0f && (CurrentHealth / MaxHealth) <= RunAwayHPThreshold;
	// 兜底：教学点迟迟未触发（如玩家层数始终不到 0/1）时按回合上限逃跑，避免软锁
	const bool bRoundCap = RoundNumber >= 15;
	return (bMomentsDone && bLowHp) || bRoundCap;
}

void UTutorialDirectorComponent::ResetDirector()
{
	bInitialHintPending = false;
	bChargeCapTaught = false;
	bChargeResistTaught = false;
	bFirstWhiteAttackUsed = false;
	bChargeLockActive = false;
	ForcedEnemyAction = EBattleAction::None;
	ActiveHint = FText::GetEmpty();
}
