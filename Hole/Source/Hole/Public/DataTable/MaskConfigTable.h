// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MaskConfigTable.generated.h"

// ============================================================================
// 面具相关枚举
// ============================================================================

/** 面具稀有度 */
UENUM(BlueprintType)
enum class EMaskRarity : uint8
{
	Common		UMETA(DisplayName = "普通"),
	Rare		UMETA(DisplayName = "稀有"),
	Legendary	UMETA(DisplayName = "传说"),
	Demonic		UMETA(DisplayName = "恶魔")
};

// ============================================================================
// FMaskConfigRow — DT_MaskConfig 的行结构体
// ============================================================================

/**
 * 定义每种面具的配置数据。
 * 面具效果全部通过 AddModifier() 施加属性修正，不需要独立的 C++ 类。
 * 策划在编辑器的 DataTable 资产中填写每行数值。
 *
 * @see DataTable_Spec.md §7 — 面具配置
 */
USTRUCT(BlueprintType)
struct HOLE_API FMaskConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	// ---- 显示 ----

	/** 面具显示名（中文） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask|Display")
	FText DisplayName;

	// ---- 基础属性 ----

	/** 稀有度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask|Base")
	EMaskRarity Rarity = EMaskRarity::Common;

	/** 面具描述（碎片叙事文本） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask|Base")
	FText Description;

	// ---- 属性修正（全部通过 AddModifier 实现） ----

	/** 烟获取量倍率（1.0 = 无加成，如 1.1 = +10%） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask|Modifier")
	float SmokeGainScale = 1.0f;

	/** 红色攻击伤害倍率（1.0 = 无加成，如 1.15 = +15%） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask|Modifier")
	float ColorDamageScale_Red = 1.0f;

	/** 蓝色攻击伤害倍率（1.0 = 无加成） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask|Modifier")
	float ColorDamageScale_Blue = 1.0f;

	/** 白色攻击伤害倍率（1.0 = 无加成） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask|Modifier")
	float ColorDamageScale_White = 1.0f;

	/** 击败敌人后回复 HP 百分比（如 0.05 = 5%） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask|Modifier")
	float HPRegenOnKill = 0.0f;

	/** 技能消耗倍率 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask|Modifier")
	float SkillCostScale = 1.0f;

	// ---- 经济 ----

	/** 掉落概率（0~1），[待定] 击败哪种敌人可掉落 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask|Economy")
	float DropChance = 0.0f;

	/** 商店售价 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask|Economy")
	int32 Price = 0;

	// ---- 资产 ----

	/** 面具图标 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask|Asset")
	TSoftObjectPtr<UTexture2D> IconTexture;

	/** 面具模型（不同面具视觉不同，数据驱动模型替换） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask|Asset")
	TSoftObjectPtr<USkeletalMesh> MeshAsset;
};
