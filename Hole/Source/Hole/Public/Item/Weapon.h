// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Weapon.generated.h"

/**
 * AWeapon — 武器实例（占位基类）
 *
 * 当前仅用于 DT_WeaponConfig 的 WeaponClass 列引用；
 * 武器系统落地时在此实现实例化、装备槽与战斗表现。
 */
UCLASS(Abstract, Blueprintable)
class HOLE_API AWeapon : public AActor
{
	GENERATED_BODY()
};
