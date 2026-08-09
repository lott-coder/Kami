// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/BattleTypes.h"
#include "DataTable/CombatAnimConfigTable.h"
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

/** 待命中事件：一次攻击在命中帧要执行的伤害与防御反应（v1 每侧最多一条） */
struct FPendingHitEvent
{
	FName EventName;
	ABaseCharacter* Target = nullptr;
	float Amount = 0.0f;
	AActor* Causer = nullptr;
	FAnimRef HitReaction;
	ABaseCharacter* Defender = nullptr;
	FAnimRef DefenderReaction;
	FAnimRef DefenderFollowUp;
	UAnimMontage* FallbackMontage = nullptr;
	bool bDefenderBlocked = false;   // 本攻击被格挡（含红防反击成功）：命中时攻击者播 BlockedReaction + 停帧
	bool bActive = false;
};

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
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

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

	/** 同色碰撞：敌方碰撞攻击时长（秒），命中帧前提示格挡/闪避 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Config")
	float ClashAttackTime = 0.8f;

	/** 先制攻击效果：开场拥有一层蓄力；默认敌方先制（敌方 1 层、我方 0 层），Satan 等 Boss 适用 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Config")
	bool bEnemyFirstStrike = true;

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

	/** 玩家选择行动（HUD 按钮 / 战斗输入回调统一入口）；返回是否接受该选择 */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	bool PlayerChooseAction(EBattleAction Action);

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
	EBattleAction GetEnemyChosenAction() const { return EnemyChosenAction; }

	UFUNCTION(BlueprintPure, Category = "Battle")
	bool IsPlayerExtraTurn() const { return bPlayerExtraTurnPending; }

	UFUNCTION(BlueprintPure, Category = "Battle")
	bool HasPlayerChosenAction() const { return bPlayerChoseAction; }

	/** 是否为序章教学战斗（BossEnemy.EnemyID == "apprentice_cave"） */
	UFUNCTION(BlueprintPure, Category = "Battle")
	bool IsTutorialBattle() const;

	/** 教学战斗当前引导提示（行动选择阶段显示当前教学点；碰撞阶段显示格挡/闪避提示） */
	UFUNCTION(BlueprintPure, Category = "Battle")
	FText GetTutorialHintText() const;

	/** 教学点锁定：条件教学点回合唯一允许的玩家行动（蓄力）；None = 不锁定（额外回合/锁定行动非法时回落不锁） */
	UFUNCTION(BlueprintPure, Category = "Battle")
	EBattleAction GetTutorialLockedPlayerAction() const;

	UFUNCTION(BlueprintPure, Category = "Battle")
	bool IsClashResolved() const { return bClashResolved; }

	/** 命中通知回调（由 UAnimNotify_CombatDamage 调用；事件驱动，战斗组件外部入口） */
	void OnHitNotify(ABaseCharacter* Attacker, FName EventName);

protected:
	UFUNCTION()
	void HandleIntroFinished(AActor* FinishedEnemy);

