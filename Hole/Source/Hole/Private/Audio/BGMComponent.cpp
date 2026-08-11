// Copyright Epic Games, Inc. All Rights Reserved.

#include "Audio/BGMComponent.h"
#include "Combat/BattleComponent.h"
#include "Character/Enemy.h"
#include "Components/AudioComponent.h"
#include "Component/BossIntroComponent.h"
#include "DataTable/MusicConfigTable.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Sound/SoundBase.h"
#include "Subsystem/CombatFormulaSubsystem.h"

UBGMComponent::UBGMComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	MusicPlayer = CreateDefaultSubobject<UAudioComponent>(TEXT("MusicPlayer"));
	MusicPlayer->bAutoActivate = false;
	MusicPlayer->bIsUISound = false;
}

void UBGMComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		if (Owner->GetRootComponent())
		{
			MusicPlayer->AttachToComponent(
				Owner->GetRootComponent(),
				FAttachmentTransformRules::KeepRelativeTransform);
		}
		Battle = Cast<UBattleComponent>(Owner->GetComponentByClass(UBattleComponent::StaticClass()));
	}
	if (Battle.IsValid())
	{
		Battle->OnBattleStateChanged.AddDynamic(this, &UBGMComponent::HandleBattleStateChanged);
		Battle->OnTutorialFleeSequenceStateChanged.AddDynamic(this, &UBGMComponent::HandleTutorialFleeSequenceStateChanged);
	}
	if (MusicPlayer)
	{
		MusicPlayer->OnAudioFinished.AddDynamic(this, &UBGMComponent::HandleMusicFinished);
	}
	// 所有 Boss 开场 Level Sequence：播放期间静音（序列自带独立音乐）
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AEnemy> It(World); It; ++It)
		{
			if (UBossIntroComponent* Intro = It->FindComponentByClass<UBossIntroComponent>())
			{
				Intro->OnIntroStateChanged.AddDynamic(this, &UBGMComponent::HandleBossIntroStateChanged);
				BoundBossIntros.Add(Intro);
			}
		}
	}

	UpdateMusic();
}

void UBGMComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Battle.IsValid())
	{
		Battle->OnBattleStateChanged.RemoveDynamic(this, &UBGMComponent::HandleBattleStateChanged);
		Battle->OnTutorialFleeSequenceStateChanged.RemoveDynamic(this, &UBGMComponent::HandleTutorialFleeSequenceStateChanged);
	}
	for (const TWeakObjectPtr<UBossIntroComponent>& Intro : BoundBossIntros)
	{
		if (Intro.IsValid())
		{
			Intro->OnIntroStateChanged.RemoveDynamic(this, &UBGMComponent::HandleBossIntroStateChanged);
		}
	}
	BoundBossIntros.Reset();
	if (MusicPlayer)
	{
		MusicPlayer->Stop();
	}
	Super::EndPlay(EndPlayReason);
}

void UBGMComponent::SetExplorationAreaID(FName NewAreaID)
{
	if (ExplorationAreaID != NewAreaID)
	{
		ExplorationAreaID = NewAreaID;
		if (!bInBattle)
		{
			UpdateMusic();
		}
	}
}

void UBGMComponent::SetSequencePlaying(bool bPlaying)
{
	SetSequenceMuteCountDelta(bPlaying ? 1 : -1);
}

void UBGMComponent::HandleTutorialFleeSequenceStateChanged(bool bPlaying)
{
	SetSequenceMuteCountDelta(bPlaying ? 1 : -1);
}

void UBGMComponent::HandleBossIntroStateChanged(EBossIntroState NewState)
{
	SetSequenceMuteCountDelta(NewState == EBossIntroState::Playing ? 1 : -1);
}

