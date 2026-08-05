// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BattleResultHUDWidget.h"
#include "Components/TextBlock.h"

void UBattleResultHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 默认整层隐藏，由 ShowResult 显示（BP 里 ResultText 可再单独控制）
	SetVisibility(ESlateVisibility::Collapsed);
}

void UBattleResultHUDWidget::ShowResult(const FText& Text)
{
	if (ResultText)
	{
		ResultText->SetText(Text);
	}
	SetVisibility(ESlateVisibility::Visible);
}
