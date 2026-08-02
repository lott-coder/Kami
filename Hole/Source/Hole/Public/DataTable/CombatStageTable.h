// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CombatStageTable.generated.h"

/**
 * FCombatStageRow - DT_BattleStage 的行结构体
 *
 * 战斗舞台配置（策划调参）：玩家/敌人站位、朝向与战斗固定摄像机。
 * 单例模式：整张表只有一行（RowName = "Default"）。
 * 非战斗状态由 UBattleComponent 在战斗开始时保存、结束时恢复，不受本表影响。
 *
 * @see DataTable_Spec.md 战斗舞台配置
 */
USTRUCT(BlueprintType)
struct HOLE_API FCombatStageRow : public FTableRowBase
{
	GENERATED_BODY()

	// ---- 站位 ----

	/** 玩家相对 Boss 位置的偏移（世界空间，含 Z 高度差），默认在 Boss 前方 550cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage|Staging")
	FVector PlayerBattleOffset = FVector(0.0f, -550.0f, 0.0f);

	/** 开战时 Boss 是否转向玩家 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage|Staging")
	bool bBossFacePlayer = true;

	/** 开战时玩家是否面向 Boss */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage|Staging")
	bool bPlayerFaceBoss = true;

	// ---- 战斗固定摄像机 ----

	/** 镜头俯仰角（负值=略俯视） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage|Camera")
	float CameraPitch = -12.0f;

	/** 镜头偏航偏移（相对 玩家→Boss 方向，用于微调构图） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage|Camera")
	float CameraYawOffset = 0.0f;

	/** 战斗时 SpringArm 长度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage|Camera")
	float CameraArmLength = 400.0f;
};
