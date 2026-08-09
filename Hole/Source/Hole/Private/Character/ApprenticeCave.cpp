// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/ApprenticeCave.h"

AApprenticeCave::AApprenticeCave()
{
	// 绑定序章教学魔法师的 DataTable 配置行（Tutorial/180HP/无掉落/洞穴）
	EnemyID = FName(TEXT("apprentice_cave"));

	// 身份
	EnemyConfig.Tier = EEnemyTier::Tutorial;
	ElementalColor = EElementalColor::None;
}

void AApprenticeCave::InitializeAttributes()
{
	// 基类 AEnemy 会从 DT_EnemyConfig 中查找 EnemyID("apprentice_cave") 行并初始化 AttributeComponent
	Super::InitializeAttributes();
}
