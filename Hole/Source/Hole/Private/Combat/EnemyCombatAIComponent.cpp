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
		return (ChargeStacks > 0 && FMath::RandBool()) ? EBattleAction::BlueAttack : EBattleAction::Charge;
	}

	// 蓝攻需要至少 1 层蓄力；其余行动始终可选
	TArray<EBattleAction> Options;
	Options.Add(EBattleAction::RedDefense);
	Options.Add(EBattleAction::WhiteAttack);
	Options.Add(EBattleAction::Charge);
	if (ChargeStacks > 0)
	{
		Options.Add(EBattleAction::BlueAttack);
	}
	return Options[FMath::RandRange(0, Options.Num() - 1)];
}
