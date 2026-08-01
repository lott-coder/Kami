// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BossIntroComponent.generated.h"

class USphereComponent;
class ULevelSequence;
class ULevelSequencePlayer;
class ALevelSequenceActor;
class UInputAction;

// ============================================================================
// Boss 出场动画相关枚举
// ============================================================================

/**
 * EBossIntroState — Boss 出场动画状态机
 *
 * Idle    = 等待玩家进入触发区域
 * Playing = 正在播放出场动画（锁定玩家输入，监听跳过键）
 * Combat  = 动画结束（或被跳过），进入战斗阶段
 */
UENUM(BlueprintType)
enum class EBossIntroState : uint8
{
	Idle	UMETA(DisplayName = "空闲"),
	Playing	UMETA(DisplayName = "播放中"),
	Combat	UMETA(DisplayName = "战斗")
};

// ============================================================================
// UBossIntroComponent — Boss 出场动画管理器
// ============================================================================

/**
 * 挂载在任意 AEnemy 子类上，提供：
 * - 球形触发器检测携带指定标签的角色（默认 "Player"）
 * - 三态状态机（Idle / Playing / Combat），防止重入
 * - Level Sequence 播放 + 动态 Actor 绑定 + 镜头管理（纯 C++）
 * - 跳过支持（EnhancedInput Action）
 * - 死亡重播（ResetIntro() 回到 Idle，无需持久化标记）
 *
 * 模块化设计：不同 Boss 只需配置 IntroSequenceAsset / TriggerRadius / SkipAction。
 *
 * 用法：在 BP_Satan 上挂载此组件（或 C++ 子类），填入属性即可。
 */
UCLASS(ClassGroup = (Boss), Blueprintable, meta = (BlueprintSpawnableComponent))
class HOLE_API UBossIntroComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBossIntroComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// ---- 触发器 ----

	/** 检测玩家的球形触发器（构造函数中自动创建并附加到 Owner） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BossIntro|Trigger")
	TObjectPtr<USphereComponent> TriggerSphere;

	// ---- 配置（每个 Boss 实例独立配置） ----

	/** 出场动画序列资产 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BossIntro|Config")
	TObjectPtr<ULevelSequence> IntroSequenceAsset;

	/** 触发球半径（cm），默认 500 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BossIntro|Config")
	float TriggerRadius = 500.0f;

	/** 跳过用的 EnhancedInput Action */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BossIntro|Config")
	TObjectPtr<UInputAction> SkipAction;

	/** 检测的 Actor 标签，默认 "Player"（Tag 检测比 Cast 更快且零耦合） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BossIntro|Config")
	FName TriggerTag = FName(TEXT("Player"));

	/** 动画结束切回玩家镜头时的混合时间（秒），默认 0.3 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BossIntro|Config")
	float CameraBlendBackTime = 0.3f;

	/** 跳过动画时切回玩家镜头的混合时间（秒），默认 0.1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BossIntro|Config")
	float CameraBlendSkipTime = 0.1f;

	/** 入场时镜头混合到 Sequence 视角的时间（秒），默认 0.5 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BossIntro|Config")
	float CameraBlendInTime = 0.5f;

	// ---- 状态机 ----

	/** 开始播放出场动画（Idle → Playing），通常由 Overlap 自动触发 */
	UFUNCTION(BlueprintCallable, Category = "BossIntro")
	virtual void StartIntro();

	/** 跳过出场动画（Playing → Combat），由跳过按键或 BP 调用 */
	UFUNCTION(BlueprintCallable, Category = "BossIntro")
	virtual void SkipIntro();

	/** 动画正常结束回调（Playing → Combat），由 Sequence OnFinished 自动触发 */
	UFUNCTION(BlueprintCallable, Category = "BossIntro")
	virtual void CompleteIntro();

	/** 重置到场状态（任意 → Idle），用于玩家死亡后允许重播 */
	UFUNCTION(BlueprintCallable, Category = "BossIntro")
	virtual void ResetIntro();

	/** 查询当前状态 */
	UFUNCTION(BlueprintPure, Category = "BossIntro")
	EBossIntroState GetIntroState() const { return CurrentState; }

	/** 获取检测到的玩家 Actor（弱引用，Playing/Combat 期间有效） */
	UFUNCTION(BlueprintPure, Category = "BossIntro")
	AActor* GetDetectedPlayer() const { return DetectedPlayerActor.Get(); }

protected:
	// ---- 出场动画实现（C++ 子类可覆写以自定义行为） ----

	/** 播放 Level Sequence + 动态绑定 Actor */
	virtual void PlayIntroSequence();

	/** 停止 Sequence 播放并清理 */
	virtual void StopIntroSequence();

	/** 镜头切到 Sequence 的视角 */
	virtual void CinematicCameraIn();

	/** 镜头切回玩家视角 */
	virtual void CinematicCameraOut(float BlendTime);

	// ---- 碰撞回调 ----

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	// ---- Sequence 完成回调 ----

	UFUNCTION()
	void OnSequenceFinished();

	// ---- 跳过输入 ----

	void OnSkipPressed();
	void BindSkipInput();
	void UnbindSkipInput();

	// ---- 输入锁定 ----

	void LockPlayerInput();
	void UnlockPlayerInput();

	// ---- 状态切换 ----

	void SetState(EBossIntroState NewState);

private:
	/** 当前状态 */
	UPROPERTY(VisibleAnywhere, Category = "BossIntro|State")
	EBossIntroState CurrentState = EBossIntroState::Idle;

	/** 检测到的玩家弱引用（不阻止 GC） */
	TWeakObjectPtr<AActor> DetectedPlayerActor;

	/** 当前播放的 Sequence Player */
	UPROPERTY()
	TObjectPtr<ULevelSequencePlayer> SequencePlayer;

	/** Sequence Player 关联的 Actor（用于设置 ViewTarget 和清理） */
	UPROPERTY()
	TObjectPtr<ALevelSequenceActor> SequenceActor;

	/** 切入动画前的玩家视点目标（用于恢复） */
	UPROPERTY()
	TObjectPtr<AActor> PreviousViewTarget;
};
