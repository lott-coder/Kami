// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/CombatHUDWidget.h"
#include "Combat/BattleComponent.h"
#include "Character/BaseCharacter.h"
#include "Character/Enemy.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "UObject/Class.h"

void UCombatHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (RedDefenseButton)
	{
		RedDefenseButton->OnClicked.AddDynamic(this, &UCombatHUDWidget::OnRedDefenseClicked);
	}
	if (BlueAttackButton)
	{
		BlueAttackButton->OnClicked.AddDynamic(this, &UCombatHUDWidget::OnBlueAttackClicked);
	}
	if (WhiteAttackButton)
	{
		WhiteAttackButton->OnClicked.AddDynamic(this, &UCombatHUDWidget::OnWhiteAttackClicked);
	}
	if (ChargeButton)
	{
		ChargeButton->OnClicked.AddDynamic(this, &UCombatHUDWidget::OnChargeClicked);
	}
	if (SkillButton)
	{
		SkillButton->OnClicked.AddDynamic(this, &UCombatHUDWidget::OnSkillClicked);
	}

	// 缩放中心设为按钮中心，悬停放大时围绕中心变化
	TArray<UButton*> ActionButtons = { RedDefenseButton, BlueAttackButton, WhiteAttackButton, ChargeButton, SkillButton };
	for (UButton* Btn : ActionButtons)
	{
		if (Btn)
		{
			Btn->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		}
	}

	BindHoverEffects();
	HideEnemyActionHint();
	CollapseTutorialHintPanel();
	if (TutorialHintOutAnim)
	{
		FWidgetAnimationDynamicEvent FinishedEvent;
		FinishedEvent.BindDynamic(this, &UCombatHUDWidget::OnTutorialHintOutFinished);
		BindToAnimationFinished(TutorialHintOutAnim, FinishedEvent);
	}
	if (TutorialHintClashOutAnim)
	{
		FWidgetAnimationDynamicEvent ClashOutFinishedEvent;
		ClashOutFinishedEvent.BindDynamic(this, &UCombatHUDWidget::OnTutorialHintOutFinished);
		BindToAnimationFinished(TutorialHintClashOutAnim, ClashOutFinishedEvent);
	}
	UpdateTutorialHint();
}

void UCombatHUDWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EnemyHintTimer);
	}
	Super::NativeDestruct();
}

void UCombatHUDWidget::BindToBattle(UBattleComponent* InBattle)
{
	Battle = InBattle;
	if (Battle.IsValid())
	{
		Battle->OnBattleStateChanged.AddDynamic(this, &UCombatHUDWidget::HandleBattleStateChanged);
	}
	RefreshAll();
}

void UCombatHUDWidget::HandleBattleStateChanged()
{
	if (Battle.IsValid())
	{
		const EBattlePhase CurrentPhase = Battle->GetBattlePhase();
		const bool bRoundAdvanced = Battle->GetRoundNumber() != EnemyHintRound;

		// 敌方额外回合：进入 Resolving 时展示敌方该回合行动
		if (CurrentPhase == EBattlePhase::Resolving && LastSeenPhase != EBattlePhase::Resolving)
		{
			ShowEnemyActionHint();
		}
		else if (bRoundAdvanced
			|| CurrentPhase == EBattlePhase::Ended
			|| (Battle->IsPlayerExtraTurn() && !Battle->HasPlayerChosenAction()))
		{
			HideEnemyActionHint();
		}
		LastSeenPhase = CurrentPhase;
	}
	RefreshAll();
}

