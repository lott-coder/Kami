// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/Role.h"
#include "Dale.generated.h"

/**
 * ADale — 主角 "漂泊者"（The Drifter）
 *
 * 继承自 ARole（可操控角色），设置主角专属的默认属性值。
 * 相机系统由 ARole 提供，Dale 专注于主角特有的属性与技能。
 */
UCLASS(Blueprintable)
class HOLE_API ADale : public ARole
{
	GENERATED_BODY()

public:
	ADale();

protected:
	virtual void BeginPlay() override;

public:
	/** 初始化主角属性（设置主角默认值） */
	virtual void InitializeAttributes() override;
};
