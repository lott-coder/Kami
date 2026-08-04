// Copyright Epic Games, Inc. All Rights Reserved.

#include "Component/InventoryComponent.h"
#include "Character/BaseCharacter.h"
#include "Component/AttributeComponent.h"
#include "DataTable/MaskConfigTable.h"
#include "DataTable/WeaponConfigTable.h"
#include "Subsystem/CombatFormulaSubsystem.h"
#include "Engine/GameInstance.h"

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UInventoryComponent::EquipMask(FName MaskID)
{
	if (MaskID == NAME_None || MaskID == EquippedMaskID)
	{
		return false;
	}

	UCombatFormulaSubsystem* Subsystem = GetCombatSubsystem();
	if (!Subsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("UInventoryComponent::EquipMask - 无法获取 UCombatFormulaSubsystem"));
		return false;
	}

	const FMaskConfigRow* Row = Subsystem->GetMaskRow(MaskID);
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("UInventoryComponent::EquipMask - 找不到面具行: %s"), *MaskID.ToString());
		return false;
	}

	// 先卸下旧面具，再装备新面具
	UnequipMask();

	EquippedMaskID = MaskID;
	ApplyMaskModifiers(*Row, true);
	return true;
}

void UInventoryComponent::UnequipMask()
{
	if (!HasMaskEquipped())
	{
		return;
	}

	const FName OldTag = MakeMaskSourceTag(EquippedMaskID);
	EquippedMaskID = NAME_None;

	if (ABaseCharacter* Character = Cast<ABaseCharacter>(GetOwner()))
	{
		if (Character->AttributeComponent)
		{
			Character->AttributeComponent->RemoveModifiersBySource(OldTag);
		}
	}
}

void UInventoryComponent::ApplyMaskModifiers(const FMaskConfigRow& Row, bool bApply)
{
	ABaseCharacter* Character = Cast<ABaseCharacter>(GetOwner());
	if (!Character || !Character->AttributeComponent)
	{
		return;
	}

	UAttributeComponent* Attr = Character->AttributeComponent;
	const FName SourceTag = MakeMaskSourceTag(EquippedMaskID);

	if (bApply)
	{
		// 倍率类（默认 1.0，Multiply）
		Attr->AddModifier(AttributeNames::SmokeGainScale(), EModifierOp::Multiply, Row.SmokeGainScale, 0, SourceTag);
		Attr->AddModifier(AttributeNames::RedDamageScale(), EModifierOp::Multiply, Row.ColorDamageScale_Red, 0, SourceTag);
		Attr->AddModifier(AttributeNames::BlueDamageScale(), EModifierOp::Multiply, Row.ColorDamageScale_Blue, 0, SourceTag);
		Attr->AddModifier(AttributeNames::WhiteDamageScale(), EModifierOp::Multiply, Row.ColorDamageScale_White, 0, SourceTag);
		Attr->AddModifier(AttributeNames::SkillCostScale(), EModifierOp::Multiply, Row.SkillCostScale, 0, SourceTag);

		// 加法类（百分比，默认 0.0，Add）
		Attr->AddModifier(AttributeNames::HPRegenOnKill(), EModifierOp::Add, Row.HPRegenOnKill, 0, SourceTag);
	}
	else
	{
		Attr->RemoveModifiersBySource(SourceTag);
	}
}

bool UInventoryComponent::EquipWeapon(FName WeaponID)
{
	if (WeaponID == NAME_None || WeaponID == EquippedWeaponID)
	{
		return false;
	}

	UCombatFormulaSubsystem* Subsystem = GetCombatSubsystem();
	if (!Subsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("UInventoryComponent::EquipWeapon - 无法获取 UCombatFormulaSubsystem"));
		return false;
	}

	const FWeaponConfigRow* Row = Subsystem->GetWeaponRow(WeaponID);
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("UInventoryComponent::EquipWeapon - 找不到武器行: %s"), *WeaponID.ToString());
		return false;
	}

	// 先卸下旧武器，再装备新武器
	UnequipWeapon();

	EquippedWeaponID = WeaponID;
	ApplyWeaponModifiers(*Row, true);
	OnWeaponChanged.Broadcast(EquippedWeaponID);
	return true;
}

void UInventoryComponent::UnequipWeapon()
{
	if (!HasWeaponEquipped())
	{
		return;
	}

	const FName OldTag = MakeWeaponSourceTag(EquippedWeaponID);
	EquippedWeaponID = NAME_None;

	if (ABaseCharacter* Character = Cast<ABaseCharacter>(GetOwner()))
	{
		if (Character->AttributeComponent)
		{
			Character->AttributeComponent->RemoveModifiersBySource(OldTag);
		}
	}

	OnWeaponChanged.Broadcast(NAME_None);
}

void UInventoryComponent::ApplyWeaponModifiers(const FWeaponConfigRow& Row, bool bApply)
{
	ABaseCharacter* Character = Cast<ABaseCharacter>(GetOwner());
	if (!Character || !Character->AttributeComponent)
	{
		return;
	}

	UAttributeComponent* Attr = Character->AttributeComponent;
	const FName SourceTag = MakeWeaponSourceTag(EquippedWeaponID);

	if (bApply)
	{
		// 倍率类（默认 1.0，Multiply）
		Attr->AddModifier(AttributeNames::BlueAttackDamageScale(), EModifierOp::Multiply, Row.BlueAttackDamageScale, 0, SourceTag);
		Attr->AddModifier(AttributeNames::WhiteAttackDamageScale(), EModifierOp::Multiply, Row.WhiteAttackDamageScale, 0, SourceTag);

		// 加法类（默认 0.0，Add）
		Attr->AddModifier(AttributeNames::BlueAttackDamageMod(), EModifierOp::Add, Row.BlueAttackDamageMod, 0, SourceTag);
		Attr->AddModifier(AttributeNames::WhiteAttackDamageMod(), EModifierOp::Add, Row.WhiteAttackDamageMod, 0, SourceTag);
		Attr->AddModifier(AttributeNames::BlockWindowBonus(), EModifierOp::Add, Row.BlockWindowBonus, 0, SourceTag);
		Attr->AddModifier(AttributeNames::DodgeWindowBonus(), EModifierOp::Add, Row.DodgeWindowBonus, 0, SourceTag);
		Attr->AddModifier(AttributeNames::RedPenetrationScale(), EModifierOp::Add, Row.RedPenetrationScale, 0, SourceTag);
		Attr->AddModifier(AttributeNames::ExtraChargeTurns(), EModifierOp::Add, static_cast<float>(Row.ExtraChargeTurns), 0, SourceTag);
	}
	else
	{
		Attr->RemoveModifiersBySource(SourceTag);
	}
}

FName UInventoryComponent::MakeWeaponSourceTag(FName WeaponID) const
{
	return FName(*FString::Printf(TEXT("Weapon_%s"), *WeaponID.ToString()));
}

FName UInventoryComponent::MakeMaskSourceTag(FName MaskID) const
{
	return FName(*FString::Printf(TEXT("Mask_%s"), *MaskID.ToString()));
}

UCombatFormulaSubsystem* UInventoryComponent::GetCombatSubsystem() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UCombatFormulaSubsystem>() : nullptr;
}
