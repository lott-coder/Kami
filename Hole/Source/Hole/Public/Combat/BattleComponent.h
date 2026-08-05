// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/BattleTypes.h"
#include "BattleComponent.generated.h"

class ARole;
class AEnemy;
class ABaseCharacter;
class UCombatHUDWidget;
class UBattleResultHUDWidget;
class UEnemyCombatAIComponent;
class UInputAction;
class UInputMappingContext;
class UAnimMontage;
struct FCombatAnimRow;
struct FAnimRef;
class UAttributeComponent;
class UCombatFormulaSubsystem;
class UBossIntroComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBattleStateChanged);

/**
 * UBattleComponent - 战斗会话状态机（挂在 BP_Dale 上）
 *
 * 职责：
 * - BeginPlay 时按 Tag "Boss" 找到敌人并监听 BossIntroComponent::OnIntroFinished
 * - 入场：站位/朝向/固定摄像机（沿用玩家 SpringArm + FollowCamera）、锁定输入、显示 HUD
 * - 回合：双方同时选择 → 结算矩阵 → 同色碰撞实时阶段 → 回合推进/额外回合
 * - 结束：胜利退出 / 失败回到 Boss 触发点重开
 *
 * 不写公式：伤害一律调 UCombatFormulaSubsystem。
 */
UCLASS(ClassGroup = (Combat), Blueprintable, meta = (BlueprintSpawnableComponent))
class HOLE_API UBattleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBattleComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ==================== 配置 ====================

	/** 战斗 HUD 类（默认自动加载 /Game/UI/WBP_CombatHUD） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Config")
	TSubclassOf<UCombatHUDWidget> CombatHUDClass;

	/** 战斗结算 HUD 类（独立于战斗 HUD；默认自动加载 /Game/UI/HUD/WBP_BattleResult） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Config")
	TSubclassOf<UBattleResultHUDWidget> BattleResultHUDClass;

	/** 战斗输入映射（默认自动加载 /Game/Input/IMC_Combat） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Config")
	TObjectPtr<UInputMappingContext> CombatMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Config")
	TObjectPtr<UInputAction> BlockAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Config")
	TObjectPtr<UInputAction> DodgeAction;

	/** 玩家入场 Montage（占位；为空则跳过入场动画，进入战斗后直接开始回合） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Config")
	TObjectPtr<UAnimMontage> PlayerEntryMontage;

	/** 玩家入场 Montage 起始 Section（如 "Draw"；NAME_None = 从头播放） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Config")
	FName PlayerEntrySectionName = TEXT("Draw");

	/** 同色碰撞：敌方攻击前摇时间（秒），期间提示格挡/闪避 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Config")
	float ClashTelegraphTime = 0.8f;

	/** 失败横幅停留时间（秒）后回到 Boss 触发点 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Config")
	float DefeatRestartDelay = 2.0f;

	// ==================== 事件 ====================

	/** HUD 监听此事件刷新（回合开始/伤害/阶段切换时广播） */
	UPROPERTY(BlueprintAssignable, Category = "Battle|Events")
	FOnBattleStateChanged OnBattleStateChanged;

	// ==================== 公开接口 ====================

	/** 开始战斗（BossIntro 结束回调或调试调用） */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void StartBattle();

	/** 强制结束战斗（调试用） */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void EndBattle();

	/** 玩家选择行动（HUD 按钮 / 战斗输入回调统一入口） */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void PlayerChooseAction(EBattleAction Action);

	/** 调试：强制敌人下一回合固定行动 */
	UFUNCTION(BlueprintCallable, Category = "Battle|Debug")
	void SetEnemyForcedAction(EBattleAction Action, bool bEnabled);

	/** 调试：直接设置玩家血量 */
	UFUNCTION(BlueprintCallable, Category = "Battle|Debug")
	void DebugSetPlayerHealth(float Value);

	/** 调试：直接设置敌人血量 */
	UFUNCTION(BlueprintCallable, Category = "Battle|Debug")
	void DebugSetEnemyHealth(float Value);

	UFUNCTION(BlueprintPure, Category = "Battle")
	EBattlePhase GetBattlePhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "Battle")
	ABaseCharacter* GetPlayerCharacter() const;

	UFUNCTION(BlueprintPure, Category = "Battle")
	AEnemy* GetBossEnemy() const;

	UFUNCTION(BlueprintPure, Category = "Battle")
	int32 GetRoundNumber() const { return RoundNumber; }

	UFUNCTION(BlueprintPure, Category = "Battle")
	int32 GetPlayerChargeStacks() const { return PlayerChargeStacks; }

	UFUNCTION(BlueprintPure, Category = "Battle")
	int32 GetPlayerMaxChargeStacks() const { return GetMaxChargeStacks(true); }

	UFUNCTION(BlueprintPure, Category = "Battle")
	int32 GetEnemyChargeStacks() const { return EnemyChargeStacks; }

	UFUNCTION(BlueprintPure, Category = "Battle")
	EBattleAction GetPlayerLastAction() const { return PlayerLastAction; }

	UFUNCTION(BlueprintPure, Category = "Battle")
	bool IsPlayerExtraTurn() const { return bPlayerExtraTurnPending; }

protected:
	UFUNCTION()
	void HandleIntroFinished();

