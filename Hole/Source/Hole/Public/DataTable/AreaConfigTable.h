// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "AreaConfigTable.generated.h"

/**
 * FAreaConfigRow — DT_AreaConfig 的行结构体
 *
 * 世界区域属性（纯静态配置，由关卡管理读取）。
 *
 * @see DataTable_Spec.md §11 — 区域配置
 */
USTRUCT(BlueprintType)
struct HOLE_API FAreaConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 区域显示名（中文） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area|Display")
	FText DisplayName;

	/** 视觉主题描述（给美术参考） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area|Display")
	FText VisualTheme;

	/** 特殊机制描述 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area|Display")
	FText SpecialMechanics;

	// ---- 进度 ----

	/** 第几次轮回起解锁 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area|Progress")
	int32 UnlockRound = 1;

	/** 难度星级（1~5） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area|Progress")
	int32 DifficultyStars = 1;

	/** 是否为安全区（无强制战斗） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area|Progress")
	bool bIsSafeZone = false;

	/** 是否为线性关卡（不可返回） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area|Progress")
	bool bIsLinear = false;

	/** 敌人等级缩放（影响 HP/伤害） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area|Progress")
	float EnemyLevelScale = 1.0f;

	// ---- 视觉/资产 ----

	/** 主色调（RGB） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area|Visual")
	FColor PrimaryColor = FColor(255, 255, 255, 255);

	/** 关卡资产路径 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area|Visual")
	TSoftObjectPtr<UWorld> LevelAsset;
};
