// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/Apprentice.h"

AApprentice::AApprentice()
{
	// 绑定低级魔法师的 DataTable 配置行
	EnemyID = FName(TEXT("apprentice"));

	// 身份
	EnemyConfig.Tier = EEnemyTier::Normal;
	ElementalColor = EElementalColor::None;
}

void AApprentice::InitializeAttributes()
{
	// 基类 AEnemy 会从 DT_EnemyConfig 中查找 EnemyID("apprentice") 行并初始化 AttributeComponent
	Super::InitializeAttributes();
}
