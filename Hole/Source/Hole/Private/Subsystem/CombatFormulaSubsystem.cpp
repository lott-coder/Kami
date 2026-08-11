// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystem/CombatFormulaSubsystem.h"
#include "Component/AttributeComponent.h"
#include "DataTable/CharacterConfigTable.h"
#include "DataTable/CombatParamsTable.h"
#include "DataTable/CombatStageTable.h"
#include "DataTable/CombatAnimConfigTable.h"
#include "DataTable/SettlementConfigTable.h"
#include "DataTable/TutorialConfigTable.h"
#include "DataTable/MusicConfigTable.h"
#include "DataTable/EnemyConfigTable.h"
#include "DataTable/MaskConfigTable.h"
#include "DataTable/WeaponConfigTable.h"
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

const FCombatStageRow* UCombatFormulaSubsystem::GetBattleStageRow() const
{
	UDataTable* Table = GetTable(BattleStageTable, TEXT("/Game/DataTable/DT_BattleStage"));
	return Table ? Table->FindRow<FCombatStageRow>(FName(TEXT("Default")), TEXT("CombatFormula::Stage")) : nullptr;
}

const FMaskConfigRow* UCombatFormulaSubsystem::GetMaskRow(FName MaskID) const
{
	UDataTable* Table = GetTable(MaskTable, TEXT("/Game/DataTable/DT_MaskConfig"));
	return Table ? Table->FindRow<FMaskConfigRow>(MaskID, TEXT("CombatFormula::Mask")) : nullptr;
}

const FWeaponConfigRow* UCombatFormulaSubsystem::GetWeaponRow(FName WeaponID) const
{
	UDataTable* Table = GetTable(WeaponTable, TEXT("/Game/DataTable/DT_WeaponConfig"));
	return Table ? Table->FindRow<FWeaponConfigRow>(WeaponID, TEXT("CombatFormula::Weapon")) : nullptr;
}

const FCombatAnimRow* UCombatFormulaSubsystem::GetCombatAnimRow(FName EntityID) const
{
	UDataTable* Table = GetTable(CombatAnimTable, TEXT("/Game/DataTable/DT_CombatAnimConfig"));
	return Table ? Table->FindRow<FCombatAnimRow>(EntityID, TEXT("CombatFormula::CombatAnim")) : nullptr;
}

const FSettlementConfigRow* UCombatFormulaSubsystem::GetSettlementConfigRow() const
{
	UDataTable* Table = GetTable(SettlementTable, TEXT("/Game/DataTable/DT_SettlementConfig"));
	if (!Table)
	{
		UE_LOG(LogTemp, Warning, TEXT("UCombatFormulaSubsystem::GetSettlementConfigRow - 未找到 DT_SettlementConfig，结算参数回退默认值"));
		return nullptr;
	}
	if (const FSettlementConfigRow* Row = Table->FindRow<FSettlementConfigRow>(
		FName(TEXT("Default")), TEXT("CombatFormula::Settlement"), false))
	{
		return Row;
	}
	// 兼容行名不是 "Default"（如编辑器新建时默认 NewRow）：取第一行，避免策划调参静默失效
	for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
	{
		UE_LOG(LogTemp, Log, TEXT("UCombatFormulaSubsystem::GetSettlementConfigRow - 未找到 Default 行，使用第一行 %s"), *Pair.Key.ToString());
		return reinterpret_cast<const FSettlementConfigRow*>(Pair.Value);
	}
	UE_LOG(LogTemp, Warning, TEXT("UCombatFormulaSubsystem::GetSettlementConfigRow - DT_SettlementConfig 没有任何数据行，结算参数回退默认值"));
	return nullptr;
}

const FTutorialConfigRow* UCombatFormulaSubsystem::GetTutorialConfigRow() const
{
	UDataTable* Table = GetTable(TutorialTable, TEXT("/Game/DataTable/DT_TutorialConfig"));
	if (!Table)
	{
		UE_LOG(LogTemp, Warning, TEXT("UCombatFormulaSubsystem::GetTutorialConfigRow - 未找到 DT_TutorialConfig，教学配置回退默认值"));
		return nullptr;
	}
	if (const FTutorialConfigRow* Row = Table->FindRow<FTutorialConfigRow>(
		FName(TEXT("Default")), TEXT("CombatFormula::Tutorial"), false))
	{
		return Row;
	}
	// 兼容行名不是 "Default"（如编辑器新建时默认 NewRow）：取第一行
	for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
	{
		UE_LOG(LogTemp, Log, TEXT("UCombatFormulaSubsystem::GetTutorialConfigRow - 未找到 Default 行，使用第一行 %s"), *Pair.Key.ToString());
		return reinterpret_cast<const FTutorialConfigRow*>(Pair.Value);
	}
	UE_LOG(LogTemp, Warning, TEXT("UCombatFormulaSubsystem::GetTutorialConfigRow - DT_TutorialConfig 没有任何数据行，教学配置回退默认值"));
	return nullptr;
}