void UCombatHUDWidget::RefreshAll()
{
	if (!Battle.IsValid())
	{
		return;
	}

	ABaseCharacter* Player = Battle->GetPlayerCharacter();
	AEnemy* Enemy = Battle->GetBossEnemy();

	if (PlayerHealthBar && Player)
	{
		PlayerHealthBar->SetPercent(Player->GetHealthPercent());
	}
	if (EnemyHealthBar && Enemy)
	{
		EnemyHealthBar->SetPercent(Enemy->GetHealthPercent());
	}
	if (PlayerNameText && Player)
	{
		PlayerNameText->SetText(FText::FromName(Player->CharacterID));
	}
	if (EnemyNameText && Enemy)
	{
		EnemyNameText->SetText(FText::FromName(Enemy->EnemyID));
	}
	if (RoundText)
	{
		RoundText->SetText(FText::Format(FText::FromString(TEXT("回合 {0}")), FText::AsNumber(Battle->GetRoundNumber())));
	}
	if (PlayerChargeText)
	{
		PlayerChargeText->SetText(FText::Format(FText::FromString(TEXT("蓄力 {0}")), FText::AsNumber(Battle->GetPlayerChargeStacks())));
	}
	if (EnemyChargeText)
	{
		EnemyChargeText->SetText(FText::Format(FText::FromString(TEXT("蓄力 {0}")), FText::AsNumber(Battle->GetEnemyChargeStacks())));
	}

	UpdateTutorialHint();

	// 额外回合：只允许 蓝攻/蓄力；技能 v1 始终禁用
	const bool bExtra = Battle->IsPlayerExtraTurn();
	const bool bCanBlueAttack = Battle->GetPlayerChargeStacks() > 0;
	const bool bCanCharge = Battle->GetPlayerChargeStacks() < Battle->GetPlayerMaxChargeStacks();

	// 教学点锁定：条件教学点回合只开放指定行动按钮，其余锁定
	const EBattleAction LockedAction = Battle->GetTutorialLockedPlayerAction();
	if (LockedAction != EBattleAction::None)
	{
		SetChoiceButtonsEnabled(
			LockedAction == EBattleAction::RedDefense,
			LockedAction == EBattleAction::BlueAttack,
			LockedAction == EBattleAction::WhiteAttack,
			LockedAction == EBattleAction::Charge,
			false);
		return;
	}

	SetChoiceButtonsEnabled(!bExtra, bCanBlueAttack, !bExtra, bCanCharge, false);
}

void UCombatHUDWidget::SetChoiceButtonsEnabled(bool bRed, bool bBlue, bool bWhite, bool bCharge, bool bSkill)
{
	if (RedDefenseButton) RedDefenseButton->SetIsEnabled(bRed);
	if (BlueAttackButton) BlueAttackButton->SetIsEnabled(bBlue);
	if (WhiteAttackButton) WhiteAttackButton->SetIsEnabled(bWhite);
	if (ChargeButton) ChargeButton->SetIsEnabled(bCharge);
	if (SkillButton) SkillButton->SetIsEnabled(bSkill);
}

void UCombatHUDWidget::OnRedDefenseClicked()
{
	ChooseAction(EBattleAction::RedDefense);
}

void UCombatHUDWidget::OnBlueAttackClicked()
{
	ChooseAction(EBattleAction::BlueAttack);
}

void UCombatHUDWidget::OnWhiteAttackClicked()
{
	ChooseAction(EBattleAction::WhiteAttack);
}

void UCombatHUDWidget::OnChargeClicked()
{
	ChooseAction(EBattleAction::Charge);
}

void UCombatHUDWidget::OnSkillClicked()
{
	ChooseAction(EBattleAction::Skill);
}

void UCombatHUDWidget::ChooseAction(EBattleAction Action)
{
	if (!Battle.IsValid())
	{
		return;
	}
	// 必须在调用前记录是否额外回合：PlayerChooseAction 内部会清除额外回合标记；
	// 只显示敌方出招——玩家额外回合敌方不出招，不显示提示；普通回合展示敌方出招
	const bool bWasExtraTurn = Battle->IsPlayerExtraTurn();
	if (Battle->PlayerChooseAction(Action))
	{
		if (bWasExtraTurn)
		{
			// 玩家额外回合敌方不出招：不显示任何提示
			HideEnemyActionHint();
		}
		else
		{
			ShowEnemyActionHint();
		}
	}
}

void UCombatHUDWidget::ShowEnemyActionHint()
{
	if (!Battle.IsValid() || !EnemyActionHintText)
	{
		return;
	}

	const EBattleAction EnemyAction = Battle->GetEnemyChosenAction();
	if (EnemyAction == EBattleAction::None)
	{
		HideEnemyActionHint();
		return;
	}

	SetHintText(FText::Format(
		FText::FromString(TEXT("敌方出招：{0}")),
		StaticEnum<EBattleAction>()->GetDisplayNameTextByValue(static_cast<int64>(EnemyAction))));
}

