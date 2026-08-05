// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "BaseCharacterAnimInstance.generated.h"

class ACharacter;
class UWeaponVisualComponent;

/**
 * UBaseCharacterAnimInstance — 所有角色动画实例的公共基类
 *
 * 统一计算 Speed / Direction / bIsMoving / bIsInAir / VerticalVelocity / GroundDistance。
 * URoleAnimInstance 与 UEnemyAnimInstance 仅作为 Blueprint 兼容壳继承本类，
 * 避免两份几乎相同的逐帧逻辑（DRY）。
 */
UCLASS(Blueprintable)
class HOLE_API UBaseCharacterAnimInstance : public UAnimInstance
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

	/** 垂直速度 (cm/s)，正值=上升，负值=下落 */
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	float VerticalVelocity = 0.0f;

	/** 离地距离 (cm)，仅在空中时通过 Sphere Sweep 计算 */
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	float GroundDistance = 0.0f;

	/** 武器是否已拔到手上（从 UWeaponVisualComponent::IsWeaponDrawn 同步，供 ABP 做持剑 Idle/移动变体） */
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	bool bWeaponDrawn = false;

	/** 同色碰撞准备阶段：由战斗组件置位，ABP 切换到 ClashReady 姿态，结束后复位 */
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	bool bClashReady = false;

	/** 设置 bClashReady（事件驱动，仅战斗组件在 Clash 阶段调用） */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void SetClashReady(bool bReady);

protected:
	/** 缓存的拥有者角色引用 */
	UPROPERTY()
	TObjectPtr<ACharacter> OwnerCharacter;

	/** 缓存的武器显示组件（初始化时取一次，避免每帧组件查找） */
	UPROPERTY(Transient)
	TObjectPtr<UWeaponVisualComponent> CachedWeaponVisual;
};
