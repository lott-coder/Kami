// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/Apprentice.h"
#include "ApprenticeCave.generated.h"

/**
 * AApprenticeCave — 序章教学低级魔法师（洞穴，仅第 0 次轮回）
 *
 * 低级魔法师的教学变体：EnemyID = "apprentice_cave"（DT_EnemyConfig 专用行），
 * 战斗组件/敌人 AI 据此自动启用教学模式（玩家先制 + 7 回合教学脚本 + 锁血必逃）。
 *
 * @see DataTable_Spec.md §5.4 — 敌人行数据（apprentice_cave 行）
 * @see Plans/tutorial-enemy.md — 序章教学敌人设计方案
 */
UCLASS(Blueprintable)
class HOLE_API AApprenticeCave : public AApprentice
{
	GENERATED_BODY()

public:
	AApprenticeCave();

protected:
	virtual void InitializeAttributes() override;
};
