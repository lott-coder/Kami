// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/Enemy.h"
#include "Apprentice.generated.h"

/**
 * AApprentice — 低级魔法师（学徒魔法师）
 *
 * 继承自 AEnemy，设置低级魔法师专属的默认属性值。
 * 属性从 DT_EnemyConfig（RowName = "apprentice"）加载。
 *
 * 序章教学版见 AApprenticeCave（RowName = "apprentice_cave"）。
 *
 * @see DataTable_Spec.md §5.4 — 敌人行数据
 */
UCLASS(Blueprintable)
class HOLE_API AApprentice : public AEnemy
{
	GENERATED_BODY()

public:
	AApprentice();

protected:
	virtual void InitializeAttributes() override;
};
