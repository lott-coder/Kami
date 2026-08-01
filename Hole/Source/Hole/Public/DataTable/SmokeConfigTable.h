// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "SmokeConfigTable.generated.h"

/** 烟来源敌人分类 */
UENUM(BlueprintType)
enum class ESmokeSource : uint8
{
	Inept		UMETA(DisplayName = "无能力者"),
	Apprentice	UMETA(DisplayName = "低级魔法师"),
	Adept		UMETA(DisplayName = "高级魔法师"),
	Commander	UMETA(DisplayName = "魔法师统领"),
	BorderGuard	UMETA(DisplayName = "边境守卫"),
	Demon		UMETA(DisplayName = "恶魔"),
	Satan		UMETA(DisplayName = "撒旦"),
	Friendly	UMETA(DisplayName = "友善生物"),
	Puzzle		UMETA(DisplayName = "解密/隐藏区域")
};

/**
 * FSmokeConfigRow — DT_SmokeConfig 的行结构体
 *
 * 定义每种烟的分类、力量印记与转化规则。
 * 掉落/转化系统读取此表（纯物品产出，不进入属性组件）。
 *
 * @see DataTable_Spec.md §8 — 烟分类配置
 */
USTRUCT(BlueprintType)
struct HOLE_API FSmokeConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 烟显示名（中文） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Display")
	FText DisplayName;

	/** 来源敌人分类 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Base")
	ESmokeSource Source = ESmokeSource::Inept;

	/** 力量印记描述（给策划看的，不显示在游戏中） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Base")
	FText PowerImprint;

	// ---- 转化规则 ----

	/** 是否自动转化为技能 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Convert")
	bool bConvertToSkill = false;

	/** 转化后的技能 ID（引用 DT_SkillConfig，可不填） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Convert")
	FName ConvertedSkillID;

	/** 是否自动转化为道具 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Convert")
	bool bConvertToItem = false;

	/** 转化后的道具 ID（引用 DT_ConsumableConfig，可不填） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Convert")
	FName ConvertedItemID;

	/** 是否可在博士处提炼为永久被动 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Convert")
	bool bCanRefineToPassive = false;

	// ---- 经济 ----

	/** 是否可兑换为货币（仅治愈之烟为 true） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Economy")
	bool bCanExchangeForCurrency = false;

	/** 兑换货币数量（仅治愈之烟），[待定] 约 50-100 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Economy")
	int32 CurrencyPerSmoke = 0;

	/** 是否可直接回复烟储备（仅治愈之烟为 true） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Economy")
	bool bCanDirectHealSmokeReserve = false;

	/** 直接回复烟储备量 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Economy")
	float DirectHealAmount = 0.0f;
};
