// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/Satan.h"
#include "Component/AttributeComponent.h"

ASatan::ASatan()
{
	// 绑定撒旦的 DataTable 配置行
	EnemyID = FName(TEXT("satan"));

	// 身份
	Tier = EEnemyTier::FinalBoss;
	ElementalColor = EElementalColor::None;
}

void ASatan::InitializeAttributes()
{
	// 基类 AEnemy 会从 DT_EnemyConfig 中查找 EnemyID("satan") 行并初始化 AttributeComponent
	Super::InitializeAttributes();
}
