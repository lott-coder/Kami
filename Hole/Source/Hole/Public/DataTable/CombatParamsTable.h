// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CombatParamsTable.generated.h"

/**
 * FCombatParamsRow — DT_CombatParams 的行结构体
 *
 * 全局战斗数值参数。采用**单例模式**——整张表只有一行（RowName = "Default"）。
 * 策划修改一次，影响所有角色和敌人的战斗数值基础。
 *
 * 若需不同难度分别配置，后续可扩展为多行（RowName = "Normal" / "Hard" / "Nightmare"）。
 *
 * @see DataTable_Spec.md §3 — 全局战斗参数
 */
USTRUCT(BlueprintType)
struct HOLE_API FCombatParamsRow : public FTableRowBase
{
	GENERATED_BODY()

	// ---- 白色攻击 ----

	/** 白色攻击最小伤害 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|WhiteAttack")
	float WhiteAttackDamageMin = 15.0f;

	/** 白色攻击最大伤害 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|WhiteAttack")
	float WhiteAttackDamageMax = 25.0f;

	// ---- 蓝色攻击 ----

	/** 蓝色攻击最小伤害（0蓄力） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|BlueAttack")
	float BlueAttackDamageMin_0Charge = 20.0f;

	/** 蓝色攻击最大伤害（0蓄力） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|BlueAttack")
	float BlueAttackDamageMax_0Charge = 30.0f;

	// ---- 蓄力 ----

	/** 1层蓄力伤害倍率 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Charge")
	float ChargeDamageMultiplier_1 = 1.5f;

	/** 2层蓄力伤害倍率（= 1.5²） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Charge")
	float ChargeDamageMultiplier_2 = 2.25f;

	/** 最大蓄力层数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Charge")
	int32 MaxChargeStacks = 2;

	/** 白攻打断蓄力时的伤害倍率 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Charge")
	float WhiteInterruptChargeDamageScale = 0.3f;

	// ---- 防御/操作 ----

	/** 格挡判定窗口（秒），[PLAYTEST] */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Defense")
	float BlockWindowSeconds = 0.25f;

	/** 闪避判定窗口（秒），[PLAYTEST] */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Defense")
	float DodgeWindowSeconds = 0.35f;

	/** 闪避失败额外伤害倍率 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Defense")
	float DodgeFailDamageScale = 1.2f;

	// ---- 暴击 ----

	/** 蓝色攻击暴击伤害倍率（默认 ×1.5），[PLAYTEST] */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Crit")
	float CritDamageMultiplier = 1.5f;

	// ---- 闪避Buff ----

	/** 闪避成功后的下回合伤害倍率（1.2 = +20%），[PLAYTEST] */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|DodgeBuff")
	float DodgeBuffDamageScale = 1.2f;

	/** 闪避Buff持续回合数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|DodgeBuff")
	int32 DodgeBuffTurns = 1;

	// ---- 先制 ----

	/** 玩家先制攻击的伤害比例（相对白攻基础伤害，0.3 = 30%），[PLAYTEST] */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|FirstStrike")
	float FirstStrikeDamageScale = 0.3f;

	// ---- 金色攻击（格挡反击） ----

	/** 金色攻击（格挡反击）最小伤害，[PLAYTEST] */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|GoldAttack")
	float GoldAttackDamageMin = 25.0f;

	/** 金色攻击（格挡反击）最大伤害，[PLAYTEST] */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|GoldAttack")
	float GoldAttackDamageMax = 35.0f;

	// ---- 特殊规则 ----

	/** 先制攻击使敌人禁用蓄力的回合数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Rules")
	int32 FirstStrikeDisableChargeTurns = 1;

	/** 教学战斗敌人逃跑血量百分比阈值（0~1），[待定] */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Rules")
	float RunAwayHPThreshold = 0.3f;

	// ---- 默认值 ----

	/** 玩家初始 HP（用于新角色默认值，DT_CharacterConfig 行值优先） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Defaults")
	float PlayerDefaultHP = 100.0f;

	// ---- 移动 ----

	/** 后退速度倍率（所有角色通用） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Movement")
	float Movement_BackwardSpeedScale = 0.6f;
};
