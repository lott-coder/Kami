// Copyright Epic Games, Inc. All Rights Reserved.

#include "Component/AttributeComponent.h"
#include "DataTable/CharacterConfigTable.h"
#include "DataTable/CombatParamsTable.h"
#include "DataTable/EnemyConfigTable.h"
#include "Engine/DataTable.h"

UAttributeComponent::UAttributeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAttributeComponent::BeginPlay()
{
	Super::BeginPlay();
}

// ---- 初始化 ----

void UAttributeComponent::InitializeFromCharacterConfig(FName CharacterID)
{
	BaseAttributes.Empty();
	ActiveModifiers.Empty();
	bCacheDirty = true;

	// -- 1. 加载角色配置表 --
	static const FString CharDTPath = TEXT("/Game/DataTable/DT_CharacterConfig");
	UDataTable* CharDT = LoadObject<UDataTable>(nullptr, *CharDTPath);
	if (!CharDT)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAttributeComponent::InitFromCharacter - 无法加载 DT_CharacterConfig"));
		return;
	}

	const FCharacterConfigRow* CharRow = CharDT->FindRow<FCharacterConfigRow>(CharacterID, TEXT("AttrComp::Init"));
	if (!CharRow)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAttributeComponent::InitFromCharacter - 找不到角色行: %s"), *CharacterID.ToString());
		return;
	}

	// -- 2. 加载全局战斗参数表 --
	static const FString CombatDTPath = TEXT("/Game/DataTable/DT_CombatParams");
	UDataTable* CombatDT = LoadObject<UDataTable>(nullptr, *CombatDTPath);

	const FCombatParamsRow* CombatRow = nullptr;
	if (CombatDT)
	{
		CombatRow = CombatDT->FindRow<FCombatParamsRow>(FName(TEXT("Default")), TEXT("AttrComp::Init"));
	}

	// -- 3. 从角色行填充属性 --

	// 生存
	SetBase(AttributeNames::MaxHP(),           CharRow->MaxHP);
	SetBase(AttributeNames::MaxSmokeReserve(), CharRow->MaxSmokeReserve);

	// 伤害
	SetBase(AttributeNames::BaseDamageScale(),  CharRow->BaseDamageScale);
	SetBase(AttributeNames::BlueAttackBonus(),  CharRow->BlueAttackBonus);
	SetBase(AttributeNames::WhiteAttackBonus(), CharRow->WhiteAttackBonus);

	// 移动 — 从角色配置读取（不同角色可设不同速度）
	SetBase(AttributeNames::WalkSpeed(),   CharRow->WalkSpeed);
	SetBase(AttributeNames::SprintSpeed(), CharRow->SprintSpeed);
	SetBase(AttributeNames::LandingLockTime(), CharRow->LandingLockTime);

	// -- 4. 从全局战斗参数行填充属性（有表用表，无表用默认值） --

	// 蓄力（MaxChargeStacks 为 int32，需显式转换）
	SetBase(AttributeNames::ChargeSpeedBonus(), 0.0f);
	SetBase(AttributeNames::MaxChargeStacks(),
		CombatRow ? static_cast<float>(CombatRow->MaxChargeStacks) : 2.0f);
	SetBase(AttributeNames::WhiteInterruptChargeDamageScale(),
		CombatRow ? CombatRow->WhiteInterruptChargeDamageScale : 0.3f);

	// 防御/操作
	SetBase(AttributeNames::BlockWindow(),
		CombatRow ? CombatRow->BlockWindowSeconds : 0.25f);
	SetBase(AttributeNames::DodgeWindow(),
		CombatRow ? CombatRow->DodgeWindowSeconds : 0.35f);
	SetBase(AttributeNames::DodgeFailDamageScale(),
		CombatRow ? CombatRow->DodgeFailDamageScale : 1.2f);
	SetBase(AttributeNames::RedPenetrationScale(), 0.0f);

	// 特殊
	SetBase(AttributeNames::DamageTakenScale(),   1.0f);
	SetBase(AttributeNames::CounterDmgBonus(),    0.0f);
	SetBase(AttributeNames::CounterHealPercent(), 0.0f);

	// -- 5. 完成 --
	RebuildCache();

	UE_LOG(LogTemp, Log, TEXT("UAttributeComponent::InitFromCharacter - %s 完成 | MaxHP=%.0f Walk=%.0f Sprint=%.0f BlockWin=%.3f"),
		*CharacterID.ToString(),
		GetFinal(AttributeNames::MaxHP()),
		GetFinal(AttributeNames::WalkSpeed()),
		GetFinal(AttributeNames::SprintSpeed()),
		GetFinal(AttributeNames::BlockWindow()));
}

