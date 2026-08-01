// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/Enemy.h"
#include "Component/AttributeComponent.h"
#include "Subsystem/CombatFormulaSubsystem.h"
#include "Engine/GameInstance.h"

AEnemy::AEnemy()
{
	// 敌人默认不使用 CharacterID（玩家角色用），而是使用 EnemyID
	CharacterID = NAME_None;
	EnemyID = NAME_None;
	ElementalColor = EElementalColor::None;
}

void AEnemy::InitializeAttributes()
{
	if (!AttributeComponent || EnemyID == NAME_None)
	{
		return;
	}

	// -- 1. 从子系统读取敌人配置行，整体持有（避免逐字段复制造成双源真相） --
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UCombatFormulaSubsystem* Subsystem = GameInstance->GetSubsystem<UCombatFormulaSubsystem>())
		{
			if (const FEnemyConfigRow* EnemyRow = Subsystem->GetEnemyRow(EnemyID))
			{
				EnemyConfig = *EnemyRow;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("AEnemy::InitializeAttributes - 找不到敌人行: %s"), *EnemyID.ToString());
			}
		}
	}

	// -- 2. 战斗属性（跨表合并）由子系统生成，组件只负责存储 --
	AttributeComponent->InitializeFromEnemyConfig(EnemyID);

	// -- 3. 重置当前生命值 --
	CurrentHealth = GetMaxHealth();
	bAttributesInitialized = true;
}

float AEnemy::GetAIDifficulty() const
{
	if (AttributeComponent)
	{
		return AttributeComponent->GetFinal(AttributeNames::AIDifficulty());
	}
	return 0.5f; // 回退默认值
}
