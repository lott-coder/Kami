// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "EnemyConfigTable.generated.h"

class AEnemy;

// ============================================================================
// 敌人相关枚举
// ============================================================================

/** 敌人等级 */
UENUM(BlueprintType)
enum class EEnemyTier : uint8
{
	Tutorial	UMETA(DisplayName = "教学级"),
	Normal		UMETA(DisplayName = "普通"),
	Elite		UMETA(DisplayName = "精英"),
	Boss		UMETA(DisplayName = "Boss"),
	FinalBoss	UMETA(DisplayName = "最终Boss")
};

/** 敌人 AI 行为偏好 */
UENUM(BlueprintType)
enum class EEnemyAIPreference : uint8
{
	Balanced		UMETA(DisplayName = "均衡型"),
	PreferWhite		UMETA(DisplayName = "偏好白攻"),
	PreferBlue		UMETA(DisplayName = "偏好蓝攻"),
	PreferCharge	UMETA(DisplayName = "偏好蓄力"),
	Adaptive		UMETA(DisplayName = "自适应型"),
	Random			UMETA(DisplayName = "随机型")
};

// ============================================================================
// FEnemyConfigRow — DT_EnemyConfig 的行结构体
// ============================================================================

/**
 * 定义每种敌人类型的基础配置数据。
 * 策划在编辑器的 DataTable 资产中填写每行数值。
 *
 * 外键引用：DT_SmokeConfig（DropSmokeType）
 *
 * @see DataTable_Spec.md §5 — 敌人配置
 */
USTRUCT(BlueprintType)
struct HOLE_API FEnemyConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	// ---- 显示 ----

	/** 敌人显示名（中文） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Display")
	FText DisplayName;

	// ---- 基础属性 ----

	/** 敌人等级 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Base")
	EEnemyTier Tier = EEnemyTier::Normal;

	/** 对应 C++ / Blueprint 类（敌人生成时使用） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Base")
	TSubclassOf<AEnemy> EnemyClass;

	/** 最大生命值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Base")
	float MaxHP = 80.0f;

	/** 伤害输出倍率（相对战斗参数基值） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Base")
	float BaseDamageScale = 1.0f;

	// ---- AI 行为 ----

	/** AI 行为偏好 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI")
	EEnemyAIPreference AIPreference = EEnemyAIPreference::Balanced;

	/** AI 智能度（0=随机，1=最优决策），影响"预判玩家行为"的概率 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI")
	float AIDifficulty = 0.5f;

	// ---- 移动 ----

	/** 默认走路速度（cm/s） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Movement")
	float WalkSpeed = 300.0f;

	/** 默认跑动速度（cm/s） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Movement")
	float SprintSpeed = 600.0f;

	/** 落地锁定时间（秒），跳跃/坠落后禁止移动的时长，防止落地动画滑步 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Movement")
	float LandingLockTime = 0.3f;

	// ---- 掉落 ----

	/** 击败后掉落的烟类型 ID（引用 DT_SmokeConfig） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Drop")
	FName DropSmokeType;

	/** 掉落烟数量 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Drop")
	int32 DropSmokeCount = 1;

	/** 掉落货币最小值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Drop")
	int32 DropCurrencyMin = 0;

	/** 掉落货币最大值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Drop")
	int32 DropCurrencyMax = 0;

	// ---- 生成 ----

	/** 出现区域列表（逗号分隔 AreaID，如 "town,market"），[待定] 可改为数组 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Spawn")
	FString SpawnAreas;

	/** [待定] 警觉范围（cm），GDD §5.5 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Spawn")
	float AlertRange = 1500.0f;

	/** [待定] 追击范围（cm），GDD §5.5 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Spawn")
	float ChaseRange = 900.0f;
};
