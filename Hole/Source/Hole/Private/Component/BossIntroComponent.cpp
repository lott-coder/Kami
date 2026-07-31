// Copyright Epic Games, Inc. All Rights Reserved.

#include "Component/BossIntroComponent.h"
#include "Components/SphereComponent.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "LevelSequenceActor.h"
#include "InputAction.h"
#include "EnhancedInputComponent.h"
#include "Character/Role.h"
#include "GameFramework/PlayerController.h"

UBossIntroComponent::UBossIntroComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	TriggerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerSphere"));
	TriggerSphere->SetSphereRadius(TriggerRadius);
	TriggerSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerSphere->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerSphere->SetGenerateOverlapEvents(true);
}

void UBossIntroComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (Owner && TriggerSphere)
	{
		TriggerSphere->AttachToComponent(
			Owner->GetRootComponent(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		TriggerSphere->SetSphereRadius(TriggerRadius);
	}

	if (TriggerSphere)
	{
		TriggerSphere->OnComponentBeginOverlap.AddDynamic(
			this, &UBossIntroComponent::OnTriggerBeginOverlap);
	}
}

void UBossIntroComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CurrentState == EBossIntroState::Playing)
	{
		StopIntroSequence();
		UnbindSkipInput();
		UnlockPlayerInput();
	}

	Super::EndPlay(EndPlayReason);
}

void UBossIntroComponent::OnTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (CurrentState != EBossIntroState::Idle)
	{
		return;
	}

	if (!OtherActor || !OtherActor->ActorHasTag(TriggerTag))
	{
		return;
	}

	DetectedPlayerActor = OtherActor;
	StartIntro();
}

void UBossIntroComponent::StartIntro()
{
	if (CurrentState != EBossIntroState::Idle)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("BossIntroComponent::StartIntro - not in Idle state (%d), ignored"),
			static_cast<int32>(CurrentState));
		return;
	}

	SetState(EBossIntroState::Playing);
}

void UBossIntroComponent::SkipIntro()
{
	if (CurrentState != EBossIntroState::Playing)
	{
		return;
	}

	// 先停止并销毁 SequenceActor，释放其相机控制权
	StopIntroSequence();

	// 再切回玩家镜头（必须在 Stop/Destroy 之后，否则会被覆盖）
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (PC && DetectedPlayerActor.IsValid())
	{
		PC->SetViewTarget(DetectedPlayerActor.Get());
	}

	UnbindSkipInput();
	UnlockPlayerInput();

	SetState(EBossIntroState::Combat);
}

void UBossIntroComponent::CompleteIntro()
{
	if (CurrentState != EBossIntroState::Playing)
	{
		return;
	}

	// 先停止并销毁 SequenceActor，释放其相机控制权
	StopIntroSequence();

	// 再切回玩家镜头（必须在 Stop/Destroy 之后，否则会被覆盖）
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (PC && DetectedPlayerActor.IsValid())
	{
		PC->SetViewTarget(DetectedPlayerActor.Get());
	}

	UnbindSkipInput();
	UnlockPlayerInput();

	SetState(EBossIntroState::Combat);
}

void UBossIntroComponent::ResetIntro()
{
	if (CurrentState == EBossIntroState::Idle)
	{
		return;
	}

	if (CurrentState == EBossIntroState::Playing)
	{
		StopIntroSequence();
	}

	SetState(EBossIntroState::Idle);
}

void UBossIntroComponent::SetState(EBossIntroState NewState)
{
	if (CurrentState == NewState)
	{
		return;
	}

	const EBossIntroState OldState = CurrentState;
	CurrentState = NewState;

	switch (OldState)
	{
	case EBossIntroState::Idle:
		if (TriggerSphere)
		{
			TriggerSphere->SetGenerateOverlapEvents(false);
		}
		break;

	case EBossIntroState::Playing:
		UnbindSkipInput();
		UnlockPlayerInput();
		break;

	case EBossIntroState::Combat:
		if (TriggerSphere)
		{
			TriggerSphere->SetGenerateOverlapEvents(true);
		}
		DetectedPlayerActor.Reset();
		break;
	}

	switch (NewState)
	{
	case EBossIntroState::Playing:
		LockPlayerInput();
		BindSkipInput();
		PlayIntroSequence();
		break;

	case EBossIntroState::Combat:
		break;

	case EBossIntroState::Idle:
		break;
	}
}

