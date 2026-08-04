// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifies/Entrance/AnimNotify_Hold.h"

#include "Character/BaseCharacter.h"
#include "Component/WeaponVisualComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_Hold::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	ABaseCharacter* Character = MeshComp ? Cast<ABaseCharacter>(MeshComp->GetOwner()) : nullptr;
	if (!Character)
	{
		return;
	}

	if (UWeaponVisualComponent* WeaponVisual = Character->WeaponVisualComponent)
	{
		WeaponVisual->AttachWeaponToSocket(WeaponVisual->HandSocketName);
	}
}

FString UAnimNotify_Hold::GetNotifyName_Implementation() const
{
	return TEXT("Hold");
}
