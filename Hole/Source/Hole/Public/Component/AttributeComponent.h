// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AttributeComponent.generated.h"

// ============================================================================
// 属性名常量（防拼写错误）
// ============================================================================

namespace AttributeNames
{
	// -- 生存 --
	FORCEINLINE FName MaxHP()				{ return FName(TEXT("MaxHP")); }
	FORCEINLINE FName MaxSmokeReserve()		{ return FName(TEXT("MaxSmokeReserve")); }

	// -- 伤害 --
	FORCEINLINE FName BaseDamageScale()		{ return FName(TEXT("BaseDamageScale")); }
	FORCEINLINE FName BlueAttackBonus()		{ return FName(TEXT("BlueAttackBonus")); }
	FORCEINLINE FName WhiteAttackBonus()	{ return FName(TEXT("WhiteAttackBonus")); }

	// -- 防御/操作 --
	FORCEINLINE FName BlockWindow()			{ return FName(TEXT("BlockWindow")); }
	FORCEINLINE FName DodgeWindow()			{ return FName(TEXT("DodgeWindow")); }
	FORCEINLINE FName DodgeFailDamageScale(){ return FName(TEXT("DodgeFailDamageScale")); }
	FORCEINLINE FName RedPenetrationScale()	{ return FName(TEXT("RedPenetrationScale")); }

	// -- 蓄力 --
	FORCEINLINE FName ChargeSpeedBonus()	{ return FName(TEXT("ChargeSpeedBonus")); }
	FORCEINLINE FName MaxChargeStacks()		{ return FName(TEXT("MaxChargeStacks")); }
	FORCEINLINE FName WhiteInterruptChargeDamageScale() { return FName(TEXT("WhiteInterruptChargeDamageScale")); }

	// -- 特殊 --
	FORCEINLINE FName DamageTakenScale()	{ return FName(TEXT("DamageTakenScale")); }
	FORCEINLINE FName CounterDmgBonus()		{ return FName(TEXT("CounterDmgBonus")); }
	FORCEINLINE FName CounterHealPercent()	{ return FName(TEXT("CounterHealPercent")); }

	// -- 移动（[待定] DT_CombatParams 后迁移） --
		// -- AI --
		FORCEINLINE FName AIDifficulty()		{ return FName(TEXT("AIDifficulty")); }
	FORCEINLINE FName WalkSpeed()			{ return FName(TEXT("WalkSpeed")); }
	FORCEINLINE FName SprintSpeed()			{ return FName(TEXT("SprintSpeed")); }
	FORCEINLINE FName LandingLockTime()		{ return FName(TEXT("LandingLockTime")); }
}

// ============================================================================
// 属性修正器
// ============================================================================

/** 修正器操作类型 */
UENUM(BlueprintType)
enum class EModifierOp : uint8
{
	Add			UMETA(DisplayName = "加法"),
	Multiply	UMETA(DisplayName = "乘法")
};

/**
 * FAttributeModifier — 属性修正器
 *
 * 表达一个 buff / debuff / 装备 / 被动 对某个属性的修正。
 * RemainingTurns = 0 表示永久修正（装备、技能树被动、永久道具）。
 */
USTRUCT(BlueprintType)
struct HOLE_API FAttributeModifier
{
	GENERATED_BODY()

	/** 目标属性名（使用 AttributeNames:: 常量） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName AttributeName;

	/** 修正量（加法时的固定值，或乘法时的倍率） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Value = 0.0f;

	/** 操作类型：加法叠加 或 乘法叠加 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EModifierOp Op = EModifierOp::Add;

	/** 剩余持续回合数。0 = 永久（装备/被动），>0 = 临时 buff/debuff */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RemainingTurns = 0;

