// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/EnemyAnimInstance.h"
#include "Character/Enemy.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "WorldCollision.h"
#include "CollisionQueryParams.h"

void UEnemyAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if (AActor* Owner = GetOwningActor())
	{
		OwnerEnemy = Cast<AEnemy>(Owner);
	}
}

void UEnemyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwnerEnemy)
	{
		return;
	}

	// ---- Speed ----
	const FVector Velocity = OwnerEnemy->GetVelocity();
	Speed = Velocity.Size2D();

	// ---- Direction ----
	if (Speed > 0.0f)
	{
		const FRotator ActorRotation = OwnerEnemy->GetActorRotation();
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
	if (const UCharacterMovementComponent* MovementComp = OwnerEnemy->GetCharacterMovement())
	{
		bIsInAir = MovementComp->IsFalling();
	}

	// ---- VerticalVelocity ----
	VerticalVelocity = Velocity.Z;

	// ---- GroundDistance ----
	// 仅在空中才扫描（落地后距离恒为 0），避免每帧物理查询
	if (bIsInAir)
	{
		const UWorld* World = OwnerEnemy->GetWorld();
		if (World)
		{
			const UCapsuleComponent* Capsule = OwnerEnemy->GetCapsuleComponent();
			const float CapsuleHalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 88.0f;
			const float CapsuleRadius      = Capsule ? Capsule->GetScaledCapsuleRadius()      : 34.0f;

			const FVector SweepStart = OwnerEnemy->GetActorLocation()
				- FVector::UpVector * CapsuleHalfHeight;
			const FVector SweepEnd   = SweepStart - FVector::UpVector * 2000.0f;

			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(OwnerEnemy);

			const FCollisionShape SphereShape = FCollisionShape::MakeSphere(CapsuleRadius);

			FHitResult HitResult;
			const bool bHit = World->SweepSingleByChannel(
				HitResult,
				SweepStart,
				SweepEnd,
				FQuat::Identity,
				ECC_Visibility,
				SphereShape,
				QueryParams
			);

			GroundDistance = bHit ? HitResult.Distance : 2000.0f;
		}
		else
		{
			GroundDistance = 0.0f;
		}
	}
	else
	{
		GroundDistance = 0.0f;
	}
}
