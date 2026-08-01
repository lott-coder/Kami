// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "SkillTreeConfigTable.generated.h"

/** 技能树分支 */
UENUM(BlueprintType)
enum class ESkillTreeBranch : uint8
{
	Foundation		UMETA(DisplayName = "基础强化"),
	RedSpecialty	UMETA(DisplayName = "红色专精"),
	BlueSpecialty	UMETA(DisplayName = "蓝色专精"),
	WhiteSpecialty	UMETA(DisplayName = "白色专精"),
	DefenseSpecialty	UMETA(DisplayName = "防御专精"),
	CounterSpecialty	UMETA(DisplayName = "反击专精"),
	DodgeSpecialty	UMETA(DisplayName = "闪避专精")
};

/**
 * FSkillTreeConfigRow — DT_SkillTreeConfig 的行结构体
 *
 * 局外技能树节点。解锁后效果通过 AddModifier() 施加永久属性修正。
 *
 * @see DataTable_Spec.md §10 — 技能树节点
 */
USTRUCT(BlueprintType)
struct HOLE_API FSkillTreeConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 节点显示名 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillTree|Display")
	FText DisplayName;

	/** 效果描述 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillTree|Display")
	FText Description;

	// ---- 结构 ----

	/** 所属分支 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillTree|Structure")
	ESkillTreeBranch Branch = ESkillTreeBranch::Foundation;

	/** 层级（0=根，2=最深） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillTree|Structure")
	int32 Tier = 0;

	/** 前置节点 RowName（空=根节点） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillTree|Structure")
	FName ParentNodeID;

	// ---- 解锁 ----

	/** 解锁所需的烟类型（引用 DT_SmokeConfig） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillTree|Unlock")
	FName RequiredSmokeType;

	/** 需要消耗的烟数量 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillTree|Unlock")
	int32 RequiredSmokeCount = 1;

	/** 博士提炼费用（货币） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillTree|Unlock")
	int32 CurrencyCost = 100;

	// ---- 效果 ----

	/** 效果类型标识（代码用），如 "MaxHP"、"BlockWindow" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillTree|Effect")
	FString EffectType;

	/** 效果数值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillTree|Effect")
	float EffectValue = 0.0f;
};
