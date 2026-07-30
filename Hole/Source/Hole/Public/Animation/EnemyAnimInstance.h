// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "EnemyAnimInstance.generated.h"

class AEnemy;

/**
 * UEnemyAnimInstance — 敌人角色（Enemy 分支）的动画数据层
 *
 * 暴露敌人运动状态给 Animation Blueprint。
 * 状态机逻辑在 ABP 中实现，C++ 仅负责更新数据。
 *
 * 所有继承 AEnemy 的角色共用此 AnimInstance。
 */
UCLASS(Blueprintable)
class HOLE_API UEnemyAnimInstance : public UAnimInstance
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

	/** 离地距离 (cm)，通过 Sphere Sweep 计算 */
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	float GroundDistance = 0.0f;

private:
	/** 缓存的拥有者敌人引用 */
	UPROPERTY()
	TObjectPtr<AEnemy> OwnerEnemy;
};
