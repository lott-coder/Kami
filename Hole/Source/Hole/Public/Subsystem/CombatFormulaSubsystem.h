// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CombatFormulaSubsystem.generated.h"

class UDataTable;
struct FCharacterConfigRow;
struct FEnemyConfigRow;
struct FCombatParamsRow;
struct FMaskConfigRow;
struct FAttributeModifier;

/**
 * UCombatFormulaSubsystem — 跨表公式计算 / 多表数据合并
 *
 * 按项目三层架构，所有"看其他表"或"做运算"的逻辑集中在此：
 * - 持有 DataTable 引用（懒加载并缓存）
 * - 玩家/敌人基础属性 = DT 行 + DT_CombatParams 的跨表合并结果
 * - modifier 栈的最终值公式（Add 累加 → Multiply 累乘）
 *
 * UAttributeComponent 只负责存储运行时状态，不直接访问 DataTable。
 */
UCLASS()
class HOLE_API UCombatFormulaSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ---- 行查询（单表） ----

	/** DT_CharacterConfig 行（找不到返回 nullptr） */
	const FCharacterConfigRow* GetCharacterRow(FName CharacterID) const;

	/** DT_EnemyConfig 行（找不到返回 nullptr） */
	const FEnemyConfigRow* GetEnemyRow(FName EnemyID) const;

	/** DT_CombatParams 单行 "Default"（表缺失返回 nullptr） */
	const FCombatParamsRow* GetCombatParams() const;

	/** DT_MaskConfig 行（找不到返回 nullptr） */
	const FMaskConfigRow* GetMaskRow(FName MaskID) const;

	// ---- 跨表合并（角色/敌人基础属性） ----

	/** 玩家基础属性 = DT_CharacterConfig 行 + DT_CombatParams + 默认值 */
	TMap<FName, float> BuildCharacterAttributes(FName CharacterID) const;

	/** 敌人基础属性 = DT_EnemyConfig 行 + DT_CombatParams + 默认值 */
	TMap<FName, float> BuildEnemyAttributes(FName EnemyID) const;

	// ---- 公式 ----

	/** 最终值 = (Base + ΣAdd) × ΠMultiply */
	float CalculateFinalValue(float BaseValue, const TArray<FAttributeModifier>& Modifiers, FName AttributeName) const;

private:
	/** 懒加载并缓存 DataTable */
	UDataTable* GetTable(TObjectPtr<UDataTable>& Cache, const TCHAR* Path) const;

	mutable TObjectPtr<UDataTable> CharacterTable;
	mutable TObjectPtr<UDataTable> EnemyTable;
	mutable TObjectPtr<UDataTable> CombatParamsTable;
	mutable TObjectPtr<UDataTable> MaskTable;
};
