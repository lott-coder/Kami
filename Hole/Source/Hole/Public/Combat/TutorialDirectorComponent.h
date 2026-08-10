// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/BattleTypes.h"
#include "TutorialDirectorComponent.generated.h"

class UBattleComponent;

/**
 * UTutorialDirectorComponent - 序章教学导演（挂在教学怪 BP_Apprentice_Cave 上）
 *
 * 纯决策层：根据战斗状态输出 敌方行动覆盖 / 玩家行动锁定 / 提示文案 / 必逃判定 / 首次白攻碰撞。
 * 不持有战斗执行状态（血量/层数/阶段由 UBattleComponent 持有），执行仍由战斗组件负责。
 *
 * EnemyID == "apprentice_cave" 时自动启用。
 *
 * @see Plans/tutorial-enemy.md — 教学引导设计
 */
UCLASS(ClassGroup = (Combat), Blueprintable, meta = (BlueprintSpawnableComponent))
class HOLE_API UTutorialDirectorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTutorialDirectorComponent();

	virtual void BeginPlay() override;

	/** 是否启用（EnemyID=apprentice_cave 时自动启用） */
	bool IsActive() const { return bActive; }

	/** 进入战斗：标记首条总提示待显示（玩家首次选择行动后清除） */
	void OnBattleEntered();

	/** 每回合开始：根据玩家层数决定教学点（更新敌方行动覆盖 / 蓄力锁定 / 提示文案） */
	void UpdateForRound(int32 RoundNumber, int32 PlayerChargeStacks);

	/** 教学导演覆盖的敌方行动（None = 随机 AI） */
	EBattleAction GetForcedEnemyAction() const { return ForcedEnemyAction; }

	/** 当前锁定的玩家行动（教学点回合 = Charge；非法/非教学点 = None） */
	EBattleAction GetLockedPlayerAction(int32 PlayerChargeStacks, int32 MaxChargeStacks) const;

	/** 当前应显示的提示文案（含碰撞提示/开场总提示/教学点提示；无提示返回空） */
	FText GetTutorialHintText(EBattlePhase Phase, bool bClashResolved, bool bPlayerChoseAction, bool bPlayerExtraTurnPending) const;

	/** 玩家确认行动：清除首条总提示；教学战内第一次白攻返回 true（战斗组件需把敌方行动覆盖为白攻） */
	bool OnPlayerChoseAction(EBattleAction Action, bool bExtraTurn);

	/** 必逃判定：两个蓄力教学点均演示后血量 ≤ 阈值，或 15 回合兜底 */
	bool ShouldTriggerFlee(int32 RoundNumber, float CurrentHealth, float MaxHealth, float RunAwayHPThreshold) const;

	/** 重置导演状态（失败重开时调用） */
	void ResetDirector();

private:
	bool bActive = false;
	/** 开场总提示待显示（进入战斗后置位，玩家首次选择行动后清除） */
	bool bInitialHintPending = false;
	/** 教学点 A：蓄力到上限破红防（玩家 1 层 + 敌方红防）是否已演示 */
	bool bChargeCapTaught = false;
	/** 教学点 B：蓄力抵抗白攻获得额外回合（玩家 0 层 + 敌方白攻）是否已演示 */
	bool bChargeResistTaught = false;
	/** 玩家是否已使用过白攻（第一次使用白攻时强制敌方同步白攻，触发白白碰撞教学） */
	bool bFirstWhiteAttackUsed = false;
	/** 教学点进行中：本回合锁定玩家只能使用蓄力 */
	bool bChargeLockActive = false;
	/** 教学导演覆盖的敌方行动（None = 随机 AI） */
	EBattleAction ForcedEnemyAction = EBattleAction::None;
	/** 当前教学点提示文字（非教学点回合为空） */
	FText ActiveHint;
};
