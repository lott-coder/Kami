// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_CombatMarker.generated.h"

/**
 * 战斗标记通知：标注无伤害关键帧（如红防的 GuardReady 举剑帧），
 * 仅供预排计算扫描，Notify 不触发战斗逻辑。
 */
UCLASS()
class HOLE_API UAnimNotify_CombatMarker : public UAnimNotify
{
	GENERATED_BODY()

public:
	/** 标记名，如 GuardReady */
	UPROPERTY(EditAnywhere, Category = "Combat")
	FName MarkerName;
};