void UAttributeComponent::InitializeFromEnemyConfig(FName EnemyID)
{
	BaseAttributes.Empty();
	ActiveModifiers.Empty();
	bCacheDirty = true;

	// -- 1. 加载敌人配置表 --
	static const FString EnemyDTPath = TEXT("/Game/DataTable/DT_EnemyConfig");
	UDataTable* EnemyDT = LoadObject<UDataTable>(nullptr, *EnemyDTPath);
	if (!EnemyDT)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAttributeComponent::InitFromEnemy - 无法加载 DT_EnemyConfig"));
		return;
	}

	const FEnemyConfigRow* EnemyRow = EnemyDT->FindRow<FEnemyConfigRow>(EnemyID, TEXT("AttrComp::InitEnemy"));
	if (!EnemyRow)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAttributeComponent::InitFromEnemy - 找不到敌人行: %s"), *EnemyID.ToString());
		return;
	}

	// -- 2. 加载全局战斗参数表 --
	static const FString CombatDTPath = TEXT("/Game/DataTable/DT_CombatParams");
	UDataTable* CombatDT = LoadObject<UDataTable>(nullptr, *CombatDTPath);

	const FCombatParamsRow* CombatRow = nullptr;
	if (CombatDT)
	{
		CombatRow = CombatDT->FindRow<FCombatParamsRow>(FName(TEXT("Default")), TEXT("AttrComp::InitEnemy"));
	}

	// -- 3. 从敌人行填充属性 --

	// 生存
	SetBase(AttributeNames::MaxHP(),           EnemyRow->MaxHP);
	SetBase(AttributeNames::MaxSmokeReserve(), 0.0f);

	// 伤害
	SetBase(AttributeNames::BaseDamageScale(),  EnemyRow->BaseDamageScale);
	SetBase(AttributeNames::BlueAttackBonus(),  0.0f);
	SetBase(AttributeNames::WhiteAttackBonus(), 0.0f);

	// AI
	SetBase(AttributeNames::AIDifficulty(), EnemyRow->AIDifficulty);

	// 移动 — 敌人使用默认值
	SetBase(AttributeNames::WalkSpeed(),       300.0f);
	SetBase(AttributeNames::SprintSpeed(),     600.0f);
	SetBase(AttributeNames::LandingLockTime(), 0.3f);

	// -- 4. 从全局战斗参数行填充属性 --

	SetBase(AttributeNames::ChargeSpeedBonus(), 0.0f);
	SetBase(AttributeNames::MaxChargeStacks(),
		CombatRow ? static_cast<float>(CombatRow->MaxChargeStacks) : 2.0f);
	SetBase(AttributeNames::WhiteInterruptChargeDamageScale(),
		CombatRow ? CombatRow->WhiteInterruptChargeDamageScale : 0.3f);

	SetBase(AttributeNames::BlockWindow(),
		CombatRow ? CombatRow->BlockWindowSeconds : 0.25f);
	SetBase(AttributeNames::DodgeWindow(),
		CombatRow ? CombatRow->DodgeWindowSeconds : 0.35f);
	SetBase(AttributeNames::DodgeFailDamageScale(),
		CombatRow ? CombatRow->DodgeFailDamageScale : 1.2f);
	SetBase(AttributeNames::RedPenetrationScale(), 0.0f);

	SetBase(AttributeNames::DamageTakenScale(),   1.0f);
	SetBase(AttributeNames::CounterDmgBonus(),    0.0f);
	SetBase(AttributeNames::CounterHealPercent(), 0.0f);

	// -- 5. 完成 --
	RebuildCache();

	UE_LOG(LogTemp, Log, TEXT("UAttributeComponent::InitFromEnemy - %s 完成 | MaxHP=%.0f DmgScale=%.2f AIDiff=%.2f BlockWin=%.3f"),
		*EnemyID.ToString(),
		GetFinal(AttributeNames::MaxHP()),
		GetFinal(AttributeNames::BaseDamageScale()),
		EnemyRow->AIDifficulty,
		GetFinal(AttributeNames::BlockWindow()));
}

