# UDaleAnimInstance — Design Spec

> **Status:** Approved
> **Date:** 2026-07-09
> **Project:** Hole（洞穴）

## Overview

Create `UDaleAnimInstance`, a C++ `UAnimInstance` subclass that acts as the data layer between `ADale` the protagonist character and his Animation Blueprint. The state machine (Idle → Walk/Run → Jump/Fall) lives in the ABP; C++ only exposes the variables the ABP reads.

## Files

| File | Purpose |
|---|---|
| `Hole/Source/Hole/Public/Animation/DaleAnimInstance.h` | Header — class declaration, exposed variables |
| `Hole/Source/Hole/Private/Animation/DaleAnimInstance.cpp` | Implementation — `NativeUpdateAnimation` logic |

## Class Design

```
UAnimInstance
 └── UDaleAnimInstance
```

### Exposed Variables

All `BlueprintReadOnly`, category `Animation`. Updated every frame by `NativeUpdateAnimation`.

| Variable | Type | Description |
|---|---|---|
| `Speed` | `float` | Character velocity magnitude (cm/s) |
| `Direction` | `float` | Movement angle relative to actor forward, in degrees [−180, 180] |
| `bIsMoving` | `bool` | `Speed > 10.0f` threshold |
| `bIsInAir` | `bool` | From `CharacterMovementComponent::IsFalling()` |

### Cached References

- `TObjectPtr<ABaseCharacter> OwnerCharacter` — set in `NativeInitializeAnimation`, refreshed in `NativeUpdateAnimation` in case of pawn change

### Overrides

- `NativeInitializeAnimation()` — cache `OwnerCharacter` from `TryGetPawnOwner()`
- `NativeUpdateAnimation(float DeltaSeconds)` — read `OwnerCharacter` velocity, grounded state, compute direction, write to exposed vars

### Module Dependencies

None new. `Engine` module already provides `UAnimInstance` and `UCharacterMovementComponent`.

## Animation Blueprint Convention (future)

The ABP (`ABP_Dale`) will:
- Inherit from `UDaleAnimInstance`
- Read `Speed`, `Direction`, `bIsMoving`, `bIsInAir` for state transitions
- Use Blend Space for locomotion (mapped from Speed + Direction)
- Transition: Idle ↔ Locomotion, locomotion ↔ In Air
