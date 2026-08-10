// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "Animation/WidgetAnimation.h"
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
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "CombatHUD")
	void BindToBattle(UBattleComponent* InBattle);

	UFUNCTION(BlueprintCallable, Category = "CombatHUD")
	void SetChoiceButtonsEnabled(bool bRed, bool bBlue, bool bWhite, bool bCharge, bool bSkill);

	/** 鼠标悬停按钮时的缩放倍率（1.0 = 关闭 C++ 悬停效果，可改用 BP 动画） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatHUD")
	float HoverScale = 1.1f;

	/** 敌方出招提示的显示时长（秒），<=0 表示一直显示到回合切换 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatHUD")
	float EnemyHintDuration = 3.0f;

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

	/** 敌方出招提示文字（WBP 中可选，缺失时该功能静默跳过） */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EnemyActionHintText;

	/** 教学引导提示文字（WBP 中可选，缺失时教学提示静默跳过） */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TutorialHintText;

	/** 教学提示背景容器（WBP 中可选；入场/退场动画控制它的缩放与透明度） */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> TutorialHintPanel;

	/** 教学提示完整入场：背景右侧向左展开 + 文字渐显（WBP 中可选绑定，名字须一致） */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> TutorialHintInAnim;

	/** 碰撞提示专用入场：提示框从屏幕左侧出现（回合中触发的特殊提示，未配置时回退 TutorialHintInAnim） */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> TutorialHintClashInAnim;

	/** 教学提示完整退出：文字淡出 + 背景收缩（无连续提示/战斗结束时） */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> TutorialHintOutAnim;

	/** 碰撞提示专用退出：提示框向屏幕左侧消失（未配置时回退 TutorialHintOutAnim） */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> TutorialHintClashOutAnim;

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
	void OnRedDefenseHovered();

	UFUNCTION()
	void OnRedDefenseUnhovered();

	UFUNCTION()
	void OnBlueAttackHovered();

	UFUNCTION()
	void OnBlueAttackUnhovered();

	UFUNCTION()
	void OnWhiteAttackHovered();

	UFUNCTION()
	void OnWhiteAttackUnhovered();

	UFUNCTION()
	void OnChargeHovered();

	UFUNCTION()
	void OnChargeUnhovered();

	UFUNCTION()
	void OnSkillHovered();

	UFUNCTION()
	void OnSkillUnhovered();

	UFUNCTION()
	void HandleBattleStateChanged();

	void ShowEnemyActionHint();
	void SetHintText(const FText& Text);
	void HideEnemyActionHint();
	void UpdateTutorialHint();
	/** 显示提示：背景未显示→完整入场（背景展开+文字渐显）；背景已显示→仅文字渐显 */
	void ShowTutorialHint(const FText& Text);
	/** 完整隐藏（文字淡出 + 背景收缩，结束后折叠面板） */
	void HideTutorialHintFull();
	/** 完整退场动画结束后回调：折叠面板并复位 */
	UFUNCTION()
	void OnTutorialHintOutFinished();
	/** 立即折叠教学提示面板并复位渲染状态 */
	void CollapseTutorialHintPanel();
	void ChooseAction(EBattleAction Action);

	UFUNCTION()
	void OnEnemyHintTimeout();

	void RefreshAll();
	void BindHoverEffects();
	void ApplyHoverScale(UButton* Button, bool bHovered);

	TWeakObjectPtr<UBattleComponent> Battle;
	FTimerHandle EnemyHintTimer;
	int32 EnemyHintRound = -1;
	EBattlePhase LastSeenPhase = EBattlePhase::Idle;
	bool bTutorialHintBackgroundVisible = false;
	bool bCurrentHintIsClash = false;
	FText CurrentTutorialHint;
};
