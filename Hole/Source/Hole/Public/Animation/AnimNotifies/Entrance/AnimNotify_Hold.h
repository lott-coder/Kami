// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_Hold.generated.h"

/**
 * UAnimNotify_Hold — 入场（Entrance）Montage 类通知
 *
 * 对应目录 Animation/AnimNotifies/Entrance（按 Montage 类别归档）；
 * 触发时把背上武器转移到玩家手部插槽（UWeaponVisualComponent::HandSocketName）。
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Hold"))
class HOLE_API UAnimNotify_Hold : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