void UCombatHUDWidget::SetHintText(const FText& Text)
{
	if (!EnemyActionHintText)
	{
		return;
	}
	EnemyActionHintText->SetText(Text);
	EnemyActionHintText->SetVisibility(ESlateVisibility::Visible);
	EnemyHintRound = Battle.IsValid() ? Battle->GetRoundNumber() : -1;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EnemyHintTimer);
		if (EnemyHintDuration > 0.0f)
		{
			World->GetTimerManager().SetTimer(EnemyHintTimer, this, &UCombatHUDWidget::OnEnemyHintTimeout, EnemyHintDuration, false);
		}
	}
}

void UCombatHUDWidget::OnEnemyHintTimeout()
{
	HideEnemyActionHint();
}

void UCombatHUDWidget::HideEnemyActionHint()
{
	if (EnemyActionHintText)
	{
		EnemyActionHintText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EnemyHintTimer);
	}
}

void UCombatHUDWidget::UpdateTutorialHint()
{
	if (!TutorialHintText)
	{
		return;
	}

	bool bShow = false;
	bool bExtraTurnSelect = false;
	if (Battle.IsValid() && Battle->IsTutorialBattle())
	{
		const EBattlePhase Phase = Battle->GetBattlePhase();
		// 行动选择阶段：显示当前教学点；玩家额外回合显示"只能蓝攻/蓄力"引导
		// 碰撞阶段：显示格挡/闪避操作引导
		bExtraTurnSelect = Battle->IsPlayerExtraTurn() && !Battle->HasPlayerChosenAction();
		bShow = ((Phase == EBattlePhase::ActionSelect && !Battle->HasPlayerChosenAction()
					&& (bExtraTurnSelect || !Battle->IsPlayerExtraTurn()))
			|| (Phase == EBattlePhase::Clash && !Battle->IsClashResolved()));
	}

	const FText HintText = bExtraTurnSelect
		? FText::FromString(TEXT("额外回合：只能选择蓝攻或蓄力！"))
		: (Battle.IsValid() ? Battle->GetTutorialHintText() : FText::GetEmpty());
	const bool bDesiredShow = bShow && !HintText.IsEmpty();
	if (bDesiredShow)
	{
		ShowTutorialHint(HintText);
	}
	else
	{
		// 无提示：完整退出（进入有提示的回合时 In，玩家确认行动/碰撞结束/战斗结束 Out）
		HideTutorialHintFull();
	}
}

void UCombatHUDWidget::ShowTutorialHint(const FText& Text)
{
	if (CurrentTutorialHint.EqualTo(Text) && bTutorialHintBackgroundVisible
		&& TutorialHintText->GetRenderOpacity() > 0.01f)
	{
		// 同一提示持续显示：不重播
		return;
	}

	if (TutorialHintOutAnim) StopAnimation(TutorialHintOutAnim);
	if (TutorialHintInAnim) StopAnimation(TutorialHintInAnim);
	if (TutorialHintClashInAnim) StopAnimation(TutorialHintClashInAnim);
	if (TutorialHintClashOutAnim) StopAnimation(TutorialHintClashOutAnim);

	TutorialHintText->SetText(Text);
	CurrentTutorialHint = Text;

	// 渲染数值（平移/缩放/透明度/锚点）全部由 WBP 动画关键帧定义，C++ 只负责可见性与播放
	if (TutorialHintPanel)
	{
		TutorialHintPanel->SetVisibility(ESlateVisibility::Visible);
	}
	TutorialHintText->SetVisibility(ESlateVisibility::Visible);
	bTutorialHintBackgroundVisible = true;

	// 碰撞提示（回合中触发）使用专用入场动画（从屏幕左侧出现）；未配置时回退普通入场
	const bool bClashHint = Battle.IsValid() && Battle->GetBattlePhase() == EBattlePhase::Clash && !Battle->IsClashResolved();
	bCurrentHintIsClash = bClashHint;
	UWidgetAnimation* const InAnim = bClashHint
		? (TutorialHintClashInAnim ? TutorialHintClashInAnim : TutorialHintInAnim)
		: TutorialHintInAnim;
	if (InAnim)
	{
		PlayAnimation(InAnim);
	}
}

