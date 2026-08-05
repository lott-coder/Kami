// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/EnemyCombatAIComponent.h"
#include "Character/Enemy.h"
#include "Component/AttributeComponent.h"

UEnemyCombatAIComponent::UEnemyCombatAIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

EBattleAction UEnemyCombatAIComponent::ChooseAction(int32 RoundNumber, EBattleAction LastPlayerAction, bool bExtraTurn, int32 ChargeStacks) const
{
	const int32 MaxStacks = GetMaxChargeStacks();

	if (bExtraTurn)
	{
		// GDD 5.2.4：额外回合只能 出蓝刀 或 继续蓄力
		TArray<EBattleAction> Options;
		if (ChargeStacks > 0) Options.Add(EBattleAction::BlueAttack);
		if (ChargeStacks < MaxStacks) Options.Add(EBattleAction::Charge);
		return Options.Num() > 0
			? Options[FMath::RandRange(0, Options.Num() - 1)]
			: EBattleAction::RedDefense;
	}

	// 蓝攻需 ≥1 层蓄力；蓄力满上限后不能再蓄力
	TArray<EBattleAction> Options;
	Options.Add(EBattleAction::RedDefense);
	Options.Add(EBattleAction::WhiteAttack);
	if (ChargeStacks > 0) Options.Add(EBattleAction::BlueAttack);
	if (ChargeStacks < MaxStacks) Options.Add(EBattleAction::Charge);
	return Options[FMath::RandRange(0, Options.Num() - 1)];
}

int32 UEnemyCombatAIComponent::GetMaxChargeStacks() const
{
	if (const AEnemy* Enemy = Cast<AEnemy>(GetOwner()))
	{
		if (Enemy->AttributeComponent)
		{
			return FMath::Max(1, FMath::RoundToInt(Enemy->AttributeComponent->GetFinal(AttributeNames::MaxChargeStacks())));
		}
	}
	return 2;
}
