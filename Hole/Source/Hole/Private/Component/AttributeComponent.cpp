// Copyright Epic Games, Inc. All Rights Reserved.

#include "Component/AttributeComponent.h"
#include "Subsystem/CombatFormulaSubsystem.h"
#include "Engine/GameInstance.h"

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

	UCombatFormulaSubsystem* Subsystem = GetCombatSubsystem();
	if (!Subsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAttributeComponent::InitializeFromCharacterConfig - 无法获取 UCombatFormulaSubsystem"));
		return;
	}

	// 跨表合并（DT_CharacterConfig + DT_CombatParams + 默认值）由子系统完成
	TMap<FName, float> Attributes = Subsystem->BuildCharacterAttributes(CharacterID);
	if (Attributes.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("UAttributeComponent::InitializeFromCharacterConfig - 未生成属性: %s"), *CharacterID.ToString());
		return;
	}

	BaseAttributes = MoveTemp(Attributes);
	RebuildCache();

	UE_LOG(LogTemp, Log, TEXT("UAttributeComponent::InitializeFromCharacterConfig - %s 完成 | MaxHP=%.0f Walk=%.0f Sprint=%.0f BlockWin=%.3f"),
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

	UCombatFormulaSubsystem* Subsystem = GetCombatSubsystem();
	if (!Subsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAttributeComponent::InitializeFromEnemyConfig - 无法获取 UCombatFormulaSubsystem"));
		return;
	}

	// 跨表合并（DT_EnemyConfig + DT_CombatParams + 默认值）由子系统完成
	TMap<FName, float> Attributes = Subsystem->BuildEnemyAttributes(EnemyID);
	if (Attributes.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("UAttributeComponent::InitializeFromEnemyConfig - 未生成属性: %s"), *EnemyID.ToString());
		return;
	}

	BaseAttributes = MoveTemp(Attributes);
	RebuildCache();

	UE_LOG(LogTemp, Log, TEXT("UAttributeComponent::InitializeFromEnemyConfig - %s 完成 | MaxHP=%.0f DmgScale=%.2f AIDiff=%.2f BlockWin=%.3f"),
		*EnemyID.ToString(),
		GetFinal(AttributeNames::MaxHP()),
		GetFinal(AttributeNames::BaseDamageScale()),
		GetFinal(AttributeNames::AIDifficulty()),
		GetFinal(AttributeNames::BlockWindow()));
}

// ---- 属性读取 ----

float UAttributeComponent::GetFinal(FName AttributeName) const
{
	if (bCacheDirty)
	{
		RebuildCache();
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

void UAttributeComponent::RebuildCache() const
{
	CachedFinalAttributes.Reset();

	const UCombatFormulaSubsystem* Subsystem = GetCombatSubsystem();
	for (const auto& Pair : BaseAttributes)
	{
		if (Subsystem)
		{
			CachedFinalAttributes.Add(Pair.Key, Subsystem->CalculateFinalValue(Pair.Value, ActiveModifiers, Pair.Key));
		}
		else
		{
			// 无 GameInstance（如编辑器预览）：退化为基值
			CachedFinalAttributes.Add(Pair.Key, Pair.Value);
		}
	}

	bCacheDirty = false;
}

UCombatFormulaSubsystem* UAttributeComponent::GetCombatSubsystem() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UCombatFormulaSubsystem>() : nullptr;
}
