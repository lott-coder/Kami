// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Component/BossIntroComponent.h"
#include "BGMComponent.generated.h"

class UAudioComponent;
class USoundBase;
class UBattleComponent;
struct FBGMConfigRow;

/**
 * UBGMComponent - 背景音乐管理器（挂在 BP_Dale 上）
 *
 * - 非战斗（探索）：播放 DT_AreaBGMConfig 中当前区域行（ExplorationAreaID）的 BGM；
 * - 战斗：播放 DT_EnemyBGMConfig 中当前敌人（EnemyID）行的 BGM（不同敌人不同，缺失回退 Default）；
 * - 通过监听 UBattleComponent::OnBattleStateChanged 在战斗开始/结束间交叉淡入淡出切换。
 */
UCLASS(ClassGroup = (Audio), Blueprintable, meta = (BlueprintSpawnableComponent))
class HOLE_API UBGMComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBGMComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 设置当前探索区域（切换区域/关卡时调用，立即切换非战斗 BGM） */
	UFUNCTION(BlueprintCallable, Category = "BGM")
	void SetExplorationAreaID(FName NewAreaID);

	/** 外部 Level Sequence 播放/结束时调用：播放期间 BGM 组件静音（序列自带独立音乐） */
	UFUNCTION(BlueprintCallable, Category = "BGM")
	void SetSequencePlaying(bool bPlaying);

	/** 当前探索区域行 ID（默认 hole 洞穴） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BGM|Config")
	FName ExplorationAreaID = FName(TEXT("hole"));

	/** 未配置淡入/淡出时长时的回退值（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BGM|Config")
	float DefaultFadeInTime = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BGM|Config")
	float DefaultFadeOutTime = 1.0f;

protected:
	/** 音乐播放组件（构造函数创建并附加到 Owner） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BGM")
	TObjectPtr<UAudioComponent> MusicPlayer;

private:
	UFUNCTION()
	void HandleBattleStateChanged();

	UFUNCTION()
	void HandleMusicFinished();

	UFUNCTION()
	void HandleTutorialFleeSequenceStateChanged(bool bPlaying);

	UFUNCTION()
	void HandleBossIntroStateChanged(EBossIntroState NewState);

	void SetSequenceMuteCountDelta(int32 Delta);
	void UpdateMusic();
	void PlayBGM(const FBGMConfigRow& Row, float FallbackFadeIn, float FallbackFadeOut);

	TWeakObjectPtr<UBattleComponent> Battle;
	TArray<TWeakObjectPtr<UBossIntroComponent>> BoundBossIntros;
	bool bInBattle = false;
	/** 正在播放 Level Sequence 的数量（>0 时 BGM 静音） */
	int32 SequenceMuteCount = 0;
	/** 当前音乐是否循环（手动循环用） */
	bool bCurrentLoop = false;
	/** 是否允许播放结束回调重播（切换曲目期间关闭，防止旧曲结束事件误重播） */
	bool bMusicFinishedReplayEnabled = false;
};
