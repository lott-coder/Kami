// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Animation/AnimMontage.h"
#include "CombatAnimConfigTable.generated.h"

/** 单个战斗动画引用：Montage 软引用 + 可选 Section + 播放参数（纯数据） */
USTRUCT(BlueprintType)
struct HOLE_API FAnimRef
{
	GENERATED_BODY()

	/** Montage 软引用；空 = 不播放或按回落约定（BlockFail/DodgeFail/ChargeInterrupted 回落 Hurt） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimRef")
	TSoftObjectPtr<UAnimMontage> Montage;

	/** 起始 Section；NAME_None = 从头播放 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimRef")
	FName SectionName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimRef")
	float PlayRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimRef")
	float BlendOutTime = 0.25f;

	bool IsValidRef() const { return !Montage.IsNull(); }
};

/**
 * FCombatAnimRow - DT_CombatAnimConfig 行结构（13 号表）
 * 一行 = 一个战斗实体（v1：drifter / satan）。
 * 纯数据：不跨表查找、不含运行逻辑。
 * @see DataTable_Spec.md §15 战斗动画配置
 */
USTRUCT(BlueprintType)
struct HOLE_API FCombatAnimRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Action")
	FAnimRef Entry;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Action")
	FAnimRef Sheathe;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Action")
	FAnimRef RedDefense;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Action")
	FAnimRef GoldCounter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Action")
	FAnimRef BlueAttack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Action")
	FAnimRef WhiteAttack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Action")
	FAnimRef Charge;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Action")
	FAnimRef ChargeInterrupted;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Reaction")
	FAnimRef BlockedReaction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Reaction")
	FAnimRef Hurt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Reaction")
	FAnimRef BlockSuccess;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Reaction")
	FAnimRef BlockFail;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Reaction")
	FAnimRef DodgeSuccess;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Reaction")
	FAnimRef DodgeFail;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Reaction")
	FAnimRef Death;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Reaction")
	FAnimRef Victory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Action")
	FAnimRef Skill;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Clash")
	FAnimRef ClashReady;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Clash")
	FAnimRef ClashAttackBlue;

	/** 蓝碰撞攻击可选 Section（竖线分隔，如 "A|B|C"）；空 = 用 ClashAttackBlue.SectionName，非空则每次随机选一段 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Clash")
	FString ClashAttackBlueSections;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Clash")
	FAnimRef ClashAttackWhite;

	/** 白碰撞攻击可选 Section（竖线分隔）；空 = 用 ClashAttackWhite.SectionName，非空则每次随机选一段 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Clash")
	FString ClashAttackWhiteSections;
};