private:
	// ==================== 流程 ====================

	AEnemy* FindBossEnemy() const;
	void EnterBattle();
	void StartNewRound();
	void ChooseEnemyAction(bool bExtraTurn);
	void StartPlayerExtraTurn();
	void StartEnemyExtraTurn();
	FTurnResolution ResolveNormalTurn(EBattleAction PlayerAction, EBattleAction EnemyAction);
	FTurnResolution ResolveExtraTurn(bool bPlayerTurn, EBattleAction Action);
	void ApplyResolution(const FTurnResolution& Resolution);
	void ApplyDamageTo(ABaseCharacter* Target, float Amount, AActor* Causer);
	void EndTurnAndAdvance();
	void FinishBattle(bool bPlayerWon);
	void HandleVictoryCleanup();
	void HandleDefeatRestart();
	void ResetForRetry();

	// ==================== 同色碰撞 ====================

	void StartClash(EClashType ClashType);
	void OpenClashWindow();
	void OnClashImpact();
	void ResolveClash(EClashResult Result);
	void ClearClashTimers();

	// ==================== 输入 ====================

	UFUNCTION()
	void OnBlockPressed();

	UFUNCTION()
	void OnDodgePressed();

	void SetupCombatInput();
	void UnbindCombatInput();
	void AddCombatMapping();
	void RemoveCombatMapping();

	// ==================== 站位/镜头/HUD ====================

	void PositionBattleActors();
	void RestoreExplorationState();
	void LockPlayer();
	void UnlockPlayer();
	void ShowHUD();
	void HideHUD();
	void ShowResultHUD(const FText& Text);
	void HideResultHUD();
	void SheathePlayerWeapon();

	// ==================== 战斗动画 ====================

	/** 读取玩家/敌人对应的 DT_CombatAnimConfig 行（角色/敌人 ID 取 CharacterID / EnemyID） */
	const FCombatAnimRow* GetCombatAnimRow(bool bPlayer) const;

	/** 通用播放：空引用直接跳过；播放时输出日志便于 PIE 验证 */
	void PlayCombatAnim(ABaseCharacter* Character, const FAnimRef& AnimRef);

	/** 按行动播放下一个动作动画（红防/蓝攻/白攻/蓄力） */
	void PlayActionAnim(bool bPlayer, EBattleAction Action);

	/** 非碰撞回合的动画编排：受击 > 金色反击 > 蓄力被打断 > 行动动画（双方各一次） */
	void PlayResolutionAnimations(const FTurnResolution& Resolution);

	/** 玩家进入/退出同色碰撞准备姿态 */
	void SetPlayerClashReady(bool bReady);

	/** 格挡成功：先播 BlockSuccess（弹反），播完回调接 GoldCounter */
	void PlayBlockSuccessChain();

	/** 格挡/闪避失败：优先对应 Fail 动画，空则回落 Hurt */
	void PlayClashFailReaction(EClashResult Result);

	/** BlockSuccess 播完回调：接金色反击 */
	UFUNCTION()
	void OnBlockSuccessMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** 战斗结束：败方播 Death，胜方播 Victory（空引用跳过） */
	void PlayDeathAnimations(bool bPlayerWon);

	// ==================== 数值辅助（只调子系统） ====================

	float GetPlayerWhiteDamage() const;
	float GetPlayerBlueDamage(int32 Stacks) const;
	float GetEnemyWhiteDamage() const;
	float GetEnemyBlueDamage(int32 Stacks) const;
	float GetPlayerGoldDamage() const;
	float GetEnemyGoldDamage() const;
	float GetChargeResistScale() const;
	int32 GetMaxChargeStacks(bool bPlayer) const;
	float GetBlockWindow() const;
	float GetDodgeWindow() const;
	UCombatFormulaSubsystem* GetCombatSubsystem() const;
	UAttributeComponent* GetPlayerAttr() const;
	UAttributeComponent* GetEnemyAttr() const;

	void SetPhase(EBattlePhase NewPhase);

	// ==================== 状态 ====================

	UPROPERTY(VisibleAnywhere, Category = "Battle|State")
	EBattlePhase Phase = EBattlePhase::Idle;

	TWeakObjectPtr<ARole> PlayerRole;
	TWeakObjectPtr<AEnemy> BossEnemy;
	TWeakObjectPtr<UCombatHUDWidget> CombatHUD;
	TWeakObjectPtr<UBattleResultHUDWidget> ResultHUD;
	TWeakObjectPtr<UEnemyCombatAIComponent> EnemyAI;

	int32 RoundNumber = 0;
	int32 PlayerChargeStacks = 0;
	int32 EnemyChargeStacks = 0;
	EBattleAction PlayerLastAction = EBattleAction::None;
	EBattleAction EnemyChosenAction = EBattleAction::None;
	EBattleAction ForcedEnemyAction = EBattleAction::None;
	bool bForcedEnemyActionEnabled = false;
	bool bPlayerChoseAction = false;
	bool bPlayerExtraTurnPending = false;
	bool bEnemyExtraTurnPending = false;
	bool bBossDefeated = false;

	bool bClashWindowOpen = false;
	bool bClashResolved = false;
	bool bClashStarted = false;
	/** 格挡成功弹反 Montage 播完后待接金色反击 */
	bool bBlockSuccessChainPending = false;
	EClashType ActiveClashType = EClashType::None;
	EClashResult PendingClashResult = EClashResult::None;
	float PendingIncomingDamage = 0.0f;
	float PendingOutgoingDamage = 0.0f;

	FVector PlayerStartLocation;
	FRotator PlayerStartActorRotation;
	FRotator PlayerStartControlRotation;
	float OriginalArmLength = 300.0f;
	bool bOriginalCameraLag = true;
	float OriginalLagSpeed = 10.0f;
	FVector OriginalSocketOffset;
	FVector OriginalTargetOffset;
	float OriginalFOV = 90.0f;
	FVector BossStartLocation;
	FRotator BossStartRotation;

	FTimerHandle ClashOpenTimer;
	FTimerHandle ClashImpactTimer;
	FTimerHandle EndDelayTimer;
	FTimerHandle EntryDelayTimer;

	bool bCombatInputBound = false;
	bool bCombatMappingAdded = false;
	bool bMouseCursorShown = false;
};
