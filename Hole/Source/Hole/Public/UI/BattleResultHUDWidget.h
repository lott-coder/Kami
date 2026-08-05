// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleResultHUDWidget.generated.h"

class UTextBlock;

/**
 * UBattleResultHUDWidget - 战斗结算 HUD C++ 基类（独立于战斗 HUD）
 *
 * 战斗结束时由 UBattleComponent 创建（默认 /Game/UI/HUD/WBP_BattleResult）；
 * C++ 负责显示结果文字，BP 负责布局/皮肤（胜利/失败横幅、按钮等后续扩展）。
 */
UCLASS(Blueprintable)
class HOLE_API UBattleResultHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	/** 显示结果文字（胜利/失败）并显示整层 HUD */
	UFUNCTION(BlueprintCallable, Category = "BattleResult")
	void ShowResult(const FText& Text);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ResultText;
};