	/** 来源标识（调试用），如 "DodgeBuff"、"Mask_RareFire" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName SourceTag;

	bool IsExpired() const { return RemainingTurns < 0; }
	bool IsPermanent() const { return RemainingTurns == 0; }
};

// ============================================================================
// UAttributeComponent
// ============================================================================

/**
 * UAttributeComponent — 运行时属性容器
 *
 * 挂载在 ABaseCharacter / 敌人上，统一管理所有可被 buff/debuff/装备/被动 修改的属性。
 *
 * 架构：
 *   BaseAttributes（DT 加载的基值）
 *      +
 *   ActiveModifiers（buff/debuff/装备/被动栈）
 *      =
 *   CachedFinalAttributes（最终值，GetFinal() 返回）
 *
 * 使用方式：
 *   // 初始化
 *   AttributeComp->InitializeFromCharacterConfig(CharacterID);
 *
 *   // 读取
 *   float maxHP = AttributeComp->GetFinal(AttributeNames::MaxHP());
 *
 *   // 上 buff
 *   AttributeComp->AddModifier(AttributeNames::WhiteAtkBonus(), EModifierOp::Multiply, 1.2f, 1);
 *
 *   // 回合结束
 *   AttributeComp->TickTurn();
 */
UCLASS(ClassGroup = (Combat), Blueprintable, meta = (BlueprintSpawnableComponent))
class HOLE_API UAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAttributeComponent();

protected:
	virtual void BeginPlay() override;

public:
	// ---- 初始化 ----

	/**
	 * 从 DT_CharacterConfig 加载指定角色的基础属性（玩家角色使用）
	 * @param CharacterID DataTable 行名（如 "drifter"、"ace"）
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	void InitializeFromCharacterConfig(FName CharacterID);

	/**
	 * 从 DT_EnemyConfig 加载指定敌人的基础属性（敌人使用）
	 * @param EnemyID DataTable 行名（如 "apprentice"、"adept"）
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	void InitializeFromEnemyConfig(FName EnemyID);

	// ---- 属性读取 ----

	/** 获取某属性的最终值（基值 + 所有 modifier） */
	UFUNCTION(BlueprintPure, Category = "Attribute")
	float GetFinal(FName AttributeName) const;

	/** 获取某属性的基础值（不含 modifier） */
	UFUNCTION(BlueprintPure, Category = "Attribute")
	float GetBase(FName AttributeName) const;

	/** 直接设置基础值（用于初始化或永久变更，非 buff） */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	void SetBase(FName AttributeName, float Value);

	// ---- 修正器管理 ----

	/**
	 * 添加一个属性修正器
	 * @param AttributeName 目标属性名
	 * @param Op 加法或乘法
	 * @param Value 修正值
	 * @param Turns 持续回合数（0 = 永久）
	 * @param SourceTag 来源标识（调试用）
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	void AddModifier(FName AttributeName, EModifierOp Op, float Value, int32 Turns = 0, FName SourceTag = NAME_None);

	/** 移除所有匹配来源标签的修正器 */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	void RemoveModifiersBySource(FName SourceTag);

	/** 移除所有非永久修正器 */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	void RemoveAllTemporaryModifiers();

	/** 回合结束：所有临时 modifier 倒计时 -1，移除过期的 */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	void TickTurn();

	/** 获取当前活跃的修正器列表（只读，调试用） */
	UFUNCTION(BlueprintPure, Category = "Attribute")
	const TArray<FAttributeModifier>& GetActiveModifiers() const { return ActiveModifiers; }

private:
	/** 从 DataTable 加载的基值 */
	UPROPERTY(VisibleAnywhere, Category = "Attribute")
	TMap<FName, float> BaseAttributes;

	/** 基值 + 所有 modifier 叠加后的缓存 */
	UPROPERTY(VisibleAnywhere, Category = "Attribute")
	TMap<FName, float> CachedFinalAttributes;

	/** 当前活跃的修正器栈 */
	UPROPERTY(VisibleAnywhere, Category = "Attribute")
	TArray<FAttributeModifier> ActiveModifiers;

	/** 缓存是否失效（添加/移除 modifier 后置 true，GetFinal 时重算） */
	bool bCacheDirty = true;

	/** 重算所有 CachedFinalAttributes */
	void RebuildCache();

	/** 重算单个属性的最终值 */
	float ComputeFinal(FName AttributeName) const;
};
