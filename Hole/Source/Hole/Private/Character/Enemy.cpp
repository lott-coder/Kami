// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/Enemy.h"
#include "Component/AttributeComponent.h"
#include "Engine/DataTable.h"

AEnemy::AEnemy()
{
	// 敌人默认不使用 CharacterID（玩家角色用），而是使用 EnemyID
	CharacterID = NAME_None;
	EnemyID = NAME_None;
	ElementalColor = EElementalColor::None;

	Tier = EEnemyTier::Normal;
	AIPreference = EEnemyAIPreference::Balanced;
}

void AEnemy::InitializeAttributes()
{
	if (!AttributeComponent || EnemyID == NAME_None)
	{
		return;
	}

	// -- 1. 加载敌人配置表，复制非战斗字段到本地成员 --
	static const FString EnemyDTPath = TEXT("/Game/DataTable/DT_EnemyConfig");
	UDataTable* EnemyDT = LoadObject<UDataTable>(nullptr, *EnemyDTPath);
	if (EnemyDT)
	{
		const FEnemyConfigRow* EnemyRow = EnemyDT->FindRow<FEnemyConfigRow>(EnemyID, TEXT("Enemy::Init"));
		if (EnemyRow)
		{
			Tier            = EnemyRow->Tier;
			AIPreference    = EnemyRow->AIPreference;
			DropSmokeType   = EnemyRow->DropSmokeType;
			DropSmokeCount  = EnemyRow->DropSmokeCount;
			DropCurrencyMin = EnemyRow->DropCurrencyMin;
			DropCurrencyMax = EnemyRow->DropCurrencyMax;
			AlertRange      = EnemyRow->AlertRange;
			ChaseRange      = EnemyRow->ChaseRange;
		}
	}

	// -- 2. 加载战斗属性到 AttributeComponent --
	AttributeComponent->InitializeFromEnemyConfig(EnemyID);

	// -- 3. 重置当前生命值 --
	CurrentHealth = GetMaxHealth();
}

float AEnemy::GetAIDifficulty() const
{
	if (AttributeComponent)
	{
		return AttributeComponent->GetFinal(AttributeNames::AIDifficulty());
	}
	return 0.5f; // 回退默认值
}
