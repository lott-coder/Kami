// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "EconomyConfigTable.generated.h"

/**
 * FEconomyConfigRow — DT_EconomyConfig 的行结构体
 *
 * 货币兑换率与价格基数。由 UEconomySubsystem 读取。
 *
 * @see DataTable_Spec.md §13 — 货币/经济配置
 */
USTRUCT(BlueprintType)
struct HOLE_API FEconomyConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 参数显示名 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy|Display")
	FText DisplayName;

	/** 参数类别（"Exchange" / "Refine" / "Price" / "Drop"） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy|Base")
	FString Category;

	/** 参数值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy|Base")
	float Value = 0.0f;

	/** 用途说明（给策划看） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy|Display")
	FText Description;
};
