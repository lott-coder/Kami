// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/BaseCharacterAnimInstance.h"
#include "EnemyAnimInstance.generated.h"

/**
 * UEnemyAnimInstance — 敌人角色（Enemy 分支）的动画数据层
 *
 * 继承 UBaseCharacterAnimInstance，全部计算逻辑复用公共基类。
 * 保留此类仅为兼容现有 Animation Blueprint。
 */
UCLASS(Blueprintable)
class HOLE_API UEnemyAnimInstance : public UBaseCharacterAnimInstance
{
	GENERATED_BODY()
};
