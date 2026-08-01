// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BaseCharacter.generated.h"

class UAttributeComponent;

/**
 * 三色属性枚举 — 红克制蓝、蓝克制白、白克制红
 */
UENUM(BlueprintType)
enum class EElementalColor : uint8
{
	None	UMETA(DisplayName = "无"),
	Red		UMETA(DisplayName = "红"),
	Blue	UMETA(DisplayName = "蓝"),
	White	UMETA(DisplayName = "白")
};

/**
 * ABaseCharacter — 所有角色的基类
 *
 * 提供生命值、颜色属性和死亡处理等基础功能。
 * 战斗属性（MaxHP / 伤害修正 / 窗口时间 等）统一由 UAttributeComponent 管理。
 * 后续所有角色（主角、魔法师、NPC 等）均继承此类。
 */
UCLASS(Abstract, Blueprintable)
class HOLE_API ABaseCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ABaseCharacter();

protected:
	virtual void BeginPlay() override;

	/** 属性是否已从 DataTable 初始化（BeginPlay 兜底判断用） */
	bool bAttributesInitialized = false;

public:
	// ---- 身份 ----

	/**
	 * 角色配置 ID，对应 DT_CharacterConfig 的 RowName。
	 * 子类构造函数中设置（如 "drifter"、"ace"）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BaseCharacter|Identity")
	FName CharacterID;

	/** 角色颜色属性（红/蓝/白） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BaseCharacter|Identity")
	EElementalColor ElementalColor;

	// ---- 组件 ----

	/** 运行时属性容器（MaxHP / 伤害修正 / 窗口时间 等所有可被 buff 修改的属性） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BaseCharacter|Component")
	TObjectPtr<UAttributeComponent> AttributeComponent;

	// ---- 运行时状态 ----

	/** 当前生命值（高频变化，保留在 Character 上） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BaseCharacter|State")
	float CurrentHealth;

	// ---- 接口 ----

	/** 初始化角色属性（从 DataTable 加载，子类重写以指定 CharacterID） */
	UFUNCTION(BlueprintCallable, Category = "BaseCharacter")
	virtual void InitializeAttributes();

	/** 受到伤害（子类可重写以实现自定义伤害逻辑） */
	UFUNCTION(BlueprintCallable, Category = "BaseCharacter")
	virtual void ReceiveDamage(float DamageAmount, AActor* DamageCauser);

	/** 角色死亡处理（子类可重写以实现自定义死亡逻辑） */
	UFUNCTION(BlueprintCallable, Category = "BaseCharacter")
	virtual void OnDeath();

	/** 是否已死亡 */
	UFUNCTION(BlueprintPure, Category = "BaseCharacter")
	bool IsDead() const;

	/** 获取当前生命值百分比（从 AttributeComponent 读取 MaxHP） */
	UFUNCTION(BlueprintPure, Category = "BaseCharacter")
	float GetHealthPercent() const;

	/** 治疗角色（上限从 AttributeComponent 读取） */
	UFUNCTION(BlueprintCallable, Category = "BaseCharacter")
	virtual void Heal(float HealAmount);

	/** 便捷访问：获取当前最大生命值 */
	UFUNCTION(BlueprintPure, Category = "BaseCharacter")
	float GetMaxHealth() const;
};
