// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Consumable.generated.h"

/**
 * AConsumable — 消耗品实例（占位基类）
 *
 * 当前仅用于 DT_ConsumableConfig 的 ConsumableClass 列引用；
 * 消耗品系统落地时在此实现使用效果（即时效果或 AddModifier）。
 */
UCLASS(Abstract, Blueprintable)
class HOLE_API AConsumable : public AActor
{
	GENERATED_BODY()
};
