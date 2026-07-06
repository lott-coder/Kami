// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"

class UInputMappingContext;
class UInputAction;

#include "BaseCharacter.generated.h"

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
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// ---- 输入回调（子类可重写以扩展输入处理） ----

	virtual void Move(const FInputActionValue& Value);
	virtual void Look(const FInputActionValue& Value);

public:
	// ---- 属性 ----

	/** 当前生命值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BaseCharacter|Attributes")
	float CurrentHealth;

	/** 最大生命值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BaseCharacter|Attributes")
	float MaxHealth;

	/** 角色颜色属性（红/蓝/白） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BaseCharacter|Attributes")
	EElementalColor ElementalColor;

	// ---- 输入配置（在蓝图子类中设置对应资产） ----

	/** 默认 Input Mapping Context */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseCharacter|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** 移动输入动作（WASD / 左摇杆） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseCharacter|Input")
	TObjectPtr<UInputAction> MoveAction;

	/** 视角输入动作（鼠标 / 右摇杆） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseCharacter|Input")
	TObjectPtr<UInputAction> LookAction;

	// ---- 接口 ----

	/** 初始化角色属性（子类可重写以设置不同的默认值） */
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

	/** 获取当前生命值百分比 */
	UFUNCTION(BlueprintPure, Category = "BaseCharacter")
	float GetHealthPercent() const;

	/** 治疗角色 */
	UFUNCTION(BlueprintCallable, Category = "BaseCharacter")
	virtual void Heal(float HealAmount);
};
