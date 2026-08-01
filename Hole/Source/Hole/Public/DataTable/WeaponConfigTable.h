// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "WeaponConfigTable.generated.h"

class AWeapon;

/** 武器类别 */
UENUM(BlueprintType)
enum class EWeaponCategory : uint8
{
	GreatSword	UMETA(DisplayName = "大剑"),
	Hammer		UMETA(DisplayName = "锤子"),
	Sword		UMETA(DisplayName = "单手剑"),
	TBD1		UMETA(DisplayName = "[待定] 武器4"),
	TBD2		UMETA(DisplayName = "[待定] 武器5")
};

/**
 * FWeaponConfigRow — DT_WeaponConfig 的行结构体
 *
 * 定义每种武器的类型与战斗修正。
 * 武器效果通过 AddModifier() 施加属性修正，不需要独立的 C++ 类。
 *
 * @see DataTable_Spec.md §6 — 武器配置
 */
USTRUCT(BlueprintType)
struct HOLE_API FWeaponConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	// ---- 显示 ----

	/** 武器显示名 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Display")
	FText DisplayName;

	/** 武器描述（碎片叙事文本） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Display")
	FText Description;

	/** 武器图标 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Asset")
	TSoftObjectPtr<UTexture2D> IconTexture;

	/** 武器模型 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Asset")
	TSoftObjectPtr<UStaticMesh> MeshAsset;

	// ---- 基础 ----

	/** 武器类别 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Base")
	EWeaponCategory Category = EWeaponCategory::Sword;

	/** 对应 C++ / Blueprint 类（武器实例化时使用） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Class")
	TSubclassOf<AWeapon> WeaponClass;

	// ---- 战斗修正 ----

	/** 蓝色攻击伤害加成（加法） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Combat")
	float BlueAttackDamageMod = 0.0f;

	/** 白色攻击伤害加成（加法） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Combat")
	float WhiteAttackDamageMod = 0.0f;

	/** 蓝色攻击伤害倍率（乘法） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Combat")
	float BlueAttackDamageScale = 1.0f;

	/** 白色攻击伤害倍率（乘法） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Combat")
	float WhiteAttackDamageScale = 1.0f;

	/** 格挡窗口加成（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Combat")
	float BlockWindowBonus = 0.0f;

	/** 闪避窗口加成（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Combat")
	float DodgeWindowBonus = 0.0f;

	/** 对红色防御的穿透伤害比例（0~1） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Combat")
	float RedPenetrationScale = 0.0f;

	/** 额外蓄力回合数（大剑 +1） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Combat")
	int32 ExtraChargeTurns = 0;

	// ---- 经济 ----

	/** 商店售价（货币） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Economy")
	int32 Price = 0;
};
