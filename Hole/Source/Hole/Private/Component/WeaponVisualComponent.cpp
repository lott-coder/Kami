// Copyright Epic Games, Inc. All Rights Reserved.

#include "Component/WeaponVisualComponent.h"

#include "Character/BaseCharacter.h"
#include "Component/InventoryComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DataTable/WeaponConfigTable.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Subsystem/CombatFormulaSubsystem.h"

UWeaponVisualComponent::UWeaponVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(this);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetGenerateOverlapEvents(false);
	WeaponMesh->SetHiddenInGame(true);
}

void UWeaponVisualComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ABaseCharacter* Character = Cast<ABaseCharacter>(GetOwner()))
	{
		if (UInventoryComponent* Inventory = Character->InventoryComponent)
		{
			Inventory->OnWeaponChanged.AddDynamic(this, &UWeaponVisualComponent::HandleWeaponChanged);
		}
	}

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (USkeletalMeshComponent* CharacterMesh = Character->GetMesh())
		{
			const FName SocketName = BackSocketName.IsNone() ? TEXT("weapon_back") : BackSocketName;
			if (CharacterMesh->DoesSocketExist(SocketName))
			{
				AttachToComponent(CharacterMesh, FAttachmentTransformRules::SnapToTargetIncludingScale, SocketName);
			}
			else
			{
				SetRelativeTransform(BackAttachOffset);
				AttachToComponent(CharacterMesh, FAttachmentTransformRules::KeepRelativeTransform);
				UE_LOG(LogTemp, Warning, TEXT("UWeaponVisualComponent::BeginPlay - socket '%s' not found on '%s', fallback to mesh root"),
					*SocketName.ToString(), *GetNameSafe(CharacterMesh->GetSkeletalMeshAsset()));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("UWeaponVisualComponent::BeginPlay - owner '%s' has no skeletal mesh, weapon visual disabled"),
				*GetNameSafe(GetOwner()));
		}
	}

	RefreshWeaponVisual();
}

void UWeaponVisualComponent::SetWeaponVisible(bool bShow)
{
	bWeaponVisible = bShow;
	if (WeaponMesh)
	{
		WeaponMesh->SetHiddenInGame(!bWeaponVisible || WeaponMesh->GetStaticMesh() == nullptr);
	}
}

void UWeaponVisualComponent::RefreshWeaponVisual()
{
	ABaseCharacter* Character = Cast<ABaseCharacter>(GetOwner());
	UInventoryComponent* Inventory = Character ? Character->InventoryComponent : nullptr;
	if (!Inventory || !WeaponMesh)
	{
		return;
	}

	const FName WeaponID = Inventory->GetEquippedWeaponID();
	UStaticMesh* Mesh = nullptr;

	if (WeaponID != NAME_None)
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				if (UCombatFormulaSubsystem* Subsystem = GameInstance->GetSubsystem<UCombatFormulaSubsystem>())
				{
					if (const FWeaponConfigRow* Row = Subsystem->GetWeaponRow(WeaponID))
					{
						Mesh = Row->MeshAsset.LoadSynchronous();
						if (!Mesh)
						{
							UE_LOG(LogTemp, Warning, TEXT("UWeaponVisualComponent::RefreshWeaponVisual - weapon '%s' MeshAsset missing"),
								*WeaponID.ToString());
						}
					}
				}
			}
		}
	}

	WeaponMesh->SetStaticMesh(Mesh);
	WeaponMesh->SetHiddenInGame(!bWeaponVisible || Mesh == nullptr);
}

void UWeaponVisualComponent::HandleWeaponChanged(FName WeaponID)
{
	RefreshWeaponVisual();
}
