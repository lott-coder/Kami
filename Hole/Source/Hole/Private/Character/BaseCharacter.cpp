// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/BaseCharacter.h"
#include "Component/AttributeComponent.h"
#include "Component/InventoryComponent.h"
#include "DataTable/CharacterConfigTable.h"
#include "Subsystem/CombatFormulaSubsystem.h"
#include "Engine/GameInstance.h"

ABaseCharacter::ABaseCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	// 身份默认值
	CharacterID = NAME_None;
	ElementalColor = EElementalColor::None;

	// 创建属性容器组件
	AttributeComponent = CreateDefaultSubobject<UAttributeComponent>(TEXT("AttributeComponent"));

	// 创建物品/装备容器组件
	InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));

	// 运行时状态（不再是硬编码 100 —— 由 InitializeAttributes() 从 DataTable 加载后覆盖）
	CurrentHealth = 1.0f;
}

void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 若子类尚未调用 InitializeAttributes，在此处兜底
	if (!bAttributesInitialized)
	{
		InitializeAttributes();
	}
}

void ABaseCharacter::InitializeAttributes()
{
	// 基类实现：从 DataTable 加载属性，重置生命值
	if (AttributeComponent && CharacterID != NAME_None)
	{
		AttributeComponent->InitializeFromCharacterConfig(CharacterID);
		CurrentHealth = GetMaxHealth();
		bAttributesInitialized = true;

		// 自动装备默认面具（DT_CharacterConfig.DefaultMaskID）
		if (InventoryComponent)
		{
			if (UGameInstance* GameInstance = GetGameInstance())
			{
				if (UCombatFormulaSubsystem* Subsystem = GameInstance->GetSubsystem<UCombatFormulaSubsystem>())
				{
					if (const FCharacterConfigRow* Row = Subsystem->GetCharacterRow(CharacterID))
					{
						if (Row->DefaultMaskID != NAME_None)
						{
							InventoryComponent->EquipMask(Row->DefaultMaskID);
						}
					}
				}
			}
		}
	}
}

void ABaseCharacter::ReceiveDamage(float DamageAmount, AActor* DamageCauser)
{
	if (IsDead() || DamageAmount <= 0.0f)
	{
		return;
	}

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - DamageAmount);

	if (CurrentHealth <= 0.0f)
	{
		OnDeath();
	}
}

void ABaseCharacter::OnDeath()
{
	// 子类重写以实现自定义死亡逻辑（播放动画、销毁 Actor、触发事件等）
}

bool ABaseCharacter::IsDead() const
{
	return CurrentHealth <= 0.0f;
}

float ABaseCharacter::GetHealthPercent() const
{
	const float MaxHP = GetMaxHealth();
	if (MaxHP <= 0.0f)
	{
		return 0.0f;
	}
	return CurrentHealth / MaxHP;
}

void ABaseCharacter::Heal(float HealAmount)
{
	if (IsDead() || HealAmount <= 0.0f)
	{
		return;
	}

	CurrentHealth = FMath::Min(GetMaxHealth(), CurrentHealth + HealAmount);
}

float ABaseCharacter::GetMaxHealth() const
{
	if (AttributeComponent)
	{
		return AttributeComponent->GetFinal(AttributeNames::MaxHP());
	}
	return CurrentHealth; // 回退：没有 AttributeComponent 时（不应该发生）
}
