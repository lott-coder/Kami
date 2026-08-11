// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BattleResultHUDWidget.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"

UBattleResultHUDWidget::UBattleResultHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UBattleResultHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 默认整层隐藏，由 ShowResult / ShowVictoryResult 显示
	SetVisibility(ESlateVisibility::Collapsed);
}

void UBattleResultHUDWidget::ShowResult(const FText& Text)
{
	if (ResultText)
	{
		ResultText->SetText(Text);
	}
	if (RewardPanel)
	{
		RewardPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ContinueHintText)
	{
		ContinueHintText->SetVisibility(ESlateVisibility::Visible);
	}
	SetVisibility(ESlateVisibility::Visible);
}

void UBattleResultHUDWidget::ShowVictoryResult(const FText& Text, const FText& ItemRewards, const FText& EquipmentRewards)
{
	if (ResultText)
	{
		ResultText->SetText(Text);
	}
	if (ItemRewardText)
	{
		ItemRewardText->SetText(ItemRewards);
	}
	if (EquipmentRewardText)
	{
		EquipmentRewardText->SetText(EquipmentRewards);
	}
	if (RewardPanel)
	{
		RewardPanel->SetVisibility(ESlateVisibility::Visible);
	}
	if (ContinueHintText)
	{
		ContinueHintText->SetVisibility(ESlateVisibility::Visible);
	}
	SetVisibility(ESlateVisibility::Visible);
}

FReply UBattleResultHUDWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	BroadcastContinue();
	return FReply::Handled();
}

FReply UBattleResultHUDWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Enter || Key == EKeys::SpaceBar)
	{
		BroadcastContinue();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UBattleResultHUDWidget::BroadcastContinue()
{
	if (OnContinueClicked.IsBound())
	{
		OnContinueClicked.Broadcast();
	}
}
