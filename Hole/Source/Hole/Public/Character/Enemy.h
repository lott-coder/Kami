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

	// ---- 敌人配置 ----

	/** 敌人配置行（初始化时从 DT_EnemyConfig 加载，运行时状态的唯一副本） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Config")
	FEnemyConfigRow EnemyConfig;

	// ---- 便捷访问 ----

	/** 获取当前 AI 智能度（从 AttributeComponent 读取，支持 buff 修正） */
	UFUNCTION(BlueprintPure, Category = "Enemy")
	float GetAIDifficulty() const;
};
