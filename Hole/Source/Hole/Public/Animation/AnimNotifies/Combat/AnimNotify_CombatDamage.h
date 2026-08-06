// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_CombatDamage.generated.h"

/**
 * 战斗命中通知：挂在攻击/前摇 Montage 的挥击帧。
 * 触发时找到玩家的 UBattleComponent 并调用 OnHitNotify(攻击者, EventName)。
 */
UCLASS()
class HOLE_API UAnimNotify_CombatDamage : public UAnimNotify
{
	GENERATED_BODY()

public:
	/** 与待命中事件槽匹配的事件名（WhiteAttackHit / BlueAttackHit / GoldCounterHit / ClashTelegraphHit） */
	UPROPERTY(EditAnywhere, Category = "Combat")
	FName EventName;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