void UCombatHUDWidget::HideTutorialHintFull()
{
	if (!bTutorialHintBackgroundVisible)
	{
		return;
	}
	if (TutorialHintInAnim) StopAnimation(TutorialHintInAnim);
	if (TutorialHintClashInAnim) StopAnimation(TutorialHintClashInAnim);
	// 碰撞提示使用专用退出动画（向屏幕左侧消失）；未配置时回退通用退出
	UWidgetAnimation* const OutAnim = bCurrentHintIsClash && TutorialHintClashOutAnim
		? TutorialHintClashOutAnim
		: TutorialHintOutAnim;
	if (OutAnim)
	{
		PlayAnimation(OutAnim);
		// 结束后由 OnTutorialHintOutFinished 折叠面板
	}
	else
	{
		CollapseTutorialHintPanel();
	}
}

void UCombatHUDWidget::OnTutorialHintOutFinished()
{
	CollapseTutorialHintPanel();
}

void UCombatHUDWidget::CollapseTutorialHintPanel()
{
	if (TutorialHintPanel)
	{
		TutorialHintPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (TutorialHintText)
	{
		TutorialHintText->SetVisibility(ESlateVisibility::Collapsed);
	}
	bTutorialHintBackgroundVisible = false;
	bCurrentHintIsClash = false;
	CurrentTutorialHint = FText::GetEmpty();
}

void UCombatHUDWidget::BindHoverEffects()
{
	if (RedDefenseButton)
	{
		RedDefenseButton->OnHovered.AddDynamic(this, &UCombatHUDWidget::OnRedDefenseHovered);
		RedDefenseButton->OnUnhovered.AddDynamic(this, &UCombatHUDWidget::OnRedDefenseUnhovered);
	}
	if (BlueAttackButton)
	{
		BlueAttackButton->OnHovered.AddDynamic(this, &UCombatHUDWidget::OnBlueAttackHovered);
		BlueAttackButton->OnUnhovered.AddDynamic(this, &UCombatHUDWidget::OnBlueAttackUnhovered);
	}
	if (WhiteAttackButton)
	{
		WhiteAttackButton->OnHovered.AddDynamic(this, &UCombatHUDWidget::OnWhiteAttackHovered);
		WhiteAttackButton->OnUnhovered.AddDynamic(this, &UCombatHUDWidget::OnWhiteAttackUnhovered);
	}
	if (ChargeButton)
	{
		ChargeButton->OnHovered.AddDynamic(this, &UCombatHUDWidget::OnChargeHovered);
		ChargeButton->OnUnhovered.AddDynamic(this, &UCombatHUDWidget::OnChargeUnhovered);
	}
	if (SkillButton)
	{
		SkillButton->OnHovered.AddDynamic(this, &UCombatHUDWidget::OnSkillHovered);
		SkillButton->OnUnhovered.AddDynamic(this, &UCombatHUDWidget::OnSkillUnhovered);
	}
}

void UCombatHUDWidget::ApplyHoverScale(UButton* Button, bool bHovered)
{
	if (!Button)
	{
		return;
	}
	const float Scale = bHovered ? FMath::Max(1.0f, HoverScale) : 1.0f;
	Button->SetRenderScale(FVector2D(Scale, Scale));
}

void UCombatHUDWidget::OnRedDefenseHovered() { ApplyHoverScale(RedDefenseButton, true); }
void UCombatHUDWidget::OnRedDefenseUnhovered() { ApplyHoverScale(RedDefenseButton, false); }
void UCombatHUDWidget::OnBlueAttackHovered() { ApplyHoverScale(BlueAttackButton, true); }
void UCombatHUDWidget::OnBlueAttackUnhovered() { ApplyHoverScale(BlueAttackButton, false); }
void UCombatHUDWidget::OnWhiteAttackHovered() { ApplyHoverScale(WhiteAttackButton, true); }
void UCombatHUDWidget::OnWhiteAttackUnhovered() { ApplyHoverScale(WhiteAttackButton, false); }
void UCombatHUDWidget::OnChargeHovered() { ApplyHoverScale(ChargeButton, true); }
void UCombatHUDWidget::OnChargeUnhovered() { ApplyHoverScale(ChargeButton, false); }
void UCombatHUDWidget::OnSkillHovered() { ApplyHoverScale(SkillButton, true); }
void UCombatHUDWidget::OnSkillUnhovered() { ApplyHoverScale(SkillButton, false); }
