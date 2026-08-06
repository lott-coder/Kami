// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifies/Combat/AnimNotify_CombatDamage.h"
#include "Combat/BattleComponent.h"
#include "Character/BaseCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

void UAnimNotify_CombatDamage::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}
	AActor* Attacker = MeshComp->GetOwner();
	UWorld* World = MeshComp->GetWorld();
	if (!World)
	{
		return;
	}
	APlayerController* PC = World->GetFirstPlayerController();
	APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
	if (!PlayerPawn)
	{
		return;
	}
	if (UBattleComponent* Battle = PlayerPawn->FindComponentByClass<UBattleComponent>())
	{
		Battle->OnHitNotify(Cast<ABaseCharacter>(Attacker), EventName);
	}
}
