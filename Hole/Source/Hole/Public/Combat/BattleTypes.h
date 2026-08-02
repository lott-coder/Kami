// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BattleTypes.generated.h"

// ============================================================================
// 战斗阶段
// ============================================================================
UENUM(BlueprintType)
enum class EBattlePhase : uint8
{
	Idle			UMETA(DisplayName = "空闲"),
	Entering		UMETA(DisplayName = "入场"),
	ActionSelect	UMETA(DisplayName = "行动选择"),
	Resolving		UMETA(DisplayName = "结算中"),
	Clash			UMETA(DisplayName = "同色对抗"),
	Ended			UMETA(DisplayName = "已结束")
};

// ============================================================================
// 战斗行动（红防/蓝攻/白攻/蓄力/技能）
// ============================================================================
UENUM(BlueprintType)
enum class EBattleAction : uint8
{
	None		UMETA(DisplayName = "无"),
	RedDefense	UMETA(DisplayName = "红色防御"),
	BlueAttack	UMETA(DisplayName = "蓝色攻击"),
	WhiteAttack	UMETA(DisplayName = "白色攻击"),
	Charge		UMETA(DisplayName = "蓄力"),
	Skill		UMETA(DisplayName = "技能")
};

// ============================================================================
// 同色碰撞类型
// ============================================================================
UENUM(BlueprintType)
enum class EClashType : uint8
{
	None		UMETA(DisplayName = "无"),
	BlueClash	UMETA(DisplayName = "蓝色碰撞"),
	WhiteClash	UMETA(DisplayName = "白色碰撞")
};

// ============================================================================
// 同色碰撞结果
// ============================================================================
UENUM(BlueprintType)
enum class EClashResult : uint8
{
	None			UMETA(DisplayName = "无"),
	BlockSuccess	UMETA(DisplayName = "格挡成功"),
	BlockFail		UMETA(DisplayName = "格挡失败"),
	DodgeSuccess	UMETA(DisplayName = "闪避成功"),
	DodgeFail		UMETA(DisplayName = "闪避失败")
};

// ============================================================================
// 回合结算结果（ResolveNormalTurn / ResolveExtraTurn 的返回值）
// 注：bClash=true 时，PlayerDamageTaken 表示“防御前的原始伤害”，由对抗阶段再修正
// ============================================================================
USTRUCT(BlueprintType)
struct HOLE_API FTurnResolution
{
	GENERATED_BODY()

	/** 玩家受到的伤害（同色碰撞时=防御前原始值） */
	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	float PlayerDamageTaken = 0.0f;

	/** 敌人受到的伤害 */
	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	float EnemyDamageTaken = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	bool bPlayerChargeInterrupted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	bool bEnemyChargeInterrupted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	bool bPlayerExtraTurn = false;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	bool bEnemyExtraTurn = false;

	/** 是否进入同色实时对抗 */
	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	bool bClash = false;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	EClashType ClashType = EClashType::None;
};