// ---- 属性读取 ----

float UAttributeComponent::GetFinal(FName AttributeName) const
{
	if (bCacheDirty)
	{
		const_cast<UAttributeComponent*>(this)->RebuildCache();
	}

	if (const float* Found = CachedFinalAttributes.Find(AttributeName))
	{
		return *Found;
	}

	if (const float* Base = BaseAttributes.Find(AttributeName))
	{
		return *Base;
	}

	UE_LOG(LogTemp, Warning, TEXT("UAttributeComponent::GetFinal - 未知属性: %s"), *AttributeName.ToString());
	return 0.0f;
}

float UAttributeComponent::GetBase(FName AttributeName) const
{
	if (const float* Found = BaseAttributes.Find(AttributeName))
	{
		return *Found;
	}
	return 0.0f;
}

void UAttributeComponent::SetBase(FName AttributeName, float Value)
{
	BaseAttributes.FindOrAdd(AttributeName) = Value;
	bCacheDirty = true;
}

// ---- 修正器管理 ----

void UAttributeComponent::AddModifier(FName AttributeName, EModifierOp Op, float Value, int32 Turns, FName SourceTag)
{
	FAttributeModifier& Mod = ActiveModifiers.AddDefaulted_GetRef();
	Mod.AttributeName = AttributeName;
	Mod.Op = Op;
	Mod.Value = Value;
	Mod.RemainingTurns = Turns;
	Mod.SourceTag = SourceTag;

	bCacheDirty = true;
}

void UAttributeComponent::RemoveModifiersBySource(FName SourceTag)
{
	ActiveModifiers.RemoveAll([SourceTag](const FAttributeModifier& Mod)
	{
		return Mod.SourceTag == SourceTag;
	});
	bCacheDirty = true;
}

void UAttributeComponent::RemoveAllTemporaryModifiers()
{
	ActiveModifiers.RemoveAll([](const FAttributeModifier& Mod)
	{
		return !Mod.IsPermanent();
	});
	bCacheDirty = true;
}

void UAttributeComponent::TickTurn()
{
	bool bRemoved = false;

	for (int32 i = ActiveModifiers.Num() - 1; i >= 0; --i)
	{
		FAttributeModifier& Mod = ActiveModifiers[i];
		if (!Mod.IsPermanent())
		{
			Mod.RemainingTurns -= 1;
			if (Mod.IsExpired())
			{
				ActiveModifiers.RemoveAt(i);
				bRemoved = true;
			}
		}
	}

	if (bRemoved)
	{
		bCacheDirty = true;
	}
}

// ---- 内部 ----

void UAttributeComponent::RebuildCache()
{
	CachedFinalAttributes.Reset();

	for (const auto& Pair : BaseAttributes)
	{
		CachedFinalAttributes.Add(Pair.Key, ComputeFinal(Pair.Key));
	}

	bCacheDirty = false;
}

float UAttributeComponent::ComputeFinal(FName AttributeName) const
{
	const float* Base = BaseAttributes.Find(AttributeName);
	if (!Base)
	{
		return 0.0f;
	}

	float AddAccum = 0.0f;
	float MulAccum = 1.0f;

	for (const FAttributeModifier& Mod : ActiveModifiers)
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

	return (*Base + AddAccum) * MulAccum;
}