private:
	// ==================== 流程 ====================

	AEnemy* FindBossEnemy() const;
	/** 查找场景中所有 Tag=Boss 的敌人（同一关卡可存在多个 Boss/教学怪，入场结束后按完成者开战） */
	TArray<AEnemy*> FindBossEnemies() const;
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
	void FinishBattle(bool bPlayerWon, bool bFlee = false);
	void HandleVictoryCleanup();
	void HandleDefeatRestart();
	void ResetForRetry();
	/** 教学导演：每回合开始决定条件教学点（敌方行动覆盖 + 蓄力锁定 + 提示）；非教学战/额外回合跳过 */
	void UpdateTutorialDirector();
	/** 重置教学导演状态（失败重开时调用） */
	void ResetTutorialDirector();
	/** 教学必逃判定：脚本播完 或 敌方血量 <= RunAwayHPThreshold（已触发/非教学返回 false） */
	bool ShouldTriggerTutorialFlee() const;

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

	/** 蓄力抵抗姿态（循环动画）：已在播放则延续不重播；bBlockGate=true 时计入回合闸门并在当前循环播完后主动停止，防止额外回合动画打断 */
	void PlayChargeResistPose(ABaseCharacter* Character, const FAnimRef& AnimRef, bool bBlockGate);

	/** 按行动播放下一个动作动画（红防/蓝攻/白攻/蓄力） */
	void PlayActionAnim(bool bPlayer, EBattleAction Action);

	/** 非碰撞回合的动画编排：受击 > 金色反击 > 蓄力被打断 > 行动动画（双方各一次） */
	void PlayResolutionAnimations(const FTurnResolution& Resolution);

	/** 玩家进入/退出同色碰撞准备姿态 */
	void SetPlayerClashReady(bool bReady);

	/** 格挡成功：先播 Block（弹反），播完回调接 GoldCounter */
	void PlayBlockSuccessChain();

	/** 先播动作 Montage，播完（含被打断）再接反应动画；动作为空/缺失时直接播反应（玩家/敌人分槽） */
	void PlayAnimThenReaction(ABaseCharacter* Character, const FAnimRef& ActionRef, const FAnimRef& ReactionRef);

	/** 清理单侧待接反应：解绑回调并复位状态 */
	void ClearPendingReactionSide(bool bPlayer);

	/** 清理玩家/敌人两侧待接反应 */
	void ClearPendingReactions();

	/** 该蒙太奇是否仍在玩家/敌人任意一方的动画实例上活动（避免旧实例结束回调误删新实例的闸门条目） */
	bool IsMontageActiveOnCombatants(UAnimMontage* Montage) const;

	/** 格挡/闪避失败：优先对应 Fail 动画，空则回落 Hurt */
	void PlayClashFailReaction(EClashResult Result);

	/** 提前按下格挡（窗口外）：立即播放格挡动画（红防姿态） */
	void PlayBlockAnimNow();

	/** 动作 Montage 播完回调：接待接反应动画 */
	UFUNCTION()
	void OnActionMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** 战斗结束：败方播 Death，胜方播 Victory（空引用跳过） */
	void PlayDeathAnimations(bool bPlayerWon);

	/** 从行结构取某动作的动画引用（红防/蓝攻/白攻/蓄力；其他返回 nullptr） */
	const FAnimRef* GetActionRef(const FCombatAnimRow& Row, EBattleAction Action) const;

	/** 注册待命中事件（bPlayerAttacker=true 表示攻击者为玩家） */
	void RegisterPendingHit(bool bPlayerAttacker, FName EventName, ABaseCharacter* Target, float Amount,
		AActor* Causer, const FAnimRef& HitReaction, ABaseCharacter* Defender,
		const FAnimRef& DefenderReaction, const FAnimRef& DefenderFollowUp, UAnimMontage* FallbackMontage,
		bool bDefenderBlocked = false);

	/** 清理单侧待命中事件（含其防御反应定时器） */
	void ClearPendingHit(bool bPlayerAttacker);

	/** 清理两侧待命中事件（战斗结束/重试） */
	void ClearPendingHits();

	/** 立即消费单侧待命中事件（通知或回落路径调用；保证只结算一次） */
	void ApplyPendingHitNow(bool bPlayerAttacker);

	/** 扫描 Montage 中指定 EventName 通知/标记的时间（秒）；找不到返回 -1 */
	float GetNotifyTime(UAnimMontage* Montage, FName EventName) const;

	/** 通知/标记相对 Montage 播放起点的真实秒数（扣除 Section 起点并按 PlayRate 折算）；找不到返回 -1 */
	float GetNotifyRealTime(UAnimMontage* Montage, FName EventName, const FAnimRef& AnimRef) const;

	/** Montage 指定 Section 的起点时间（秒）；无 Section 或未找到返回 0 */
	float GetSectionStartTime(UAnimMontage* Montage, FName SectionName) const;

	/** 蓝 vs 红：按真实提前量（蓝攻命中时间 - 红防 GuardReady 时间）预排红防 → 接续动画 */
	void ScheduleDefenderReaction(const FPendingHitEvent& Hit, const FAnimRef& AttackRef);

	/** 停帧：暂停玩家/敌人活动 Montage，Duration 后恢复；Duration<=0 跳过 */
	void StartHitStop(float Duration);
	void EndHitStop();

	/** 普通回合：按攻击方注册命中事件并播放行动动画 */
	void RegisterSideHit(bool bPlayerAttacker, const FTurnResolution& Resolution);

	/** 结算层播放权：该侧行动是否被克制/打断（是则不播行动动画） */
	bool IsActionSuppressed(bool bPlayerSide, EBattleAction OtherAction) const;

	/** 蓝 vs 红：注册蓝攻命中事件（含防御反应预排与金色反击注册） */
	void RegisterBlueVsRedHit(bool bAttackerPlayer, float IncomingAmount, bool bCounterSucceeds);

	/** 完整播出链播完且无待命中/待接反应时推进回合（否则保持挂起） */
	void TryAdvanceTurnIfGateDone();

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
	float GetClashInputCooldown() const;
	float GetBlockFailLockout() const;
	float GetRunAwayHPThreshold() const;
	float GetClashTimeDilation() const;
	/** 同色碰撞可格挡期间启用世界时间慢放（0.05）；已慢放则跳过 */
	void ApplyClashTimeDilation();
	/** 恢复世界时间流速（玩家点击格挡/闪避、碰撞结束、战斗清理时调用） */
	void RestoreTimeDilation();
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
	bool bTutorialFleeTriggered = false;
	/** 教学引导：进入战斗后的首条总提示（玩家首次选择行动后清除） */
	bool bTutorialInitialHintPending = false;
	/** 教学点：蓄力到上限破红防（玩家 1 层 + 敌方红防） */
	bool bTutorialChargeCapTaught = false;
	/** 教学点：蓄力抵抗白攻获得额外回合（玩家 0 层 + 敌方白攻） */
	bool bTutorialChargeResistTaught = false;
	/** 教学战玩家是否已使用过白攻（第一次使用白攻时强制敌方同步白攻，触发白白碰撞教学） */
	bool bTutorialFirstWhiteAttackUsed = false;
	/** 教学点进行中：本回合锁定玩家只能使用蓄力 */
	bool bTutorialChargeLockActive = false;
	/** 教学导演覆盖的敌方行动（None = 随机 AI） */
	EBattleAction TutorialForcedEnemyAction = EBattleAction::None;
	/** 当前教学点提示文字（非教学点回合为空） */
	FText TutorialActiveHint;
	bool bTimeDilationApplied = false;
	float OriginalTimeDilation = 1.0f;

	bool bClashWindowOpen = false;
	bool bClashResolved = false;
	bool bClashStarted = false;
	/** 玩家/敌人待接反应（动作 Montage 播完后触发；支持双方同时排队） */
	bool bPlayerReactionPending = false;
	UAnimMontage* PlayerPendingActionMontage = nullptr;
	FAnimRef PlayerPendingReactionRef;

	bool bEnemyReactionPending = false;
	UAnimMontage* EnemyPendingActionMontage = nullptr;
	FAnimRef EnemyPendingReactionRef;
	FPendingHitEvent PlayerPendingHit;
	FPendingHitEvent EnemyPendingHit;
	/** 通用回合闸门：本结算的动画完整播出链（行动+命中反应）播完才推进回合 */
	bool bTurnGateOpen = false;
	TSet<UAnimMontage*> GatedMontages;
	bool bHitStopActive = false;
	float HitStopRemaining = 0.0f;
	TArray<TWeakObjectPtr<UAnimInstance>> HitStopInstances;
	TArray<UAnimMontage*> HitStopMontages;
	FTimerHandle PlayerDefenderTimer;
	FTimerHandle EnemyDefenderTimer;
	EClashType ActiveClashType = EClashType::None;
	EClashResult PendingClashResult = EClashResult::None;
	float PendingIncomingDamage = 0.0f;
	float PendingOutgoingDamage = 0.0f;
	/** 碰撞命中时间（来自敌方 ClashAttackHit 通知；无通知 = ClashAttackTime） */
	float ClashHitTime = 0.0f;
	/** 碰撞开始时的世界时间（秒），输入判定用"当前世界时间 − 该值"与 ClashHitTime 比较 */
	float ClashStartTime = 0.0f;
	/** 上次格挡/闪避输入时间（秒），用于 ClashInputCooldown 防连按 */
	float LastClashInputTime = -1.0f;
	/** 上次格挡失败时间（相对碰撞开始，秒），用于 BlockFailLockoutSeconds 再次格挡锁定 */
	float LastBlockFailTime = -1.0f;

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
	FTimerHandle ChargePoseTimer;
	FTimerHandle BlueAttackDelayTimer;

	bool bCombatInputBound = false;
	bool bCombatMappingAdded = false;
	bool bMouseCursorShown = false;
};
