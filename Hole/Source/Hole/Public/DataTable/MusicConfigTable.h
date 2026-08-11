// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MusicConfigTable.generated.h"

class USoundBase;

/**
 * FBGMConfigRow - 背景音乐配置行（16 号表 DT_AreaBGMConfig / 17 号表 DT_EnemyBGMConfig 共用）
 *
 * 非战斗（探索）BGM 按区域行（hole/town/market/mansion/border/hell）；
 * 战斗 BGM 按敌人行（enemy ID），不同敌人可配不同音乐。
 * 纯数据：不跨表查找、不含运行逻辑；由 UBGMComponent 按战斗状态加载播放。
 * @see DataTable_Spec.md §18/§19 背景音乐配置
 */
USTRUCT(BlueprintType)
struct HOLE_API FBGMConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 背景音乐资产（Wave/SoundCue/MetaSound 等 USoundBase 派生）；空 = 不播放 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BGM")
	TSoftObjectPtr<USoundBase> Music;

	/** 播放音量（0~1） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BGM")
	float Volume = 1.0f;

	/** 是否循环 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BGM")
	bool bLoop = true;

	/** 淡入时长（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BGM")
	float FadeInTime = 1.0f;

	/** 淡出时长（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BGM")
	float FadeOutTime = 1.0f;
};
