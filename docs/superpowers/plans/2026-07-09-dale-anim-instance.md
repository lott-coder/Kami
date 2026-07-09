# Dale AnimInstance — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create `UDaleAnimInstance` C++ class as data layer between `ADale` and his Animation Blueprint.

**Architecture:** `UAnimInstance` subclass that caches the owning `ABaseCharacter` and exposes `Speed`, `Direction`, `bIsMoving`, `bIsInAir` every frame via `NativeUpdateAnimation`.

**Tech Stack:** Unreal Engine 5.6 C++ — `UAnimInstance`, `ABaseCharacter`, `UCharacterMovementComponent`.

## Global Constraints

- Module: `Hole` — no new Build.cs dependencies needed (`Engine` provides all types)
- File paths: `Hole/Source/Hole/Public/Animation/` and `Hole/Source/Hole/Private/Animation/`
- `ABaseCharacter` is the lowest common type for character access

---

### Task 1: Create UDaleAnimInstance header

**Files:**
- Create: `Hole/Source/Hole/Public/Animation/DaleAnimInstance.h`

**Produces:** `UDaleAnimInstance` class declaration with `Speed`, `Direction`, `bIsMoving`, `bIsInAir` + `OwnerCharacter` cache + `NativeInitializeAnimation`/`NativeUpdateAnimation` overrides.

- [ ] **Step 1: Write the header**

```cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "DaleAnimInstance.generated.h"

class ABaseCharacter;

/**
 * UDaleAnimInstance — 主角 Dale 的动画数据层
 *
 * 暴露角色运动状态给 Animation Blueprint。
 * 状态机逻辑在 ABP 中实现，C++ 仅负责更新数据。
 */
UCLASS(Blueprintable)
class HOLE_API UDaleAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** 移动速度 (cm/s) */
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	float Speed = 0.0f;

	/** 移动方向（相对于角色朝向，-180° ~ 180°） */
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	float Direction = 0.0f;

	/** 是否正在移动 */
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	bool bIsMoving = false;

	/** 是否在空中 */
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	bool bIsInAir = false;

private:
	/** 缓存的拥有者角色引用 */
	UPROPERTY()
	TObjectPtr<ABaseCharacter> OwnerCharacter;
};
```

- [ ] **Step 2: Commit**

```bash
git add Hole/Source/Hole/Public/Animation/DaleAnimInstance.h
git commit -m "feat: add UDaleAnimInstance header"
```

---

### Task 2: Create UDaleAnimInstance implementation

**Files:**
- Create: `Hole/Source/Hole/Private/Animation/DaleAnimInstance.cpp`

**Interfaces:**
- Consumes: `UDaleAnimInstance` from Task 1, `ABaseCharacter` (existing), `UCharacterMovementComponent` (UE engine)

- [ ] **Step 1: Write the implementation**

```cpp
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
```

- [ ] **Step 2: Commit**

```bash
git add Hole/Source/Hole/Private/Animation/DaleAnimInstance.cpp
git commit -m "feat: add UDaleAnimInstance implementation"
```

---

### Task 3: Compile and verify

- [ ] **Step 1: Build the project**

```bash
cd "D:/Program Files/Epic Games/UE_5.6/Engine/Build/BatchFiles" && ./RunUAT.bat BuildEditor -project="d:/UE5/UE_project/Kami/Hole/Hole.uproject" -platform=Win64 -configuration=Development -notools
```

Expected: Build succeeds with 0 errors.

- [ ] **Step 2: Verify in editor**

Open the project in UE Editor, confirm:
- `UDaleAnimInstance` appears in the class list when creating a new Animation Blueprint
- Can create an ABP inheriting from `UDaleAnimInstance`
- Variables (`Speed`, `Direction`, `bIsMoving`, `bIsInAir`) appear in the ABP's variables panel
