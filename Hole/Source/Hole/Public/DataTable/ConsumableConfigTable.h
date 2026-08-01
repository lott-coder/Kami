// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ConsumableConfigTable.generated.h"

class AConsumable;

/** 消耗品类型 */
UENUM(BlueprintType)
enum class EConsumableType : uint8
{
	HPRecovery		UMETA(DisplayName = "HP回复"),
	SmokeRecovery	UMETA(DisplayName = "烟储备回复"),
	TempBuff		UMETA(DisplayName = "临时增益"),
	DamageItem		UMETA(DisplayName = "伤害道具"),
	StatBoost		UMETA(DisplayName = "属性提升"),
	KeyItem			UMETA(DisplayName = "关键道具")
};

/**
 * FConsumableConfigRow — DT_ConsumableConfig 的行结构体
 *
 * 消耗品定义。使用时由背包系统转成 AddModifier 或即时效果。
 *
 * @see DataTable_Spec.md §12 — 消耗品配置
 */
USTRUCT(BlueprintType)
struct HOLE_API FConsumableConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	// ---- 显示 ----

	/** 消耗品显示名 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable|Display")
	FText DisplayName;

	/** 物品描述（碎片叙事文本） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable|Display")
	FText Description;

	/** 物品图标 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable|Display")
	TSoftObjectPtr<UTexture2D> IconTexture;

	// ---- 基础 ----

	/** 对应 C++ / Blueprint 类（消耗品使用时实例化） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable|Base")
	TSubclassOf<AConsumable> ConsumableClass;

	/** 消耗品类型 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable|Base")
	EConsumableType Type = EConsumableType::HPRecovery;

	/** 基础效果数值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable|Base")
	float EffectValue = 0.0f;

	/** 效果持续回合数（0=即时） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable|Base")
	int32 EffectDurationTurns = 0;

	/** 最大携带量 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable|Base")
	int32 MaxCarryCount = 5;

	/** 是否可在战斗中使用 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable|Base")
	bool bUsableInCombat = true;

	/** 是否在轮回间保留 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable|Loop")
	bool bRetainedAcrossLoops = false;

	// ---- 经济/获取 ----

	/** 商店售价 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable|Economy")
	int32 Price = 0;

	/** 从哪种烟转化而来（引用 DT_SmokeConfig） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable|Obtain")
	FName ObtainFromSmokeType;
};