const FBGMConfigRow* UCombatFormulaSubsystem::GetAreaBGMConfigRow(FName AreaID) const
{
	UDataTable* Table = GetTable(AreaBGMTable, TEXT("/Game/DataTable/DT_AreaBGMConfig"));
	if (!Table)
	{
		return nullptr;
	}
	if (const FBGMConfigRow* Row = Table->FindRow<FBGMConfigRow>(AreaID, TEXT("CombatFormula::AreaBGM"), false))
	{
		return Row;
	}
	if (const FBGMConfigRow* Row = Table->FindRow<FBGMConfigRow>(
		FName(TEXT("Default")), TEXT("CombatFormula::AreaBGM"), false))
	{
		return Row;
	}
	for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
	{
		return reinterpret_cast<const FBGMConfigRow*>(Pair.Value);
	}
	return nullptr;
}

const FBGMConfigRow* UCombatFormulaSubsystem::GetEnemyBGMConfigRow(FName EnemyID) const
{
	UDataTable* Table = GetTable(EnemyBGMTable, TEXT("/Game/DataTable/DT_EnemyBGMConfig"));
	if (!Table)
	{
		return nullptr;
	}
	if (const FBGMConfigRow* Row = Table->FindRow<FBGMConfigRow>(EnemyID, TEXT("CombatFormula::EnemyBGM"), false))
	{
		return Row;
	}
	if (const FBGMConfigRow* Row = Table->FindRow<FBGMConfigRow>(
		FName(TEXT("Default")), TEXT("CombatFormula::EnemyBGM"), false))
	{
		return Row;
	}
	for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
	{
		return reinterpret_cast<const FBGMConfigRow*>(Pair.Value);
	}
	return nullptr;
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

	// 武器基值（无武器 = 1.0/0）
	Out.Add(AttributeNames::BlueAttackDamageScale(), 1.0f);
	Out.Add(AttributeNames::WhiteAttackDamageScale(), 1.0f);
	Out.Add(AttributeNames::BlueAttackDamageMod(), 0.0f);
	Out.Add(AttributeNames::WhiteAttackDamageMod(), 0.0f);
	Out.Add(AttributeNames::BlockWindowBonus(), 0.0f);
	Out.Add(AttributeNames::DodgeWindowBonus(), 0.0f);
	Out.Add(AttributeNames::ExtraChargeTurns(), 0.0f);

	// 暴击/增益/技能树被动基值
	Out.Add(AttributeNames::BlueCritChance(), 0.0f);
	Out.Add(AttributeNames::NextAttackDamageScale(), 1.0f);
	Out.Add(AttributeNames::WhiteDmgBonus(), 0.0f);
	Out.Add(AttributeNames::InterruptDmgScale(), 1.0f);
	Out.Add(AttributeNames::BlockDmgReduce(), 0.0f);
	Out.Add(AttributeNames::DodgeBuffBonus(), 0.0f);

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

	// 武器基值（无武器 = 1.0/0）
	Out.Add(AttributeNames::BlueAttackDamageScale(), 1.0f);
	Out.Add(AttributeNames::WhiteAttackDamageScale(), 1.0f);
	Out.Add(AttributeNames::BlueAttackDamageMod(), 0.0f);
	Out.Add(AttributeNames::WhiteAttackDamageMod(), 0.0f);
	Out.Add(AttributeNames::BlockWindowBonus(), 0.0f);
	Out.Add(AttributeNames::DodgeWindowBonus(), 0.0f);
	Out.Add(AttributeNames::ExtraChargeTurns(), 0.0f);

	// 暴击/增益/技能树被动基值
	Out.Add(AttributeNames::BlueCritChance(), 0.0f);
	Out.Add(AttributeNames::NextAttackDamageScale(), 1.0f);
	Out.Add(AttributeNames::WhiteDmgBonus(), 0.0f);
	Out.Add(AttributeNames::InterruptDmgScale(), 1.0f);
	Out.Add(AttributeNames::BlockDmgReduce(), 0.0f);
	Out.Add(AttributeNames::DodgeBuffBonus(), 0.0f);

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

float UCombatFormulaSubsystem::GetAttrFinal(const UAttributeComponent* Attr, FName AttributeName, float Fallback) const
{
	return Attr ? Attr->GetFinal(AttributeName) : Fallback;
}

float UCombatFormulaSubsystem::CalculateWhiteDamage(const UAttributeComponent* Attr, float SkillTreeFlatBonus) const
{
	const FCombatParamsRow Defaults;
	const FCombatParamsRow& P = GetCombatParams() ? *GetCombatParams() : Defaults;

	const float Base = FMath::FRandRange(P.WhiteAttackDamageMin, P.WhiteAttackDamageMax);
	const float CharScale = GetAttrFinal(Attr, AttributeNames::BaseDamageScale(), 1.0f);
	const float WeaponScale = GetAttrFinal(Attr, AttributeNames::WhiteAttackDamageScale(), 1.0f);
	const float MaskScale = GetAttrFinal(Attr, AttributeNames::WhiteDamageScale(), 1.0f);
	const float NextAtkScale = GetAttrFinal(Attr, AttributeNames::NextAttackDamageScale(), 1.0f);
	const float FlatBonus = GetAttrFinal(Attr, AttributeNames::WhiteAttackBonus(), 0.0f)
		+ GetAttrFinal(Attr, AttributeNames::WhiteAttackDamageMod(), 0.0f)
		+ GetAttrFinal(Attr, AttributeNames::WhiteDmgBonus(), 0.0f)
		+ SkillTreeFlatBonus;

	return Base * CharScale * WeaponScale * MaskScale * NextAtkScale + FlatBonus;
}

float UCombatFormulaSubsystem::CalculateBlueDamage(const UAttributeComponent* Attr, float SkillTreeFlatBonus, int32 ChargeStacks) const
{
	const FCombatParamsRow Defaults;
	const FCombatParamsRow& P = GetCombatParams() ? *GetCombatParams() : Defaults;

	const float Base = FMath::FRandRange(P.BlueAttackDamageMin_0Charge, P.BlueAttackDamageMax_0Charge);
	const float CharScale = GetAttrFinal(Attr, AttributeNames::BaseDamageScale(), 1.0f);
	const float WeaponScale = GetAttrFinal(Attr, AttributeNames::BlueAttackDamageScale(), 1.0f);
	const float MaskScale = GetAttrFinal(Attr, AttributeNames::BlueDamageScale(), 1.0f);
	const float NextAtkScale = GetAttrFinal(Attr, AttributeNames::NextAttackDamageScale(), 1.0f);
	const float FlatBonus = GetAttrFinal(Attr, AttributeNames::BlueAttackBonus(), 0.0f)
		+ GetAttrFinal(Attr, AttributeNames::BlueAttackDamageMod(), 0.0f)
		+ SkillTreeFlatBonus;

	const int32 Stacks = FMath::Clamp(ChargeStacks, 0, P.MaxChargeStacks);
	const float ChargeMult = Stacks <= 0 ? 1.0f : (Stacks == 1 ? P.ChargeDamageMultiplier_1 : P.ChargeDamageMultiplier_2);

	float Damage = Base * CharScale * WeaponScale * MaskScale * NextAtkScale * ChargeMult + FlatBonus;

	// 暴击：蓝攻暴击率由技能树/装备提供（如 blue_crit_15 = 15%）
	const float CritChance = GetAttrFinal(Attr, AttributeNames::BlueCritChance(), 0.0f);
	if (CritChance > 0.0f && FMath::FRand() < CritChance)
	{
		Damage *= P.CritDamageMultiplier;
	}

	return Damage;
}

float UCombatFormulaSubsystem::CalculateGoldDamage(const UAttributeComponent* Attr) const
{
	const FCombatParamsRow Defaults;
	const FCombatParamsRow& P = GetCombatParams() ? *GetCombatParams() : Defaults;

	const float Base = FMath::FRandRange(P.GoldAttackDamageMin, P.GoldAttackDamageMax);
	const float CounterBonus = GetAttrFinal(Attr, AttributeNames::CounterDmgBonus(), 0.0f);

	return Base * (1.0f + CounterBonus);
}
