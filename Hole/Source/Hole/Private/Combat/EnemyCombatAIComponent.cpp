// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/EnemyCombatAIComponent.h"

UEnemyCombatAIComponent::UEnemyCombatAIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

EBattleAction UEnemyCombatAIComponent::ChooseAction(int32 RoundNumber, EBattleAction LastPlayerAction, bool bExtraTurn, int32 ChargeStacks) const
{
	if (bExtraTurn)
	{
		// GDD 5.2.4：额外回合只能 出蓝刀 或 继续蓄力
		return FMath::RandBool() ? EBattleAction::BlueAttack : EBattleAction::Charge;
	}

	switch (FMath::RandRange(0, 3))
	{
	case 0:
		return EBattleAction::RedDefense;
	case 1:
		return EBattleAction::BlueAttack;
	case 2:
		return EBattleAction::WhiteAttack;
	default:
		return EBattleAction::Charge;
	}
}