void UBossIntroComponent::PlayIntroSequence()
{
	if (!IntroSequenceAsset || !GetWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("BossIntroComponent::PlayIntroSequence - IntroSequenceAsset is null"));
		return;
	}

	FMovieSceneSequencePlaybackSettings Settings;
	Settings.bPauseAtEnd = true;
	Settings.bHidePlayer = false;
	Settings.bHideHud = false;
	Settings.bDisableMovementInput = false;
	Settings.bDisableLookAtInput = false;

	ALevelSequenceActor* TempSequenceActor = nullptr;
	SequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(
		GetWorld(),
		IntroSequenceAsset,
		Settings,
		TempSequenceActor);
	SequenceActor = TempSequenceActor;

	if (!SequencePlayer)
	{
		UE_LOG(LogTemp, Warning, TEXT("BossIntroComponent::PlayIntroSequence - Failed to create SequencePlayer"));
		return;
	}

	SequencePlayer->OnFinished.AddDynamic(this, &UBossIntroComponent::OnSequenceFinished);

	CinematicCameraIn();

	SequencePlayer->Play();
}

void UBossIntroComponent::StopIntroSequence()
{
	if (SequencePlayer)
	{
		SequencePlayer->Stop();
	}

	// 销毁动态创建的 SequenceActor，释放相机控制权
	if (SequenceActor)
	{
		SequenceActor->Destroy();
	}

	SequencePlayer = nullptr;
	SequenceActor = nullptr;
}

void UBossIntroComponent::OnSequenceFinished()
{
	CompleteIntro();
}

void UBossIntroComponent::CinematicCameraIn()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}

	PreviousViewTarget = PC->GetViewTarget();

	PC->SetViewTargetWithBlend(SequenceActor, 0.5f,
		EViewTargetBlendFunction::VTBlend_Cubic);
}

void UBossIntroComponent::CinematicCameraOut(float BlendTime)
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}

	AActor* Player = DetectedPlayerActor.Get();
	if (Player)
	{
		PC->SetViewTargetWithBlend(Player, BlendTime,
			EViewTargetBlendFunction::VTBlend_EaseOut);
	}

	PreviousViewTarget = nullptr;
}

void UBossIntroComponent::OnSkipPressed()
{
	SkipIntro();
}

void UBossIntroComponent::BindSkipInput()
{
	if (!SkipAction)
	{
		return;
	}

	// 绑定到玩家 Pawn 的 EnhancedInputComponent（ARole::SetupPlayerInputComponent 创建的）
	AActor* Player = DetectedPlayerActor.Get();
	if (!Player || !Player->InputComponent)
	{
		return;
	}

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(Player->InputComponent);
	if (!EIC)
	{
		return;
	}

	// 模板方法指针版本：直接取函数地址，无需 UFUNCTION，编译期类型安全
	EIC->BindAction(SkipAction, ETriggerEvent::Triggered, this,
		&UBossIntroComponent::OnSkipPressed);
}

void UBossIntroComponent::UnbindSkipInput()
{
	AActor* Player = DetectedPlayerActor.Get();
	if (!Player || !Player->InputComponent)
	{
		return;
	}

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(Player->InputComponent);
	if (EIC)
	{
		EIC->ClearBindingsForObject(this);
	}
}

void UBossIntroComponent::LockPlayerInput()
{
	if (ARole* Role = Cast<ARole>(DetectedPlayerActor.Get()))
	{
		Role->SetCinematicLocked(true);
	}
}

void UBossIntroComponent::UnlockPlayerInput()
{
	if (ARole* Role = Cast<ARole>(DetectedPlayerActor.Get()))
	{
		Role->SetCinematicLocked(false);
	}
}
