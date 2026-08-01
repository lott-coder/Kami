// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/Role.h"
#include "Component/AttributeComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "TimerManager.h"

ARole::ARole()
{
	PrimaryActorTick.bCanEverTick = false;

	// ---- 角色旋转：仅移动时朝移动方向，不跟镜头 ----
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);

	// ---- 弹簧臂 ----
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 300.0f;
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->bEnableCameraRotationLag = true;
	SpringArm->CameraRotationLagSpeed = 10.0f;

	// ---- 第三人称相机 ----
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// ---- 模块化角色网格体（附加到身体骨骼网格体，共享骨骼动画） ----
	EyesMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("EyesMesh"));
	EyesMesh->SetupAttachment(GetMesh());

	HairMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HairMesh"));
	HairMesh->SetupAttachment(GetMesh());

	ShirtMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ShirtMesh"));
	ShirtMesh->SetupAttachment(GetMesh());

	PantsMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PantsMesh"));
	PantsMesh->SetupAttachment(GetMesh());

	ShoesMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ShoesMesh"));
	ShoesMesh->SetupAttachment(GetMesh());
}

void ARole::BeginPlay()
{
	Super::BeginPlay();

	// 从 AttributeComponent 读取初始移动速度
	GetCharacterMovement()->MaxWalkSpeed = GetWalkSpeed();

	// 设置模块化部件共享身体骨骼动画
	SetupModularMasterPose();

	// 添加 "Player" 标签供 BossIntroComponent 快速检测
	// Tag 检测（FName 索引比较）比 Cast<ARole>（UHT 类型层级遍历）更快
	Tags.Add(FName(TEXT("Player")));
}

float ARole::GetWalkSpeed() const
{
	if (AttributeComponent)
	{
		return AttributeComponent->GetFinal(AttributeNames::WalkSpeed());
	}
	return 300.0f; // 回退
}

float ARole::GetSprintSpeed() const
{
	if (AttributeComponent)
	{
		return AttributeComponent->GetFinal(AttributeNames::SprintSpeed());
	}
	return 600.0f; // 回退
}

void ARole::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
		{
			EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ARole::Move);
		}

		if (LookAction)
		{
			EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ARole::Look);
		}

		if (SprintAction)
		{
			EnhancedInput->BindAction(SprintAction, ETriggerEvent::Started, this, &ARole::OnSprintStarted);
			EnhancedInput->BindAction(SprintAction, ETriggerEvent::Completed, this, &ARole::OnSprintCompleted);
		}

		if (JumpAction)
		{
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ARole::OnJumpStarted);
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ARole::OnJumpCompleted);
		}
	}
}

// ---- 移动 ----

void ARole::Move(const FInputActionValue& Value)
{
	// 落地锁定期间忽略移动输入，防止落地动画滑步
	if (bLandingLocked || bCinematicLocked)
	{
		return;
	}

	const FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller)
	{
		const FRotator ControlRotation = Controller->GetControlRotation();
		const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

// ---- 视角 ----

void ARole::Look(const FInputActionValue& Value)
{
	const FVector2D LookVector = Value.Get<FVector2D>();

	if (Controller)
	{
		AddControllerYawInput(LookVector.X);
		AddControllerPitchInput(LookVector.Y);
	}
}

// ---- 跑动 ----

void ARole::OnSprintStarted(const FInputActionValue& Value)
{
	if (bCinematicLocked)
	{
		return;
	}
	GetCharacterMovement()->MaxWalkSpeed = GetSprintSpeed();
}

void ARole::OnSprintCompleted(const FInputActionValue& Value)
{
	GetCharacterMovement()->MaxWalkSpeed = GetWalkSpeed();
}

// ---- 跳跃 ----

void ARole::OnJumpStarted(const FInputActionValue& Value)
{
	if (bCinematicLocked)
	{
		return;
	}
	Jump();
}

void ARole::OnJumpCompleted(const FInputActionValue& Value)
{
	StopJumping();
}

// ---- 模块化角色 ----

void ARole::SetupModularMasterPose()
{
	USkeletalMeshComponent* BodyMesh = GetMesh();
	if (!BodyMesh)
	{
		return;
	}

	if (EyesMesh)
	{
		EyesMesh->SetLeaderPoseComponent(BodyMesh);
	}
	if (HairMesh)
	{
		HairMesh->SetLeaderPoseComponent(BodyMesh);
	}
	if (ShirtMesh)
	{
		ShirtMesh->SetLeaderPoseComponent(BodyMesh);
	}
	if (PantsMesh)
	{
		PantsMesh->SetLeaderPoseComponent(BodyMesh);
	}
	if (ShoesMesh)
	{
		ShoesMesh->SetLeaderPoseComponent(BodyMesh);
	}
}

// ---- 落地锁定 ----

void ARole::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	const float LockTime = GetLandingLockTime();
	if (LockTime > 0.0f)
	{
		bLandingLocked = true;

		// 若已有在途计时器则清除，以新落地为准
		GetWorldTimerManager().ClearTimer(LandingLockTimer);
		GetWorldTimerManager().SetTimer(
			LandingLockTimer,
			this,
			&ARole::OnLandingLockExpired,
			LockTime,
			false);
	}
}

void ARole::OnLandingLockExpired()
{
	bLandingLocked = false;
}

float ARole::GetLandingLockTime() const
{
	if (AttributeComponent)
	{
		return AttributeComponent->GetFinal(AttributeNames::LandingLockTime());
	}
	return 0.3f; // 回退默认值
}

// ---- 输入控制 ----

void ARole::SetCinematicLocked(bool bLocked)
{
	bCinematicLocked = bLocked;
}
