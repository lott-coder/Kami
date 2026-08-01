// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CharacterConfigTable.generated.h"

class ABaseCharacter;

/**
 * FCharacterConfigRow — DT_CharacterConfig 的行结构体
 *
 * 定义每个角色（可玩角色或 NPC）的基础配置数据。
 * 策划在编辑器的 DataTable 资产中填写每行数值。
 *
 * 外键引用：DT_WeaponConfig（DefaultWeaponID）、DT_MaskConfig（DefaultMaskID）
 *
 * @see DataTable_Spec.md §4 — 角色配置
 */
USTRUCT(BlueprintType)
struct HOLE_API FCharacterConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	// ---- 显示 ----

	/** 角色显示名（中文） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Display")
	FText DisplayName;

	/** 角色头像 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Display")
	TSoftObjectPtr<UTexture2D> PortraitTexture;

	// ---- 蓝图绑定 ----

	/** 对应 C++ / Blueprint 类 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Class")
	TSubclassOf<ABaseCharacter> CharacterClass;

	// ---- 生存属性 ----

	/** 最大生命值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Survival")
	float MaxHP = 100.0f;

	/** 最大烟储备值（[待定]，非烟囊角色此值为 0） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Survival")
	float MaxSmokeReserve = 10.0f;

	// ---- 伤害属性 ----

	/** 角色基础伤害总倍率（乘法），叠加在全局战斗参数之上 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Damage")
	float BaseDamageScale = 1.0f;

	/** 蓝色攻击固定加成（加法），叠加在倍率计算之后 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Damage")
	float BlueAttackBonus = 0.0f;

	/** 白色攻击固定加成（加法），叠加在倍率计算之后 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Damage")
	float WhiteAttackBonus = 0.0f;

	// ---- 移动 ----

	/** 默认走路速度（cm/s），不按 Shift 时的移动速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Movement")
	float WalkSpeed = 300.0f;

	/** 默认跑动速度（cm/s），按住 Shift 时的移动速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Movement")
	float SprintSpeed = 600.0f;

	/** 落地锁定时间（秒），跳跃/坠落后禁止移动的时长，防止落地动画滑步 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Movement")
	float LandingLockTime = 0.3f;

	// ---- 初始装备 ----

	/** 初始武器 ID（引用 DT_WeaponConfig 的 RowName） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Equipment")
	FName DefaultWeaponID;

	/** 初始面具 ID（引用 DT_MaskConfig 的 RowName） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Equipment")
	FName DefaultMaskID;

	// ---- 身份标识 ----

	/** 是否可被玩家操控 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Identity")
	bool bIsPlayable = false;

	/** 是否有烟囊（魔法师 = true，洞穴人类 = false） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Identity")
	bool bHasSmokeGland = true;

	// ---- 烟回复（队友协助） ----

	/** 每日自动回复烟储备最小值（如艾斯 1），0 = 不回复 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Smoke")
	float DailySmokeRecoveryMin = 0.0f;

	/** 每日自动回复烟储备最大值（如艾斯 2），0 = 不回复 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Smoke")
	float DailySmokeRecoveryMax = 0.0f;

	// ---- 解锁条件 ----

	/** 第几次轮回后解锁（0 = 初始可用） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Unlock")
	int32 UnlockCondition_Round = 1;

	/** 解锁条件文字描述（[待定]） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Unlock")
	FText UnlockCondition_Desc;
};
