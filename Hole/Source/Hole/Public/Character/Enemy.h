// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/BaseCharacter.h"
#include "DataTable/EnemyConfigTable.h"
#include "Enemy.generated.h"

/**
 * AEnemy — 敌人的中间抽象基类
 *
 * 继承自 ABaseCharacter，添加：
 * - 敌人专属身份标识（EnemyID 对应 DT_EnemyConfig 行名）
 * - 敌人等级与 AI 行为偏好
 * - 掉落配置（烟类型、数量、货币范围）
 * - 重写 InitializeAttributes() 从 DT_EnemyConfig 加载属性
 *
 * 所有具体敌人类型均应继承此类，而非直接继承 ABaseCharacter。
 */
UCLASS(Abstract, Blueprintable)
class HOLE_API AEnemy : public ABaseCharacter
{
	GENERATED_BODY()

public:
	AEnemy();

protected:
	virtual void InitializeAttributes() override;

public:
	// ---- 身份 ----

	/**
	 * 敌人配置 ID，对应 DT_EnemyConfig 的 RowName。
	 * 子类构造函数中设置（如 "apprentice"、"adept"）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Identity")
	FName EnemyID;

	// ---- 敌人属性 ----

	/** 敌人等级 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Base")
	EEnemyTier Tier = EEnemyTier::Normal;

	/** AI 行为偏好 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI")
	EEnemyAIPreference AIPreference = EEnemyAIPreference::Balanced;

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

	// ---- 感知 ----

	/** [待定] 警觉范围（cm） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Perception")
	float AlertRange = 1500.0f;

	/** [待定] 追击范围（cm） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Perception")
	float ChaseRange = 900.0f;

	// ---- 便捷访问 ----

	/** 获取当前 AI 智能度（从 AttributeComponent 读取，支持 buff 修正） */
	UFUNCTION(BlueprintPure, Category = "Enemy")
	float GetAIDifficulty() const;
};
