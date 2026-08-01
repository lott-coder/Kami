// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/RoleAnimInstance.h"
#include "Character/Role.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "WorldCollision.h"
#include "CollisionQueryParams.h"

void URoleAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if (AActor* Owner = GetOwningActor())
	{
		OwnerRole = Cast<ARole>(Owner);
	}
}

void URoleAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwnerRole)
	{
		return;
	}

	// ---- Speed ----
	const FVector Velocity = OwnerRole->GetVelocity();
	Speed = Velocity.Size2D();

	// ---- Direction ----
	if (Speed > 0.0f)
	{
		const FRotator ActorRotation = OwnerRole->GetActorRotation();
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
	if (const UCharacterMovementComponent* MovementComp = OwnerRole->GetCharacterMovement())
	{
		bIsInAir = MovementComp->IsFalling();
	}

	// ---- VerticalVelocity ----
	// 正值=上升, 负值=下落, 用作 InAir Blend Space 1D 输入轴
	VerticalVelocity = Velocity.Z;

	// ---- GroundDistance (落地预测) ----
	// 使用 Sphere Sweep（球体扫描）而非单点 LineTrace
	// 球体半径 = 胶囊体半径，解决胶囊体悬在平台边缘时中心点 Trace 打空的问题。
	// 仅在空中才扫描（落地后距离恒为 0），避免每帧物理查询。
	if (bIsInAir)
	{
		const UWorld* World = OwnerRole->GetWorld();
		if (World)
		{
			const UCapsuleComponent* Capsule = OwnerRole->GetCapsuleComponent();
			const float CapsuleHalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 88.0f;
			const float CapsuleRadius      = Capsule ? Capsule->GetScaledCapsuleRadius()      : 34.0f;

			// 扫描起点：脚底
			const FVector SweepStart = OwnerRole->GetActorLocation()
				- FVector::UpVector * CapsuleHalfHeight;
			const FVector SweepEnd   = SweepStart - FVector::UpVector * 2000.0f;

			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(OwnerRole);

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
