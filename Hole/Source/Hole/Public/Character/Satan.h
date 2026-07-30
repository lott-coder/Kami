// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/Enemy.h"
#include "Satan.generated.h"

/**
 * ASatan — 最终 Boss "撒旦"（Satan）
 *
 * 继承自 AEnemy，设置撒旦专属的默认属性值。
 * 属性从 DT_EnemyConfig（RowName = "satan"）加载。
 *
 * @see DataTable_Spec.md §5.4 — 敌人行数据第7行
 */
UCLASS(Blueprintable)
class HOLE_API ASatan : public AEnemy
{
	GENERATED_BODY()

public:
	ASatan();

protected:
	virtual void InitializeAttributes() override;
};
