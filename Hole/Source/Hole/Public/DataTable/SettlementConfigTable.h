// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "SettlementConfigTable.generated.h"

/**
 * FSettlementConfigRow - DT_SettlementConfig 行结构（14 号表，单行 Default）
 *
 * 结算链路数据：死亡结算（延迟冻结/停帧/镜头转动）参数。
 * 纯数据：不跨表查找、不含运行逻辑；由 UBattleComponent 进入战斗时加载。
 * @see DataTable_Spec.md §16 结算配置
 */
USTRUCT(BlueprintType)
struct HOLE_API FSettlementConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	/** HP 清空后延迟冻结时长（秒）；期间世界时间正常，受击方正常播放 Hurt */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settlement")
	float FreezeDelay = 1.0f;

	/** 摄像机转动时长（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settlement")
	float CameraDuration = 1.0f;

	/** 摄像机转动到位后的停留时长（秒），随后弹出结算界面 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settlement")
	float HoldDuration = 0.5f;

	/** 摄像机偏航转动量（度，策划可调） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settlement")
	float CameraYawOffset = 15.0f;

	/** 摄像机俯仰转动量（度，策划可调） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settlement")
	float CameraPitchOffset = -3.0f;

};
