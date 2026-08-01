// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystem/CombatFormulaSubsystem.h"
#include "Component/AttributeComponent.h"
#include "DataTable/CharacterConfigTable.h"
#include "DataTable/CombatParamsTable.h"
#include "DataTable/EnemyConfigTable.h"
#include "DataTable/MaskConfigTable.h"
#include "Engine/DataTable.h"

UDataTable* UCombatFormulaSubsystem::GetTable(TObjectPtr<UDataTable>& Cache, const TCHAR* Path) const
{
	if (!Cache)
	{
		Cache = LoadObject<UDataTable>(nullptr, Path);
	}
	return Cache;
}

const FCharacterConfigRow* UCombatFormulaSubsystem::GetCharacterRow(FName CharacterID) const
{
	UDataTable* Table = GetTable(CharacterTable, TEXT("/Game/DataTable/DT_CharacterConfig"));
	return Table ? Table->FindRow<FCharacterConfigRow>(CharacterID, TEXT("CombatFormula::Char")) : nullptr;
}

const FEnemyConfigRow* UCombatFormulaSubsystem::GetEnemyRow(FName EnemyID) const
{
	UDataTable* Table = GetTable(EnemyTable, TEXT("/Game/DataTable/DT_EnemyConfig"));
	return Table ? Table->FindRow<FEnemyConfigRow>(EnemyID, TEXT("CombatFormula::Enemy")) : nullptr;
}

const FCombatParamsRow* UCombatFormulaSubsystem::GetCombatParams() const
{
	UDataTable* Table = GetTable(CombatParamsTable, TEXT("/Game/DataTable/DT_CombatParams"));
	return Table ? Table->FindRow<FCombatParamsRow>(FName(TEXT("Default")), TEXT("CombatFormula::Params")) : nullptr;
}

const FMaskConfigRow* UCombatFormulaSubsystem::GetMaskRow(FName MaskID) const
{
	UDataTable* Table = GetTable(MaskTable, TEXT("/Game/DataTable/DT_MaskConfig"));
	return Table ? Table->FindRow<FMaskConfigRow>(MaskID, TEXT("CombatFormula::Mask")) : nullptr;
}

TMap<FName, float> UCombatFormulaSubsystem::BuildCharacterAttributes(FName CharacterID) const
{
	TMap<FName, float> Out;

	const FCharacterConfigRow* CharRow = GetCharacterRow(CharacterID);
	if (!CharRow)
	{
		UE_LOG(LogTemp, Warning, TEXT("UCombatFormulaSubsystem::BuildCharacterAttributes - 找不到角色行: %s"), *CharacterID.ToString());
		return Out;
	}

	// USTRUCT 默认值是唯一回退源：表缺失时使用 FCombatParamsRow 的编辑器默认值，避免散落的硬编码
	const FCombatParamsRow Defaults;
	const FCombatParamsRow& P = GetCombatParams() ? *GetCombatParams() : Defaults;

	// 生存
	Out.Add(AttributeNames::MaxHP(), CharRow->MaxHP);
	Out.Add(AttributeNames::MaxSmokeReserve(), CharRow->MaxSmokeReserve);

	// 伤害
	Out.Add(AttributeNames::BaseDamageScale(), CharRow->BaseDamageScale);
	Out.Add(AttributeNames::BlueAttackBonus(), CharRow->BlueAttackBonus);
	Out.Add(AttributeNames::WhiteAttackBonus(), CharRow->WhiteAttackBonus);

	// 移动 — 从角色配置读取（不同角色可设不同速度）
	Out.Add(AttributeNames::WalkSpeed(), CharRow->WalkSpeed);
	Out.Add(AttributeNames::SprintSpeed(), CharRow->SprintSpeed);
	Out.Add(AttributeNames::LandingLockTime(), CharRow->LandingLockTime);

	// 蓄力
	Out.Add(AttributeNames::ChargeSpeedBonus(), 0.0f);
	Out.Add(AttributeNames::MaxChargeStacks(), static_cast<float>(P.MaxChargeStacks));
	Out.Add(AttributeNames::WhiteInterruptChargeDamageScale(), P.WhiteInterruptChargeDamageScale);

	// 防御/操作
	Out.Add(AttributeNames::BlockWindow(), P.BlockWindowSeconds);
	Out.Add(AttributeNames::DodgeWindow(), P.DodgeWindowSeconds);
	Out.Add(AttributeNames::DodgeFailDamageScale(), P.DodgeFailDamageScale);
	Out.Add(AttributeNames::RedPenetrationScale(), 0.0f);

	// 特殊
	Out.Add(AttributeNames::DamageTakenScale(), 1.0f);
	Out.Add(AttributeNames::CounterDmgBonus(), 0.0f);
	Out.Add(AttributeNames::CounterHealPercent(), 0.0f);

	// 面具/装备基值（面具通过 AddModifier 施加永久修正，基值 = 无修正状态）
	Out.Add(AttributeNames::SmokeGainScale(), 1.0f);
	Out.Add(AttributeNames::RedDamageScale(), 1.0f);
	Out.Add(AttributeNames::BlueDamageScale(), 1.0f);
	Out.Add(AttributeNames::WhiteDamageScale(), 1.0f);
	Out.Add(AttributeNames::HPRegenOnKill(), 0.0f);
	Out.Add(AttributeNames::SkillCostScale(), 1.0f);

	return Out;
}

