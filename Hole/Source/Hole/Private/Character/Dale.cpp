// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/Dale.h"

ADale::ADale()
{
	// 绑定主角的 DataTable 配置行
	CharacterID = FName(TEXT("drifter"));

	// 颜色属性
	ElementalColor = EElementalColor::None;

	// 属性不再在此硬编码——由 InitializeAttributes() 从 DT_CharacterConfig 加载
}

void ADale::InitializeAttributes()
{
	// 基类会从 DT_CharacterConfig 中查找 CharacterID("drifter") 行并初始化 AttributeComponent
	Super::InitializeAttributes();
}