void UBGMComponent::SetSequenceMuteCountDelta(int32 Delta)
{
	const int32 OldCount = SequenceMuteCount;
	SequenceMuteCount = FMath::Max(0, SequenceMuteCount + Delta);
	if (SequenceMuteCount == OldCount)
	{
		return;
	}
	if (SequenceMuteCount > 0)
	{
		// Level Sequence 播放中：BGM 组件静音（序列自带独立音乐）
		if (MusicPlayer)
		{
			// 先关闭手动循环重播，防止 Stop 触发 OnAudioFinished 把 BGM 重新插回
			bMusicFinishedReplayEnabled = false;
			if (MusicPlayer->IsPlaying())
			{
				MusicPlayer->FadeOut(DefaultFadeOutTime, 0.0f);
				MusicPlayer->Stop();
			}
		}
	}
	else if (OldCount > 0)
	{
		// 序列结束：恢复当前状态对应 BGM
		UpdateMusic();
	}
}

void UBGMComponent::HandleBattleStateChanged()
{
	const bool bNowInBattle = Battle.IsValid() && Battle->GetBattlePhase() != EBattlePhase::Idle;
	if (bNowInBattle != bInBattle)
	{
		bInBattle = bNowInBattle;
		UpdateMusic();
	}
}

void UBGMComponent::UpdateMusic()
{
	if (!MusicPlayer || !GetWorld())
	{
		return;
	}
	// Level Sequence 播放期间不播放 BGM
	if (SequenceMuteCount > 0)
	{
		// 关闭手动循环重播，防止 Stop 触发 OnAudioFinished 把 BGM 重新插回
		bMusicFinishedReplayEnabled = false;
		if (MusicPlayer->IsPlaying())
		{
			MusicPlayer->FadeOut(DefaultFadeOutTime, 0.0f);
			MusicPlayer->Stop();
		}
		return;
	}

	UCombatFormulaSubsystem* Subsystem = GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UCombatFormulaSubsystem>()
		: nullptr;
	if (!Subsystem)
	{
		return;
	}

	if (bInBattle && Battle.IsValid())
	{
		if (AEnemy* Enemy = Battle->GetBossEnemy())
		{
			if (const FBGMConfigRow* Row = Subsystem->GetEnemyBGMConfigRow(Enemy->EnemyID))
			{
				PlayBGM(*Row, DefaultFadeInTime, DefaultFadeOutTime);
				return;
			}
		}
	}

	if (const FBGMConfigRow* Row = Subsystem->GetAreaBGMConfigRow(ExplorationAreaID))
	{
		PlayBGM(*Row, DefaultFadeInTime, DefaultFadeOutTime);
	}
	else
	{
		MusicPlayer->FadeOut(DefaultFadeOutTime, 0.0f);
	}
}

void UBGMComponent::PlayBGM(const FBGMConfigRow& Row, float FallbackFadeIn, float FallbackFadeOut)
{
	if (Row.Music.IsNull())
	{
		return;
	}
	USoundBase* Sound = Row.Music.LoadSynchronous();
	if (!Sound)
	{
		return;
	}

	const float FadeIn = Row.FadeInTime > 0.0f ? Row.FadeInTime : FallbackFadeIn;
	const float FadeOut = Row.FadeOutTime > 0.0f ? Row.FadeOutTime : FallbackFadeOut;

	// 同一首音乐已在播放：不重播
	if (MusicPlayer->GetSound() == Sound && MusicPlayer->IsPlaying())
	{
		return;
	}

	// 切换曲目期间关闭手动循环，防止旧曲停止事件误重播
	bMusicFinishedReplayEnabled = false;
	MusicPlayer->FadeOut(FadeOut, 0.0f);
	MusicPlayer->Stop();
	MusicPlayer->SetSound(Sound);
	MusicPlayer->SetVolumeMultiplier(Row.Volume);
	MusicPlayer->Play();
	MusicPlayer->FadeIn(FadeIn, Row.Volume);
	bCurrentLoop = Row.bLoop;
	bMusicFinishedReplayEnabled = Row.bLoop;
	UE_LOG(LogTemp, Log, TEXT("UBGMComponent::PlayBGM - 播放 %s（音量 %.2f，循环 %d）"), *Sound->GetName(), Row.Volume, Row.bLoop ? 1 : 0);
}

void UBGMComponent::HandleMusicFinished()
{
	// 表配置 bLoop=true 且资产自身不循环时，结束回调手动重播
	if (bMusicFinishedReplayEnabled && bCurrentLoop && MusicPlayer && !MusicPlayer->IsPlaying())
	{
		MusicPlayer->Play();
	}
}
