// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "BattleResultHUDWidget.generated.h"

class UTextBlock;
class UWidget;

/** 玩家点击结算界面继续（胜利收尾/失败重开由 UBattleComponent 监听） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBattleResultContinue);

/**
 * UBattleResultHUDWidget - 战斗结算 HUD C++ 基类（独立于战斗 HUD）
 *
 * 战斗结束时由 UBattleComponent 创建（默认 /Game/UI/HUD/WBP_BattleResult）；
 * C++ 负责结果文字/奖励栏/点击继续，BP 只做布局与皮肤。
 * 设计口径（2026-08-11）：
 *  - 横幅只有胜利/失败两态文字（教学逃跑归为胜利）；
 *  - 胜利结算仅显示"道具（烟+道具）"与"装备"两个栏位；
 *  - 结算后不自动收尾，点击屏幕继续（右下角"继续前进"提示）。
 */
UCLASS(Blueprintable)
class HOLE_API UBattleResultHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UBattleResultHUDWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** 点击屏幕 / 按 Enter / 空格 继续 */
	UPROPERTY(BlueprintAssignable, Category = "BattleResult")
	FOnBattleResultContinue OnContinueClicked;

	/** 显示失败结果（无奖励栏） */
	UFUNCTION(BlueprintCallable, Category = "BattleResult")
	void ShowResult(const FText& Text);

	/** 显示胜利结果：道具栏（烟+道具）与装备栏两个栏位 */
	UFUNCTION(BlueprintCallable, Category = "BattleResult")
	void ShowVictoryResult(const FText& Text, const FText& ItemRewards, const FText& EquipmentRewards);

protected:
	/** 结果文字（胜利/失败） */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ResultText;

	/** 右下角"继续前进"提示（可选） */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ContinueHintText;

	/** 胜利奖励容器（可选；ShowVictoryResult 时显示） */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RewardPanel;

	/** 道具栏文字（烟与道具合并展示；可选） */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemRewardText;

	/** 装备栏文字（可选） */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EquipmentRewardText;

private:
	void BroadcastContinue();
};
