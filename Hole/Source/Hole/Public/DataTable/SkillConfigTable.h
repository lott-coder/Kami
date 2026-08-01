// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "SkillConfigTable.generated.h"

class ASkill;

/** 技能类别 */
UENUM(BlueprintType)
enum class ESkillCategory : uint8
{
	Healing		UMETA(DisplayName = "治愈类"),
	PhysEnhance	UMETA(DisplayName = "肉体强化类"),
	Attack		UMETA(DisplayName = "攻击类"),
	Mental		UMETA(DisplayName = "精神类"),
	Exclusive	UMETA(DisplayName = "专属技能"),
	TimeMagic	UMETA(DisplayName = "时间魔法类")
};

/** 技能目标类型 */
UENUM(BlueprintType)
enum class ESkillTarget : uint8
{
	Self		UMETA(DisplayName = "自身"),
	SingleEnemy	UMETA(DisplayName = "单体敌人"),
	AllEnemies	UMETA(DisplayName = "全体敌人"),
	Ally		UMETA(DisplayName = "友方"),
	AllAllies	UMETA(DisplayName = "全体友方")
};

/**
 * FSkillConfigRow — DT_SkillConfig 的行结构体
 *
 * 技能定义与消耗。效果通过 AddModifier 体现，技能本身不改属性。
 *
 * @see DataTable_Spec.md §9 — 技能配置
 */
USTRUCT(BlueprintType)
struct HOLE_API FSkillConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	// ---- 显示 ----

	/** 技能显示名 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Display")
	FText DisplayName;

	/** 技能描述（游戏内显示） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Display")
	FText Description;

	/** 技能图标 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Display")
	TSoftObjectPtr<UTexture2D> IconTexture;

	// ---- 基础 ----

	/** 对应 C++ / Blueprint 类（技能执行时使用） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Base")
	TSubclassOf<ASkill> SkillClass;

	/** 技能类别 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Base")
	ESkillCategory Category = ESkillCategory::Attack;

	/** 目标类型 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Base")
	ESkillTarget TargetType = ESkillTarget::SingleEnemy;

	// ---- 消耗/冷却 ----

	/** 烟储备消耗量 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Cost")
	float SmokeCost = 1.0f;

	/** 冷却回合数（0=无冷却） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Cost")
	int32 CooldownTurns = 0;

	/** 每轮轮回最大使用次数（-1=无限） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Cost")
	int32 MaxUsesPerLoop = -1;

	// ---- 效果 ----

	/** 基础效果数值（伤害量/回复量/增益百分比等） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Effect")
	float BaseEffectValue = 10.0f;

	/** 效果持续回合数（0=即时效果） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Effect")
	int32 EffectDurationTurns = 0;

	/** 是否无视颜色克制（GDD §5.2.6：所有技能均无视） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Effect")
	bool bIgnoresElementalColor = true;

	/** 技能是否在轮回间保留 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Loop")
	bool bRetainedAcrossLoops = true;

	// ---- 获取 ----

	/** 获取来源烟类型（引用 DT_SmokeConfig） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Obtain")
	FName ObtainFromSmokeType;

	/** 获取方式文字描述 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Obtain")
	FText ObtainDescription;
};
