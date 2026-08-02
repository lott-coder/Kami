// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Combat/BattleTypes.h"
#include "CombatHUDWidget.generated.h"

class UBattleComponent;
class UProgressBar;
class UTextBlock;
class UButton;
class UWidget;

/**
 * UCombatHUDWidget - 战斗 HUD C++ 基类
 *
 * 所有控件通过 BindWidget 绑定 WBP_CombatHUD 中同名字面量；
 * C++ 负责数据刷新与按钮回调，BP 只做布局/皮肤。
 */
UCLASS(Blueprintable)
class HOLE_API UCombatHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "CombatHUD")
	void BindToBattle(UBattleComponent* InBattle);

	UFUNCTION(BlueprintCallable, Category = "CombatHUD")
	void SetChoiceButtonsEnabled(bool bRed, bool bBlue, bool bWhite, bool bCharge, bool bSkill);

	UFUNCTION(BlueprintCallable, Category = "CombatHUD")
	void ShowClashPrompt(const FText& Text, float TotalTime);

	UFUNCTION(BlueprintCallable, Category = "CombatHUD")
	void HideClashPrompt();

	UFUNCTION(BlueprintCallable, Category = "CombatHUD")
	void ShowResult(const FText& Text);

	UFUNCTION(BlueprintCallable, Category = "CombatHUD")
	void HideResult();

protected:
	// ---- BindWidget：名称必须与 WBP_CombatHUD 中的控件名完全一致 ----

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> PlayerHealthBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> EnemyHealthBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PlayerNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EnemyNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> RoundText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PlayerChargeText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EnemyChargeText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> RedDefenseButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BlueAttackButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> WhiteAttackButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ChargeButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SkillButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> ClashPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ClashPromptText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> ClashWindowBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> ResultPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ResultText;

private:
	UFUNCTION()
	void OnRedDefenseClicked();

	UFUNCTION()
	void OnBlueAttackClicked();

	UFUNCTION()
	void OnWhiteAttackClicked();

	UFUNCTION()
	void OnChargeClicked();

	UFUNCTION()
	void OnSkillClicked();

	UFUNCTION()
	void HandleBattleStateChanged();

	void RefreshAll();

	TWeakObjectPtr<UBattleComponent> Battle;
	float ClashTotalTime = 0.0f;
	float ClashRemainingTime = 0.0f;
	bool bClashActive = false;
};
