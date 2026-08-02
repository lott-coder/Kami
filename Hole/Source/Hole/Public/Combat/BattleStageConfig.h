// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BattleStageConfig.generated.h"

/**
 * UBattleStageConfig - 战斗舞台配置（策划调参用，共享资产，不按 Boss 区分）
 *
 * 玩家/敌人站位、朝向与战斗固定摄像机的所有数值集中在这里；
 * 非战斗状态由 UBattleComponent 在战斗开始时保存、结束时恢复，不受本资产影响。
 */
UCLASS(BlueprintType)
class HOLE_API UBattleStageConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	// ---- 站位 ----

	/** 玩家相对 Boss 位置的偏移（世界空间，含 Z 高度差），默认在 Boss 前方 550cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Staging")
	FVector PlayerBattleOffset = FVector(0.0f, -550.0f, 0.0f);

	/** 开战时 Boss 是否转向玩家 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Staging")
	bool bBossFacePlayer = true;

	/** 开战时玩家是否面向 Boss */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Staging")
	bool bPlayerFaceBoss = true;

	// ---- 战斗固定摄像机 ----

	/** 镜头俯仰角（负值=略俯视） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float CameraPitch = -12.0f;

	/** 镜头偏航偏移（相对 玩家→Boss 方向，用于微调构图） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float CameraYawOffset = 0.0f;

	/** 战斗时 SpringArm 长度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float CameraArmLength = 400.0f;
};
