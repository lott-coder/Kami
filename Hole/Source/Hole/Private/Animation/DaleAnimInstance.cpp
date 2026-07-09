// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/DaleAnimInstance.h"
#include "Character/BaseCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

void UDaleAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if (AActor* Owner = GetOwningActor())
	{
		OwnerCharacter = Cast<ABaseCharacter>(Owner);
	}
}

void UDaleAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwnerCharacter)
	{
		return;
	}

	// ---- Speed ----
	const FVector Velocity = OwnerCharacter->GetVelocity();
	Speed = Velocity.Size2D();

	// ---- Direction ----
	if (Speed > 0.0f)
	{
		const FRotator ActorRotation = OwnerCharacter->GetActorRotation();
		const FVector VelocityDirection = Velocity.GetSafeNormal2D();
		const FVector ForwardVector = ActorRotation.Vector();

		const float Dot = FVector::DotProduct(ForwardVector, VelocityDirection);
		const float Cross = FVector::CrossProduct(ForwardVector, VelocityDirection).Z;

		Direction = FMath::RadiansToDegrees(FMath::Atan2(Cross, Dot));
	}
	else
	{
		Direction = 0.0f;
	}

	// ---- bIsMoving ----
	bIsMoving = Speed > 10.0f;

	// ---- bIsInAir ----
	if (const UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement())
	{
		bIsInAir = MovementComp->IsFalling();
	}
}
