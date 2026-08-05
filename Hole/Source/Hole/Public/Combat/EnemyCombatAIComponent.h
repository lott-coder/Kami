// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/BattleTypes.h"
#include "EnemyCombatAIComponent.generated.h"

/**
 * UEnemyCombatAIComponent - 敌人战斗 AI（挂在 BP_Satan 上）
 *
 * v1 策略：全随机四选一（红防/蓝攻/白攻/蓄力）；
 * 额外回合时按 GDD 规则只在 蓝攻/蓄力 中二选一。
 * AIDifficulty / AIPreference 暂不参与权重，保留接口供后续策划需求接入。
 */
UCLASS(ClassGroup = (Combat), Blueprintable, meta = (BlueprintSpawnableComponent))
class HOLE_API UEnemyCombatAIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyCombatAIComponent();

	/** 选择敌人本回合行动；bExtraTurn=true 时返回 BlueAttack 或 Charge */
	UFUNCTION(BlueprintCallable, Category = "EnemyAI")
	EBattleAction ChooseAction(int32 RoundNumber, EBattleAction LastPlayerAction, bool bExtraTurn, int32 ChargeStacks) const;

private:
	/** 从 Owner 属性读取最大蓄力层数（回退 2） */
	int32 GetMaxChargeStacks() const;
};
