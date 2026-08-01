// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Skill.generated.h"

/**
 * ASkill — 技能实例（占位基类）
 *
 * 当前仅用于 DT_SkillConfig 的 SkillClass 列引用；
 * 技能系统落地时在此实现执行逻辑（效果通过 AddModifier 体现）。
 */
UCLASS(Abstract, Blueprintable)
class HOLE_API ASkill : public AActor
{
	GENERATED_BODY()
};
