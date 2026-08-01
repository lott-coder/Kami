// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/BaseCharacterAnimInstance.h"
#include "RoleAnimInstance.generated.h"

/**
 * URoleAnimInstance — 玩家角色（Role 分支）的动画数据层
 *
 * 继承 UBaseCharacterAnimInstance，全部计算逻辑复用公共基类。
 * 保留此类仅为兼容现有 Animation Blueprint（如 ABP_Dale）。
 */
UCLASS(Blueprintable)
class HOLE_API URoleAnimInstance : public UBaseCharacterAnimInstance
{
	GENERATED_BODY()
};
