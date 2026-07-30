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

	/** 垂直速度 (cm/s)，正值=上升，负值=下落。用作 InAir Blend Space 1D 的输入轴 */
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	float VerticalVelocity = 0.0f;

	/** 离地距离 (cm)，通过 LineTrace 计算。用于落地预测，提前混合 Jump Land */
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	float GroundDistance = 0.0f;

private:
	/** 缓存的拥有者角色引用 */
	UPROPERTY()
	TObjectPtr<ABaseCharacter> OwnerCharacter;
};
