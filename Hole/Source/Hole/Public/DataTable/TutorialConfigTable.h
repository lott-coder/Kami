// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "TutorialConfigTable.generated.h"

class ULevelSequence;

/**
 * FTutorialConfigRow - DT_TutorialConfig 行结构（15 号表，单行 Default）
 *
 * 教学战（序章魔法师）全部配置集中于此，便于策划调整：
 * 教学敌人 ID / 先制 / 锁血 / 逃跑线 / 回合兜底 / 碰撞慢放 / 教学点触发条件 / 首次白攻碰撞 / 全部引导文案。
 * 纯数据：不跨表查找、不含运行逻辑；由教学导演与战斗组件加载。
 * 文案留空时运行回退到代码内默认文案（脚本生成的表会预填相同默认值）。
 * @see DataTable_Spec.md §17 教学战配置
 */
USTRUCT(BlueprintType)
struct HOLE_API FTutorialConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 教学战敌人 ID（教学导演自动启用 / 教学战判定的依据） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	FName TutorialEnemyID = FName(TEXT("apprentice_cave"));

	/** 教学战玩家先制（开场玩家 1 层蓄力） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	bool bPlayerFirstStrike = true;

	/** 教学敌人锁血值（避免必逃演出前被击杀） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	float LockHP = 1.0f;

	/** 敌方血量 ≤ 该比例（0~1）时触发逃跑，立即进入结算链路 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	float RunAwayHPThreshold = 0.3f;

	/** 教学战回合数兜底（>= 该值强制逃跑，避免软锁） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	int32 RunAwayRoundCap = 15;

	/** 同色碰撞可格挡慢放的世界时间流速（0.05=慢放；0=关闭，仅教学战生效） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	float ClashTimeDilation = 0.05f;

	/** 时缓提早时间（秒）：教学战同色碰撞慢放比命中点（ClashHitTime）提前开始的时间；默认取 max(格挡窗/闪避窗较小值, 本值) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	float TimeSlowEarlyTime = 0.15f;

	/** 教学点 A 触发条件：玩家蓄力层数（默认 1，蓄满破红防） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	int32 ChargeCapTriggerStack = 1;

	/** 教学点 A 最早触发回合（RoundNumber >= 该值，默认 2，避免首回合教学） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	int32 ChargeCapMinRound = 2;

	/** 教学点 B 触发条件：玩家蓄力层数（默认 0，蓄力抵抗白攻） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	int32 ChargeResistTriggerStack = 0;

	/** 教学战内第一次使用白攻时强制敌方同步白攻（必定白白碰撞教学） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	bool bFirstWhiteAttackForcesClash = true;

	/** 教学战逃跑 Level Sequence（结算后播放，机制同 Boss 开场动画：播放/镜头混合/IA_Skip 跳过；暂空 = 不播放） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	TSoftObjectPtr<ULevelSequence> FleeSequence;

	// ---- 引导文案（留空 = 回退代码默认文案） ----

	/** 开场总提示 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial|Hints")
	FText OpeningHint;

	/** 教学点 A：蓄力到上限破红防 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial|Hints")
	FText ChargeCapHint;

	/** 教学点 B：蓄力抵抗白攻获得额外回合 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial|Hints")
	FText ChargeResistHint;

	/** 同色碰撞格挡/闪避操作提示 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial|Hints")
	FText ClashHint;

	/** 额外回合提示 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial|Hints")
	FText ExtraTurnHint;
};