TMap<FName, float> UCombatFormulaSubsystem::BuildEnemyAttributes(FName EnemyID) const
{
	TMap<FName, float> Out;

	const FEnemyConfigRow* EnemyRow = GetEnemyRow(EnemyID);
	if (!EnemyRow)
	{
		UE_LOG(LogTemp, Warning, TEXT("UCombatFormulaSubsystem::BuildEnemyAttributes - 找不到敌人行: %s"), *EnemyID.ToString());
		return Out;
	}

	const FCombatParamsRow Defaults;
	const FCombatParamsRow& P = GetCombatParams() ? *GetCombatParams() : Defaults;

	// 生存
	Out.Add(AttributeNames::MaxHP(), EnemyRow->MaxHP);
	Out.Add(AttributeNames::MaxSmokeReserve(), 0.0f);

	// 伤害
	Out.Add(AttributeNames::BaseDamageScale(), EnemyRow->BaseDamageScale);
	Out.Add(AttributeNames::BlueAttackBonus(), 0.0f);
	Out.Add(AttributeNames::WhiteAttackBonus(), 0.0f);

	// AI
	Out.Add(AttributeNames::AIDifficulty(), EnemyRow->AIDifficulty);

	// 移动 — 从敌人配置读取（新增行字段，编辑器默认值 300/600/0.3）
	Out.Add(AttributeNames::WalkSpeed(), EnemyRow->WalkSpeed);
	Out.Add(AttributeNames::SprintSpeed(), EnemyRow->SprintSpeed);
	Out.Add(AttributeNames::LandingLockTime(), EnemyRow->LandingLockTime);

	// 蓄力
	Out.Add(AttributeNames::ChargeSpeedBonus(), 0.0f);
	Out.Add(AttributeNames::MaxChargeStacks(), static_cast<float>(P.MaxChargeStacks));
	Out.Add(AttributeNames::WhiteInterruptChargeDamageScale(), P.WhiteInterruptChargeDamageScale);

	// 防御/操作
	Out.Add(AttributeNames::BlockWindow(), P.BlockWindowSeconds);
	Out.Add(AttributeNames::DodgeWindow(), P.DodgeWindowSeconds);
	Out.Add(AttributeNames::DodgeFailDamageScale(), P.DodgeFailDamageScale);
	Out.Add(AttributeNames::RedPenetrationScale(), 0.0f);

	// 特殊
	Out.Add(AttributeNames::DamageTakenScale(), 1.0f);
	Out.Add(AttributeNames::CounterDmgBonus(), 0.0f);
	Out.Add(AttributeNames::CounterHealPercent(), 0.0f);

	// 面具/装备基值（敌人同样可佩戴面具类装备时使用）
	Out.Add(AttributeNames::SmokeGainScale(), 1.0f);
	Out.Add(AttributeNames::RedDamageScale(), 1.0f);
	Out.Add(AttributeNames::BlueDamageScale(), 1.0f);
	Out.Add(AttributeNames::WhiteDamageScale(), 1.0f);
	Out.Add(AttributeNames::HPRegenOnKill(), 0.0f);
	Out.Add(AttributeNames::SkillCostScale(), 1.0f);

	return Out;
}

float UCombatFormulaSubsystem::CalculateFinalValue(float BaseValue, const TArray<FAttributeModifier>& Modifiers, FName AttributeName) const
{
	float AddAccum = 0.0f;
	float MulAccum = 1.0f;

	for (const FAttributeModifier& Mod : Modifiers)
	{
		if (Mod.AttributeName != AttributeName)
		{
			continue;
		}

		if (Mod.Op == EModifierOp::Add)
		{
			AddAccum += Mod.Value;
		}
		else
		{
			MulAccum *= Mod.Value;
		}
	}

	return (BaseValue + AddAccum) * MulAccum;
}
