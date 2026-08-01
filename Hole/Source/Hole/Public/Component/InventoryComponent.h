// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryComponent.generated.h"

class ABaseCharacter;
class UCombatFormulaSubsystem;
struct FMaskConfigRow;
struct FWeaponConfigRow;

/**
 * UInventoryComponent — 物品/装备容器
 *
 * 当前实现面具装备槽：装备时把 FMaskConfigRow 的修正字段转换为
 * AttributeComponent 的永久 AddModifier（RemainingTurns = 0），
 * 卸下时按 SourceTag 整组移除，符合"新增 buff/装备 = 一行 AddModifier"的迭代规则。
 */
UCLASS(ClassGroup = (Inventory), Blueprintable, meta = (BlueprintSpawnableComponent))
class HOLE_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	// ---- 面具 ----

	/** 装备面具（先卸下旧面具；找不到配置行返回 false） */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Mask")
	bool EquipMask(FName MaskID);

	/** 卸下面具（移除该面具的全部属性修正） */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Mask")
	void UnequipMask();

	/** 当前装备的面具 ID（NAME_None = 未装备） */
	UFUNCTION(BlueprintPure, Category = "Inventory|Mask")
	FName GetEquippedMaskID() const { return EquippedMaskID; }

	/** 是否已装备面具 */
	UFUNCTION(BlueprintPure, Category = "Inventory|Mask")
	bool HasMaskEquipped() const { return EquippedMaskID != NAME_None; }

	// ---- 武器 ----

	/** 装备武器（先卸下旧武器；找不到配置行返回 false），武器修正通过 AddModifier 施加 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Weapon")
	bool EquipWeapon(FName WeaponID);

	/** 卸下武器（移除该武器的全部属性修正） */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Weapon")
	void UnequipWeapon();

	/** 当前装备的武器 ID（NAME_None = 未装备） */
	UFUNCTION(BlueprintPure, Category = "Inventory|Weapon")
	FName GetEquippedWeaponID() const { return EquippedWeaponID; }

	/** 是否已装备武器 */
	UFUNCTION(BlueprintPure, Category = "Inventory|Weapon")
	bool HasWeaponEquipped() const { return EquippedWeaponID != NAME_None; }

private:
	/** 应用/移除面具修正（bApply=true 添加，false 移除） */
	void ApplyMaskModifiers(const FMaskConfigRow& Row, bool bApply);

	/** 面具修正的来源标签（用于 RemoveModifiersBySource） */
	FName MakeMaskSourceTag(FName MaskID) const;

	/** 应用/移除武器修正（bApply=true 添加，false 移除） */
	void ApplyWeaponModifiers(const FWeaponConfigRow& Row, bool bApply);

	/** 武器修正的来源标签（用于 RemoveModifiersBySource） */
	FName MakeWeaponSourceTag(FName WeaponID) const;

	/** 获取战斗公式子系统（无 GameInstance 时返回 nullptr） */
	UCombatFormulaSubsystem* GetCombatSubsystem() const;

	/** 当前装备的面具 ID */
	UPROPERTY(VisibleAnywhere, Category = "Inventory|Mask")
	FName EquippedMaskID;

	/** 当前装备的武器 ID */
	UPROPERTY(VisibleAnywhere, Category = "Inventory|Weapon")
	FName EquippedWeaponID;
};
