// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/BaseCharacter.h"
#include "InputActionValue.h"
#include "Role.generated.h"

class UInputMappingContext;
class UInputAction;
class USpringArmComponent;
class UCameraComponent;
class USkeletalMeshComponent;

/**
 * ARole — 可操控角色的中间基类
 *
 * 继承自 ABaseCharacter，添加：
 * - 第三人称相机系统（Spring Arm + Camera）
 * - Enhanced Input 绑定（移动 / 视角 / 跑动 / 跳跃）
 * - 走路/跑动速度切换（速度值从 UAttributeComponent 读取）
 *
 * 所有玩家可操控的角色均应继承此类，而非直接继承 ABaseCharacter。
 */
UCLASS(Abstract, Blueprintable)
class HOLE_API ARole : public ABaseCharacter
{
	GENERATED_BODY()

public:
	ARole();

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void Landed(const FHitResult& Hit) override;

	// ---- 输入回调 ----

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void OnSprintStarted(const FInputActionValue& Value);
	void OnSprintCompleted(const FInputActionValue& Value);
	void OnJumpStarted(const FInputActionValue& Value);
	void OnJumpCompleted(const FInputActionValue& Value);

	// ---- 速度辅助 ----

	/** 获取当前走路速度（从 AttributeComponent 读取，支持 buff 修正） */
	float GetWalkSpeed() const;

	/** 获取当前跑动速度（从 AttributeComponent 读取，支持 buff 修正） */
	float GetSprintSpeed() const;

public:
	// ---- 相机组件 ----

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Role|Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Role|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	// ---- 输入配置（在蓝图子类中设置对应资产） ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role|Input")
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role|Input")
	TObjectPtr<UInputAction> JumpAction;

	// ---- 模块化角色网格体 ----

	/** 眼睛网格体（共享身体骨骼动画） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Role|Modular")
	TObjectPtr<USkeletalMeshComponent> EyesMesh;

	/** 头发网格体（共享身体骨骼动画） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Role|Modular")
	TObjectPtr<USkeletalMeshComponent> HairMesh;

	/** 上衣网格体（共享身体骨骼动画） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Role|Modular")
	TObjectPtr<USkeletalMeshComponent> ShirtMesh;

	/** 裤子网格体（共享身体骨骼动画） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Role|Modular")
	TObjectPtr<USkeletalMeshComponent> PantsMesh;

	/** 鞋子网格体（共享身体骨骼动画） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Role|Modular")
	TObjectPtr<USkeletalMeshComponent> ShoesMesh;

	/** 设置所有模块化部件以跟随身体网格体的骨骼动画（Leader Pose 模式） */
	UFUNCTION(BlueprintCallable, Category = "Role|Modular")
	void SetupModularMasterPose();

private:
	/** 当前是否正在跑动 */
	bool bIsSprinting;

	/** 落地后短暂禁止移动输入，防止落地动画滑步 */
	bool bLandingLocked = false;

	/** 落地锁定计时器句柄 */
	FTimerHandle LandingLockTimer;

	/** 落地锁定到期回调 */
	void OnLandingLockExpired();

	/** 获取落地锁定时长（从 AttributeComponent 读取） */
	float GetLandingLockTime() const;
};
