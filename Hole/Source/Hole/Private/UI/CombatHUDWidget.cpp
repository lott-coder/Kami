// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/CombatHUDWidget.h"
#include "Combat/BattleComponent.h"
#include "Character/BaseCharacter.h"
#include "Character/Enemy.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

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

	// 额外回合：只允许 蓝攻/蓄力；技能 v1 始终禁用
	const bool bExtra = Battle->IsPlayerExtraTurn();
	SetChoiceButtonsEnabled(!bExtra, true, !bExtra, true, false);
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
	if (Battle.IsValid()) Battle->PlayerChooseAction(EBattleAction::RedDefense);
}

void UCombatHUDWidget::OnBlueAttackClicked()
{
	if (Battle.IsValid()) Battle->PlayerChooseAction(EBattleAction::BlueAttack);
}

void UCombatHUDWidget::OnWhiteAttackClicked()
{
	if (Battle.IsValid()) Battle->PlayerChooseAction(EBattleAction::WhiteAttack);
}

void UCombatHUDWidget::OnChargeClicked()
{
	if (Battle.IsValid()) Battle->PlayerChooseAction(EBattleAction::Charge);
}

void UCombatHUDWidget::OnSkillClicked()
{
	if (Battle.IsValid()) Battle->PlayerChooseAction(EBattleAction::Skill);
}
