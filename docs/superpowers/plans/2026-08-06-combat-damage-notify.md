# Combat Damage Notify Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把所有伤害结算绑定到动画命中通知：攻击 Montage 挂 `UAnimNotify_CombatDamage`，回合结算只注册待命中事件，命中帧才扣血、播受击/防御反应并处理死亡；格挡/闪避窗口以命中通知为锚点并加入输入冷却。

**Architecture:** 沿用三层架构：伤害公式仍在 `UCombatFormulaSubsystem`；`UBattleComponent` 持有玩家/敌人两侧待命中事件槽（`FPendingHitEvent`：伤害 + 防御反应），通知回调 `OnHitNotify` 消费槽；蓝 vs 红时按"蓝攻命中时间 − 红防 GuardReady 标记时间"预排红防动画；碰撞窗口从敌方前摇 Montage 的 `ClashTelegraphHit` 通知时间推导。

**Tech Stack:** Unreal Engine 5.6 / C++（Hole 模块）、UE AnimNotify、Python 脚本（`Scripts/create_datatables.py`）。

## Global Constraints

（以下约束隐含于每个任务，执行时不重复列出）

- **构建命令（每次改 C++ 后立即编译）：**
  ```
  D:\Software\UnrealEngine\UE_5.6\Engine\Build\BatchFiles\Build.bat HoleEditor Win64 Development "d:\UE5\UE_project\Kami\Hole\Hole.uproject"
  ```
  期望输出：0 error / 0 new warning。
- **重试上限 3 次：** 失败即停止并把完整错误发给用户决策。
- **三层架构：** USTRUCT 纯数据；公式在 Subsystem；播放/注册/消费在 `UBattleComponent`。
- **伤害只结算一次：** 待命中事件槽由"命中通知"或"回落路径"二选一消费（先到先得），禁止双重结算。
- **回合推进保持解耦（v1）：** 不阻塞 `EndTurnAndAdvance`；命中通知若被下一动作打断，回落在 Montage 结束（含被打断）时结算，保证伤害不丢。PIE 验证后再决定是否改为"等动画播完再推进"。
- **回落约定：** 动作 Montage 无命中通知 → 播完（含打断）结算 + 警告日志；碰撞前摇无 `ClashTelegraphHit` → 沿用 `ClashTelegraphTime` 计时器。
- **通知类目录约定：** `Animation/AnimNotifies/Combat/`；不按 Montage 命名。
- **编辑器资产操作由用户手动完成**；不运行 `create_datatables.py` 重建命令。
- **测试方式：** 无自动化测试框架；每个任务以"编译通过 + PIE 验证清单"为闭环。
- **提交规范：** 每任务独立提交；message 前缀 `feat(combat-dmg):` / `fix(combat-dmg):` / `docs(combat-dmg):`；写 `.git` 需申请升级权限。

## File Structure

| 文件 | 责任 | 动作 |
|---|---|---|
| `Hole/Source/Hole/Public/DataTable/CombatParamsTable.h` | 新增 `ClashInputCooldown`、`RedDefenseLeadTime` | 修改（Task 1） |
| `Hole/Source/Hole/Public/DataTable/CombatAnimConfigTable.h` | `FCombatAnimRow` 新增 `BlockedReaction` 列 | 修改（Task 1） |
| `Scripts/create_datatables.py` | 参数表新增两列与默认值 | 修改（Task 1） |
| `Hole/Source/Hole/Public/Animation/AnimNotifies/Combat/AnimNotify_CombatDamage.h` + `Private/...cpp` | 命中通知（EventName → BattleComponent::OnHitNotify） | 新建（Task 2） |
| `Hole/Source/Hole/Public/Animation/AnimNotifies/Combat/AnimNotify_CombatMarker.h` + `Private/...cpp` | 无伤害标记（GuardReady） | 新建（Task 2） |
| `Hole/Source/Hole/Public/Combat/BattleComponent.h` + `Private/Combat/BattleComponent.cpp` | 待命中事件槽、注册/消费/回落、蓝红预排、碰撞窗口重做、输入冷却、停帧与被格挡动画 | 修改（Task 3-6） |
| 编辑器资产 | 给攻击/前摇/红防 Montage 挂通知 | 用户手动（Task 8） |
| `DataTable_Spec.md` / `GDD_Outline.md` / `AGENTS.md` / `DevLog.md` | 参数、规则、约定同步 | 修改（Task 7） |

---

### Task 1: 参数与动画表列扩展（ClashInputCooldown / RedDefenseLeadTime / HitStopDuration / BlockedReaction）

**Files:**
- Modify: `Hole/Source/Hole/Public/DataTable/CombatParamsTable.h`
- Modify: `Hole/Source/Hole/Public/DataTable/CombatAnimConfigTable.h`
- Modify: `Scripts/create_datatables.py`

**Interfaces:**
- Produces: `FCombatParamsRow::ClashInputCooldown`（0.15f）、`RedDefenseLeadTime`（0.3f）、`HitStopDuration`（0.12f）；`FCombatAnimRow::BlockedReaction`（FAnimRef）；脚本 `COMBATPARAMS_HEADERS`/`combat_params` 行与 `ANIM_COLUMNS` 同步。

- [ ] **Step 1: 头文件新增字段**

在 `CombatParamsTable.h` 的 `// ---- 防御/操作 ----` 区（`DodgeWindowSeconds` 后）插入：

```cpp
	/** 格挡/闪避输入冷却（秒），防止连按；[PLAYTEST] */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Defense")
	float ClashInputCooldown = 0.15f;

	/** 红防举剑标记（GuardReady）缺失时的回落提前量（秒），用于蓝 vs 红防御反应预排；[PLAYTEST] */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Defense")
	float RedDefenseLeadTime = 0.3f;

	/** 格挡/闪避/红防反击成功时的停帧时长（秒），0 关闭；[PLAYTEST] */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Defense")
	float HitStopDuration = 0.12f;
```

- [ ] **Step 2: FCombatAnimRow 新增 BlockedReaction 列**

在 `CombatAnimConfigTable.h` 的 `FCombatAnimRow` 中（`Hurt` 列前）插入：

```cpp
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Reaction")
	FAnimRef BlockedReaction;
```

- [ ] **Step 3: 脚本新增列与默认值**

在 `Scripts/create_datatables.py` 的 `COMBATPARAMS_HEADERS` 中 `"DodgeFailDamageScale"` 后加：

```python
    "ClashInputCooldown", "RedDefenseLeadTime", "HitStopDuration",
```

在 `combat_params` 行中 `"DodgeFailDamageScale": 1.2,` 后加：

```python
        "ClashInputCooldown": 0.15, "RedDefenseLeadTime": 0.3, "HitStopDuration": 0.12,
```

在 `ANIM_COLUMNS` 列表（`"Hurt"` 前）加：

```python
    "BlockedReaction",
```

- [ ] **Step 4: 编译 + Python 语法检查**

Run: 构建命令；`python -m py_compile Scripts/create_datatables.py`
Expected: 均通过。

- [ ] **Step 5: 提交**

```bash
git add Hole/Source/Hole/Public/DataTable/CombatParamsTable.h Hole/Source/Hole/Public/DataTable/CombatAnimConfigTable.h Scripts/create_datatables.py
git commit -m "feat(combat-dmg): add hit-stop and blocked-reaction params and anim column"
```

---

### Task 2: 命中通知与标记通知类

**Files:**
- Create: `Hole/Source/Hole/Public/Animation/AnimNotifies/Combat/AnimNotify_CombatDamage.h` + `Hole/Source/Hole/Private/Animation/AnimNotifies/Combat/AnimNotify_CombatDamage.cpp`
- Create: `Hole/Source/Hole/Public/Animation/AnimNotifies/Combat/AnimNotify_CombatMarker.h` + `Hole/Source/Hole/Private/Animation/AnimNotifies/Combat/AnimNotify_CombatMarker.cpp`

**Interfaces:**
- Produces: `UAnimNotify_CombatDamage::EventName`（FName）+ `Notify` 回调；`UAnimNotify_CombatMarker::MarkerName`（FName）。两者供 Task 3 的 `GetNotifyTime` 扫描与 `OnHitNotify` 消费。

- [ ] **Step 1: 创建命中通知头文件**

```cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_CombatDamage.generated.h"

/**
 * 战斗命中通知：挂在攻击/前摇 Montage 的挥击帧。
 * 触发时找到玩家的 UBattleComponent 并调用 OnHitNotify(攻击者, EventName)。
 */
UCLASS()
class HOLE_API UAnimNotify_CombatDamage : public UAnimNotify
{
	GENERATED_BODY()

public:
	/** 与待命中事件槽匹配的事件名（WhiteAttackHit / BlueAttackHit / GoldCounterHit / ClashTelegraphHit） */
	UPROPERTY(EditAnywhere, Category = "Combat")
	FName EventName;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
```

- [ ] **Step 2: 创建命中通知实现**

```cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifies/Combat/AnimNotify_CombatDamage.h"
#include "Combat/BattleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

void UAnimNotify_CombatDamage::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}
	AActor* Attacker = MeshComp->GetOwner();
	UWorld* World = MeshComp->GetWorld();
	if (!World)
	{
		return;
	}
	APlayerController* PC = World->GetFirstPlayerController();
	APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
	if (!PlayerPawn)
	{
		return;
	}
	if (UBattleComponent* Battle = PlayerPawn->FindComponentByClass<UBattleComponent>())
	{
		Battle->OnHitNotify(Attacker, EventName);
	}
}
```

- [ ] **Step 3: 创建标记通知头文件**

```cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_CombatMarker.generated.h"

/**
 * 战斗标记通知：标注无伤害关键帧（如红防的 GuardReady 举剑帧），
 * 仅供预排计算扫描，Notify 不触发战斗逻辑。
 */
UCLASS()
class HOLE_API UAnimNotify_CombatMarker : public UAnimNotify
{
	GENERATED_BODY()

public:
	/** 标记名，如 GuardReady */
	UPROPERTY(EditAnywhere, Category = "Combat")
	FName MarkerName;
};
```

- [ ] **Step 4: 创建标记通知实现（空实现）**

```cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifies/Combat/AnimNotify_CombatMarker.h"

// 空实现：标记仅用于 GetNotifyTime 扫描
```

- [ ] **Step 5: 编译**

Run: 构建命令
Expected: 0 error / 0 new warning。

- [ ] **Step 6: 提交**

```bash
git add Hole/Source/Hole/Public/Animation/AnimNotifies/Combat Hole/Source/Hole/Private/Animation/AnimNotifies/Combat
git commit -m "feat(combat-dmg): add combat damage and marker anim notifies"
```

---

### Task 3: 待命中事件核心 + 普通回合注册/消费/回落

**Files:**
- Modify: `Hole/Source/Hole/Public/Combat/BattleComponent.h`
- Modify: `Hole/Source/Hole/Private/Combat/BattleComponent.cpp`

**Interfaces:**
- Consumes: Task 1 参数、Task 2 通知类。
- Produces: `struct FPendingHitEvent`（每侧一槽，含 `bDefenderBlocked`）、`RegisterPendingHit(...)`、`ClearPendingHit(bool)`、`ApplyPendingHitNow(bool)`、`OnHitNotify(ABaseCharacter*, FName)`、`GetNotifyTime(UAnimMontage*, FName)`、`GetActionRef(...)`；`PlayCombatAnim` 统一绑定 `OnActionMontageEnded`；`OnActionMontageEnded` 增加待命中事件回落；`ApplyResolution`/`PlayResolutionAnimations` 改为注册+播行动（不再立即扣血/播受击）。

- [ ] **Step 1: 头文件增加结构体与方法声明**

在 `BattleComponent.h` 的类声明前插入：

```cpp
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
```

在"战斗动画"私有区（`ClearPendingReactions` 后）增加：

```cpp
	/** 从行结构取某动作的动画引用（红防/蓝攻/白攻/蓄力；其他返回 nullptr） */
	const FAnimRef* GetActionRef(const FCombatAnimRow& Row, EBattleAction Action) const;

	/** 注册待命中事件（bPlayerAttacker=true 表示攻击者为玩家） */
	void RegisterPendingHit(bool bPlayerAttacker, FName EventName, ABaseCharacter* Target, float Amount,
		AActor* Causer, const FAnimRef& HitReaction, ABaseCharacter* Defender,
		const FAnimRef& DefenderReaction, const FAnimRef& DefenderFollowUp, UAnimMontage* FallbackMontage,
		bool bDefenderBlocked = false);

	/** 停帧：暂停玩家/敌人活动 Montage，Duration 后恢复；Duration<=0 跳过 */
	void StartHitStop(float Duration);

	/** 清理单侧待命中事件（含其防御反应定时器） */
	void ClearPendingHit(bool bPlayerAttacker);

	/** 清理两侧待命中事件（战斗结束/重试） */
	void ClearPendingHits();

	/** 立即消费单侧待命中事件（通知或回落路径调用；保证只结算一次） */
	void ApplyPendingHitNow(bool bPlayerAttacker);

	/** 命中通知回调（由 UAnimNotify_CombatDamage 调用） */
	void OnHitNotify(ABaseCharacter* Attacker, FName EventName);

	/** 扫描 Montage 中指定 EventName 通知/标记的时间（秒）；找不到返回 -1 */
	float GetNotifyTime(UAnimMontage* Montage, FName EventName) const;
```

在私有状态区（`bEnemyReactionPending` 附近）增加：

```cpp
	FPendingHitEvent PlayerPendingHit;
	FPendingHitEvent EnemyPendingHit;
	FTimerHandle HitStopTimer;
```

- [ ] **Step 2: cpp 增加 include**

在 `BattleComponent.cpp` include 区加：

```cpp
#include "Animation/AnimNotifies/Combat/AnimNotify_CombatDamage.h"
#include "Animation/AnimNotifies/Combat/AnimNotify_CombatMarker.h"
```

- [ ] **Step 3: 实现 GetActionRef / GetNotifyTime / 注册与消费**

在 `PlayResolutionAnimations` 定义前插入：

```cpp
const FAnimRef* UBattleComponent::GetActionRef(const FCombatAnimRow& Row, EBattleAction Action) const
{
	switch (Action)
	{
	case EBattleAction::RedDefense: return &Row.RedDefense;
	case EBattleAction::BlueAttack: return &Row.BlueAttack;
	case EBattleAction::WhiteAttack: return &Row.WhiteAttack;
	case EBattleAction::Charge: return &Row.Charge;
	default: return nullptr;
	}
}

float UBattleComponent::GetNotifyTime(UAnimMontage* Montage, FName EventName) const
{
	if (!Montage)
	{
		return -1.0f;
	}
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		if (!Event.IsValidNotify())
		{
			continue;
		}
		if (const UAnimNotify_CombatDamage* DamageNotify = Cast<UAnimNotify_CombatDamage>(Event.Notify))
		{
			if (DamageNotify->EventName == EventName)
			{
				return Event.GetTime();
			}
		}
		if (const UAnimNotify_CombatMarker* Marker = Cast<UAnimNotify_CombatMarker>(Event.Notify))
		{
			if (Marker->MarkerName == EventName)
			{
				return Event.GetTime();
			}
		}
	}
	return -1.0f;
}

void UBattleComponent::RegisterPendingHit(bool bPlayerAttacker, FName EventName, ABaseCharacter* Target, float Amount,
	AActor* Causer, const FAnimRef& HitReaction, ABaseCharacter* Defender,
	const FAnimRef& DefenderReaction, const FAnimRef& DefenderFollowUp, UAnimMontage* FallbackMontage,
	bool bDefenderBlocked)
{
	FPendingHitEvent& Hit = bPlayerAttacker ? PlayerPendingHit : EnemyPendingHit;
	ClearPendingHit(bPlayerAttacker);
	Hit.bActive = true;
	Hit.EventName = EventName;
	Hit.Target = Target;
	Hit.Amount = Amount;
	Hit.Causer = Causer;
	Hit.HitReaction = HitReaction;
	Hit.Defender = Defender;
	Hit.DefenderReaction = DefenderReaction;
	Hit.DefenderFollowUp = DefenderFollowUp;
	Hit.FallbackMontage = FallbackMontage;
	Hit.bDefenderBlocked = bDefenderBlocked;
}

void UBattleComponent::ClearPendingHit(bool bPlayerAttacker)
{
	FPendingHitEvent& Hit = bPlayerAttacker ? PlayerPendingHit : EnemyPendingHit;
	Hit = FPendingHitEvent();
}

void UBattleComponent::ClearPendingHits()
{
	ClearPendingHit(true);
	ClearPendingHit(false);
}

void UBattleComponent::ApplyPendingHitNow(bool bPlayerAttacker)
{
	FPendingHitEvent& Hit = bPlayerAttacker ? PlayerPendingHit : EnemyPendingHit;
	if (!Hit.bActive)
	{
		return;
	}
	ABaseCharacter* Target = Hit.Target;
	const FAnimRef HitReaction = Hit.HitReaction;
	const float Amount = Hit.Amount;
	AActor* Causer = Hit.Causer;
	const bool bBlocked = Hit.bDefenderBlocked;
	ClearPendingHit(bPlayerAttacker);

	if (Target && Amount > 0.0f)
	{
		ApplyDamageTo(Target, Amount, Causer);
		if (!Target->IsDead() && !HitReaction.Montage.IsNull())
		{
			PlayCombatAnim(Target, HitReaction);
		}
	}

	// 被格挡：攻击者立即混入 BlockedReaction + 停帧
	if (bBlocked && Causer)
	{
		if (ABaseCharacter* Attacker = Cast<ABaseCharacter>(Causer))
		{
			if (!Attacker->IsDead())
			{
				const bool bAttackerPlayer = (Attacker == PlayerRole.Get());
				if (const FCombatAnimRow* Row = GetCombatAnimRow(bAttackerPlayer))
				{
					PlayCombatAnim(Attacker, Row->BlockedReaction);
				}
			}
		}
		const FCombatParamsRow Defaults;
		const FCombatParamsRow* Params = GetCombatSubsystem() ? GetCombatSubsystem()->GetCombatParams() : nullptr;
		const FCombatParamsRow& P = Params ? *Params : Defaults;
		StartHitStop(P.HitStopDuration);
	}
}

void UBattleComponent::OnHitNotify(ABaseCharacter* Attacker, FName EventName)
{
	if (!Attacker)
	{
		return;
	}
	const bool bPlayerAttacker = (Attacker == PlayerRole.Get());
	FPendingHitEvent& Hit = bPlayerAttacker ? PlayerPendingHit : EnemyPendingHit;
	if (!Hit.bActive || Hit.EventName != EventName)
	{
		return;
	}
	ApplyPendingHitNow(bPlayerAttacker);
}
```

- [ ] **Step 3b: 实现 StartHitStop（暂停/恢复双方 Montage）**

在 `OnHitNotify` 实现后插入：

```cpp
void UBattleComponent::StartHitStop(float Duration)
{
	UWorld* World = GetWorld();
	if (Duration <= 0.0f || !World)
	{
		return;
	}
	TArray<UAnimInstance*> PausedInstances;
	if (PlayerRole.IsValid())
	{
		if (UAnimInstance* AI = PlayerRole->GetMesh()->GetAnimInstance())
		{
			AI->Montage_Pause();
			PausedInstances.Add(AI);
		}
	}
	if (BossEnemy.IsValid())
	{
		if (UAnimInstance* AI = BossEnemy->GetMesh()->GetAnimInstance())
		{
			AI->Montage_Pause();
			PausedInstances.Add(AI);
		}
	}
	World->GetTimerManager().SetTimer(HitStopTimer, [PausedInstances]()
	{
		for (UAnimInstance* AI : PausedInstances)
		{
			if (AI)
			{
				AI->Montage_Resume();
			}
		}
	}, Duration, false);
}
```

- [ ] **Step 4: PlayCombatAnim 统一绑定 Montage 结束回调（保证回落可达）**

将 `PlayCombatAnim` 中 `Character->PlayAnimMontage(...)` 前插入：

```cpp
	if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
	{
		AnimInstance->OnMontageEnded.RemoveDynamic(this, &UBattleComponent::OnActionMontageEnded);
		AnimInstance->OnMontageEnded.AddDynamic(this, &UBattleComponent::OnActionMontageEnded);
	}
```

- [ ] **Step 5: OnActionMontageEnded 增加待命中事件回落**

在 `OnActionMontageEnded` 末尾（两侧待接反应检查之后）追加：

```cpp
	// 待命中事件回落：动作蒙太奇播完（含被打断）仍未触发通知 → 结算，保证伤害不丢
	if (PlayerPendingHit.bActive && PlayerPendingHit.FallbackMontage == Montage)
	{
		ApplyPendingHitNow(true);
	}
	if (EnemyPendingHit.bActive && EnemyPendingHit.FallbackMontage == Montage)
	{
		ApplyPendingHitNow(false);
	}
```

- [ ] **Step 6: ApplyResolution 非碰撞分支改为只播动画（注册在 PlayResolutionAnimations 内）**

将 `ApplyResolution` 非碰撞分支中两个 `ApplyDamageTo` 调用块整体替换为：

```cpp
	// 伤害延迟到动画命中通知：注册与播放由 PlayResolutionAnimations 负责
	if (Phase != EBattlePhase::Ended)
	{
		PlayResolutionAnimations(Resolution);
	}
```

- [ ] **Step 7: PlayResolutionAnimations 重写为"注册 + 播行动"**

将整个 `PlayResolutionAnimations` 函数体替换为：

```cpp
void UBattleComponent::PlayResolutionAnimations(const FTurnResolution& Resolution)
{
	if (!PlayerRole.IsValid() || !BossEnemy.IsValid())
	{
		return;
	}

	const EBattleAction PlayerAction = PlayerLastAction;
	const EBattleAction EnemyAction = EnemyChosenAction;

	// 蓝 vs 红（含红防反击/2 层正面承受）：专用注册路径（Task 4 实现）
	if (PlayerAction == EBattleAction::RedDefense && EnemyAction == EBattleAction::BlueAttack)
	{
		RegisterBlueVsRedHit(false, Resolution.PlayerDamageTaken, Resolution.PlayerDamageTaken <= 0.0f);
		return;
	}
	if (EnemyAction == EBattleAction::RedDefense && PlayerAction == EBattleAction::BlueAttack)
	{
		RegisterBlueVsRedHit(true, Resolution.PlayerDamageTaken, Resolution.EnemyDamageTaken <= 0.0f);
		return;
	}

	// 普通回合：按攻击方注册命中事件并播行动动画（受击/蓄力反应在命中帧播放）
	RegisterSideHit(true, Resolution);
	RegisterSideHit(false, Resolution);
}

void UBattleComponent::RegisterSideHit(bool bPlayerAttacker, const FTurnResolution& Resolution)
{
	const EBattleAction AttackerAction = bPlayerAttacker ? PlayerLastAction : EnemyChosenAction;
	const bool bTargetInterrupted = bPlayerAttacker ? Resolution.bEnemyChargeInterrupted : Resolution.bPlayerChargeInterrupted;
	const bool bTargetResist = bPlayerAttacker ? Resolution.bEnemyExtraTurn : Resolution.bPlayerExtraTurn;
	const float Amount = bPlayerAttacker ? Resolution.EnemyDamageTaken : Resolution.PlayerDamageTaken;

	ABaseCharacter* Attacker = bPlayerAttacker ? Cast<ABaseCharacter>(PlayerRole.Get()) : Cast<ABaseCharacter>(BossEnemy.Get());
	ABaseCharacter* Target = bPlayerAttacker ? Cast<ABaseCharacter>(BossEnemy.Get()) : Cast<ABaseCharacter>(PlayerRole.Get());
	const FCombatAnimRow* AttackerRow = GetCombatAnimRow(bPlayerAttacker);
	const FCombatAnimRow* TargetRow = GetCombatAnimRow(!bPlayerAttacker);
	if (!Attacker || !Target || !AttackerRow || !TargetRow)
	{
		return;
	}

	FName EventName = NAME_None;
	switch (AttackerAction)
	{
	case EBattleAction::BlueAttack: EventName = FName(TEXT("BlueAttackHit")); break;
	case EBattleAction::WhiteAttack: EventName = FName(TEXT("WhiteAttackHit")); break;
	default: return;
	}

	const FAnimRef* ActionRef = GetActionRef(*AttackerRow, AttackerAction);
	if (ActionRef)
	{
		PlayCombatAnim(Attacker, *ActionRef);
	}

	FAnimRef HitReaction;
	if (bTargetInterrupted)
	{
		HitReaction = TargetRow->ChargeInterrupted;
	}
	else if (bTargetResist)
	{
		HitReaction = TargetRow->Charge;
	}
	else
	{
		HitReaction = TargetRow->Hurt;
	}

	RegisterPendingHit(bPlayerAttacker, EventName, Target, Amount, Attacker, HitReaction,
		nullptr, FAnimRef(), FAnimRef(),
		ActionRef ? ActionRef->Montage.LoadSynchronous() : nullptr, false);
}
```

`RegisterBlueVsRedHit` 在本任务先以空实现占位（Task 4 补齐），保证本任务可编译：

```cpp
void UBattleComponent::RegisterBlueVsRedHit(bool bAttackerPlayer, float IncomingAmount, bool bCounterSucceeds)
{
	// Task 4 补齐：注册蓝攻命中事件（伤害 + 防御反应预排）与金色反击注册
}
```

- [ ] **Step 8: 头文件增加 RegisterSideHit / RegisterBlueVsRedHit 声明**

```cpp
	/** 普通回合：按攻击方注册命中事件并播放行动动画 */
	void RegisterSideHit(bool bPlayerAttacker, const FTurnResolution& Resolution);

	/** 蓝 vs 红：注册蓝攻命中事件（含防御反应预排与金色反击注册） */
	void RegisterBlueVsRedHit(bool bAttackerPlayer, float IncomingAmount, bool bCounterSucceeds);
```

- [ ] **Step 9: 编译**

Run: 构建命令
Expected: 0 error / 0 new warning（空实现占位允许）。

- [ ] **Step 10: 提交**

```bash
git add Hole/Source/Hole/Public/Combat/BattleComponent.h Hole/Source/Hole/Private/Combat/BattleComponent.cpp
git commit -m "feat(combat-dmg): defer damage to hit notifies with pending hit slots and fallback"
```

---

### Task 4: 蓝 vs 红防御反应预排与金色反击注册

**Files:**
- Modify: `Hole/Source/Hole/Private/Combat/BattleComponent.cpp`
- Modify: `Hole/Source/Hole/Public/Combat/BattleComponent.h`

**Interfaces:**
- Consumes: Task 3 的 `RegisterPendingHit` / `GetNotifyTime` / `PlayAnimThenReaction`、Task 1 的 `RedDefenseLeadTime`。
- Produces: `RegisterBlueVsRedHit` 完整实现；`ScheduleDefenderReaction(const FPendingHitEvent&)`；`PlayerDefenderTimer` / `EnemyDefenderTimer`。

- [ ] **Step 1: 头文件增加声明与定时器**

在"战斗动画"私有区（`GetNotifyTime` 后）加：

```cpp
	/** 蓝 vs 红：按提前量（蓝攻命中时间 - 红防 GuardReady 时间）预排红防 → 接续动画 */
	void ScheduleDefenderReaction(const FPendingHitEvent& Hit);
```

在私有状态区（`EnemyPendingHit` 附近）加：

```cpp
	FTimerHandle PlayerDefenderTimer;
	FTimerHandle EnemyDefenderTimer;
```

- [ ] **Step 2: 实现 RegisterBlueVsRedHit 与 ScheduleDefenderReaction**

在 `RegisterSideHit` 实现后插入：

```cpp
void UBattleComponent::RegisterBlueVsRedHit(bool bAttackerPlayer, float IncomingAmount, bool bCounterSucceeds)
{
	ABaseCharacter* Attacker = bAttackerPlayer ? Cast<ABaseCharacter>(PlayerRole.Get()) : Cast<ABaseCharacter>(BossEnemy.Get());
	ABaseCharacter* Defender = bAttackerPlayer ? Cast<ABaseCharacter>(BossEnemy.Get()) : Cast<ABaseCharacter>(PlayerRole.Get());
	const FCombatAnimRow* AttackerRow = GetCombatAnimRow(bAttackerPlayer);
	const FCombatAnimRow* DefenderRow = GetCombatAnimRow(!bAttackerPlayer);
	if (!Attacker || !Defender || !AttackerRow || !DefenderRow)
	{
		return;
	}

	const FAnimRef* BlueRef = GetActionRef(*AttackerRow, EBattleAction::BlueAttack);
	if (BlueRef)
	{
		PlayCombatAnim(Attacker, *BlueRef);
	}

	FAnimRef DefenderFollowUp = bCounterSucceeds ? DefenderRow->GoldCounter : DefenderRow->Hurt;
	RegisterPendingHit(bAttackerPlayer, FName(TEXT("BlueAttackHit")), Defender, IncomingAmount, Attacker,
		FAnimRef(), Defender, DefenderRow->RedDefense, DefenderFollowUp,
		BlueRef ? BlueRef->Montage.LoadSynchronous() : nullptr, bCounterSucceeds);
	ScheduleDefenderReaction(bAttackerPlayer ? PlayerPendingHit : EnemyPendingHit);

	if (bCounterSucceeds)
	{
		const float GoldAmount = bAttackerPlayer ? GetEnemyGoldDamage() : GetPlayerGoldDamage();
		const FAnimRef& GoldCounterRef = DefenderRow->GoldCounter;
		RegisterPendingHit(!bAttackerPlayer, FName(TEXT("GoldCounterHit")), Attacker, GoldAmount, Defender,
			AttackerRow->Hurt, nullptr, FAnimRef(), FAnimRef(),
			GoldCounterRef.Montage.IsNull() ? nullptr : GoldCounterRef.Montage.LoadSynchronous(), false);
	}
}

void UBattleComponent::ScheduleDefenderReaction(const FPendingHitEvent& Hit)
{
	if (!Hit.Defender || Hit.DefenderReaction.Montage.IsNull())
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const bool bPlayerDefender = (Hit.Defender == PlayerRole.Get());
	FTimerHandle& Timer = bPlayerDefender ? PlayerDefenderTimer : EnemyDefenderTimer;
	World->GetTimerManager().ClearTimer(Timer);

	const float HitTime = GetNotifyTime(Hit.FallbackMontage, FName(TEXT("BlueAttackHit")));
	const float GuardReadyTime = GetNotifyTime(Hit.DefenderReaction.Montage.LoadSynchronous(), FName(TEXT("GuardReady")));
	float Delay = 0.0f;
	if (HitTime > 0.0f && GuardReadyTime >= 0.0f)
	{
		Delay = FMath::Max(0.0f, HitTime - GuardReadyTime);
	}
	else if (HitTime > 0.0f)
	{
		const FCombatParamsRow Defaults;
		const FCombatParamsRow* Params = GetCombatSubsystem() ? GetCombatSubsystem()->GetCombatParams() : nullptr;
		const FCombatParamsRow& P = Params ? *Params : Defaults;
		Delay = FMath::Max(0.0f, HitTime - P.RedDefenseLeadTime);
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::ScheduleDefenderReaction - 红防无 GuardReady 标记，使用 RedDefenseLeadTime 回落"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::ScheduleDefenderReaction - 蓝攻无 BlueAttackHit 通知，红防立即启动"));
	}

	World->GetTimerManager().SetTimer(Timer, [this, Defender = Hit.Defender,
		Reaction = Hit.DefenderReaction, FollowUp = Hit.DefenderFollowUp]()
	{
		if (Phase != EBattlePhase::Ended)
		{
			PlayAnimThenReaction(Defender, Reaction, FollowUp);
		}
	}, Delay, false);
}
```

- [ ] **Step 3: ClearPendingHit 同时清理对应防御反应定时器**

将 `ClearPendingHit` 实现替换为：

```cpp
void UBattleComponent::ClearPendingHit(bool bPlayerAttacker)
{
	FPendingHitEvent& Hit = bPlayerAttacker ? PlayerPendingHit : EnemyPendingHit;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(bPlayerAttacker ? PlayerDefenderTimer : EnemyDefenderTimer);
	}
	Hit = FPendingHitEvent();
}
```

- [ ] **Step 4: 编译**

Run: 构建命令
Expected: 0 error / 0 new warning。

- [ ] **Step 5: 提交**

```bash
git add Hole/Source/Hole/Public/Combat/BattleComponent.h Hole/Source/Hole/Private/Combat/BattleComponent.cpp
git commit -m "feat(combat-dmg): schedule red defense to align guard-ready with blue attack hit"
```

---

### Task 5: 碰撞窗口重做（命中通知锚点 + 每键窗口 + 输入冷却）

**Files:**
- Modify: `Hole/Source/Hole/Private/Combat/BattleComponent.cpp`
- Modify: `Hole/Source/Hole/Public/Combat/BattleComponent.h`

**Interfaces:**
- Consumes: Task 3 待命中事件槽、Task 1 `ClashInputCooldown`、现有 `BlockWindowSeconds`/`DodgeWindowSeconds`。
- Produces: `ClashHitTime`、`LastClashInputTime`；`StartClash` 注册 `ClashTelegraphHit` 待命中事件；`ResolveClash` 只决定结果与金额、不立即扣血；`OnBlockPressed`/`OnDodgePressed` 每键独立窗口 + 冷却。

- [ ] **Step 1: 头文件增加状态**

在私有状态区（`PendingOutgoingDamage` 附近）加：

```cpp
	/** 碰撞命中时间（来自敌方前摇 ClashTelegraphHit 通知；无通知 = ClashTelegraphTime） */
	float ClashHitTime = 0.0f;
	/** 上次格挡/闪避输入时间（秒），用于 ClashInputCooldown 防连按 */
	float LastClashInputTime = -1.0f;
```

- [ ] **Step 2: StartClash 注册敌方前摇命中事件并锚定 HitTime**

在 `StartClash` 的 `SetPhase(EBattlePhase::Clash);` 之后、现有动画块位置，将动画/计时块替换为：

```cpp
	// 动画：敌方碰撞前摇 + 玩家进入准备姿态（Idle 切换）
	UAnimMontage* TelegraphMontage = nullptr;
	if (const FCombatAnimRow* Row = GetCombatAnimRow(false))
	{
		const FAnimRef& Telegraph = (ClashType == EClashType::BlueClash)
			? Row->ClashTelegraphBlue
			: Row->ClashTelegraphWhite;
		TelegraphMontage = Telegraph.Montage.IsNull() ? nullptr : Telegraph.Montage.LoadSynchronous();
		PlayCombatAnim(BossEnemy.Get(), Telegraph);
	}
	SetPlayerClashReady(true);

	// 命中时间锚点：优先取前摇 Montage 的 ClashTelegraphHit 通知时间，缺失回落 ClashTelegraphTime
	const float NotifyHitTime = GetNotifyTime(TelegraphMontage, FName(TEXT("ClashTelegraphHit")));
	ClashHitTime = NotifyHitTime > 0.0f ? NotifyHitTime : ClashTelegraphTime;
	LastClashInputTime = -1.0f;

	// 注册敌方前摇待命中事件（金额由 ResolveClash 决定；通知/回落二选一消费）
	RegisterPendingHit(false, FName(TEXT("ClashTelegraphHit")), PlayerRole.Get(), 0.0f, BossEnemy.Get(),
		FAnimRef(), nullptr, FAnimRef(), FAnimRef(), TelegraphMontage, false);

	const float BlockWindow = GetBlockWindow();
	const float DodgeWindow = GetDodgeWindow();
	const float OpenDelay = FMath::Max(0.0f, ClashHitTime - FMath::Max(BlockWindow, DodgeWindow));

	GetWorld()->GetTimerManager().SetTimer(ClashOpenTimer, this, &UBattleComponent::OpenClashWindow, OpenDelay, false);
	GetWorld()->GetTimerManager().SetTimer(ClashImpactTimer, this, &UBattleComponent::OnClashImpact, ClashHitTime, false);
```

> 原 `const float BlockWindow = ...` 与 `ClashTelegraphTime` 计时行被本块取代；`OnBattleStateChanged.Broadcast()` 保留在函数末尾。

- [ ] **Step 3: ResolveClash 改为"决定结果与金额，不立即扣血"**

将 `ResolveClash` 中伤害应用段：

```cpp
	if (Incoming > 0.0f)
	{
		ApplyDamageTo(PlayerRole.Get(), Incoming, BossEnemy.Get());
	}
```

替换为：

```cpp
	// 伤害延迟到 ClashTelegraphHit 通知/回落触发；金额写回待命中事件
	if (EnemyPendingHit.bActive && EnemyPendingHit.EventName == FName(TEXT("ClashTelegraphHit")))
	{
		EnemyPendingHit.Amount = Incoming;
	}
	else if (Incoming > 0.0f)
	{
		// 通知已消费（或槽已被覆盖）：直接结算，保证伤害不丢
		ApplyDamageTo(PlayerRole.Get(), Incoming, BossEnemy.Get());
	}
```

在 `case EClashResult::BlockSuccess:` 中，`PlayBlockSuccessChain();` 前插入被格挡反馈与金色反击待命中注册：

```cpp
		// 被格挡反馈：敌方立即混入 BlockedReaction + 停帧
		if (const FCombatAnimRow* EnemyRow = GetCombatAnimRow(false))
		{
			PlayCombatAnim(BossEnemy.Get(), EnemyRow->BlockedReaction);
		}
		const FCombatParamsRow Defaults;
		const FCombatParamsRow* Params = GetCombatSubsystem() ? GetCombatSubsystem()->GetCombatParams() : nullptr;
		const FCombatParamsRow& P = Params ? *Params : Defaults;
		StartHitStop(P.HitStopDuration);
		if (const FCombatAnimRow* Row = GetCombatAnimRow(true))
		{
			RegisterPendingHit(true, FName(TEXT("GoldCounterHit")), BossEnemy.Get(), GetPlayerGoldDamage(),
				PlayerRole.Get(), Row->Hurt, nullptr, FAnimRef(), FAnimRef(),
				Row->GoldCounter.Montage.IsNull() ? nullptr : Row->GoldCounter.Montage.LoadSynchronous(), false);
		}
```

- [ ] **Step 3b: DodgeSuccess 增加停帧**

在 `case EClashResult::DodgeSuccess:` 中，闪避 Buff 施加之后、`DodgeSuccess` 动画播放之前插入：

```cpp
		const FCombatParamsRow Defaults;
		const FCombatParamsRow* Params = GetCombatSubsystem() ? GetCombatSubsystem()->GetCombatParams() : nullptr;
		const FCombatParamsRow& P = Params ? *Params : Defaults;
		StartHitStop(P.HitStopDuration);
```

- [ ] **Step 4: OnClashImpact 增加回落消费**

在 `OnClashImpact` 的 `ResolveClash(Result);` 后追加：

```cpp
	// 前摇无 ClashTelegraphHit 通知：由影响计时器回落结算（若通知已消费则跳过）
	if (EnemyPendingHit.bActive && EnemyPendingHit.EventName == FName(TEXT("ClashTelegraphHit")))
	{
		ApplyPendingHitNow(false);
	}
```

- [ ] **Step 5: 输入增加冷却与每键独立窗口**

将 `OnBlockPressed` 替换为：

```cpp
void UBattleComponent::OnBlockPressed()
{
	if (Phase != EBattlePhase::Clash || bClashResolved)
	{
		return;
	}
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float Cooldown = GetClashInputCooldown();
	if (LastClashInputTime >= 0.0f && Now - LastClashInputTime < Cooldown)
	{
		UE_LOG(LogTemp, Log, TEXT("UBattleComponent::OnBlockPressed - 输入冷却中，忽略"));
		return;
	}
	LastClashInputTime = Now;

	if (Now >= ClashHitTime - GetBlockWindow())
	{
		ResolveClash(EClashResult::BlockSuccess);
		return;
	}
	// 窗口外按下：先记录失败结果，窗口内再按可覆盖
	PendingClashResult = EClashResult::BlockFail;
}
```

将 `OnDodgePressed` 替换为（同构，窗口用 `GetDodgeWindow()`，结果 `DodgeSuccess`/`DodgeFail`）：

```cpp
void UBattleComponent::OnDodgePressed()
{
	if (Phase != EBattlePhase::Clash || bClashResolved)
	{
		return;
	}
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float Cooldown = GetClashInputCooldown();
	if (LastClashInputTime >= 0.0f && Now - LastClashInputTime < Cooldown)
	{
		UE_LOG(LogTemp, Log, TEXT("UBattleComponent::OnDodgePressed - 输入冷却中，忽略"));
		return;
	}
	LastClashInputTime = Now;

	if (Now >= ClashHitTime - GetDodgeWindow())
	{
		ResolveClash(EClashResult::DodgeSuccess);
		return;
	}
	PendingClashResult = EClashResult::DodgeFail;
}
```

- [ ] **Step 6: 增加 GetClashInputCooldown 辅助**

在"数值辅助"区（`GetDodgeWindow` 后）加声明与实现：

```cpp
	/** 读取格挡/闪避输入冷却（秒） */
	float GetClashInputCooldown() const;
```

```cpp
float UBattleComponent::GetClashInputCooldown() const
{
	const FCombatParamsRow Defaults;
	const FCombatParamsRow* Params = GetCombatSubsystem() ? GetCombatSubsystem()->GetCombatParams() : nullptr;
	const FCombatParamsRow& P = Params ? *Params : Defaults;
	return P.ClashInputCooldown;
}
```

- [ ] **Step 7: 编译**

Run: 构建命令
Expected: 0 error / 0 new warning。

- [ ] **Step 8: 提交**

```bash
git add Hole/Source/Hole/Public/Combat/BattleComponent.h Hole/Source/Hole/Private/Combat/BattleComponent.cpp
git commit -m "feat(combat-dmg): anchor clash windows to telegraph hit notify with input cooldown"
```

---

### Task 6: 结算清理与收尾接线

**Files:**
- Modify: `Hole/Source/Hole/Private/Combat/BattleComponent.cpp`

**Interfaces:**
- Consumes: Task 3-5。
- Produces: 战斗结束/重试时清理待命中事件、防御定时器与停帧定时器；`FinishBattle` 的死亡/胜利动画时机不变（由命中时刻的 `ApplyDamageTo` 触发）。

- [ ] **Step 1: SheathePlayerWeapon 增加待命中事件清理**

在 `SheathePlayerWeapon` 的 `ClearPendingReactions();` 后加：

```cpp
		ClearPendingHits();
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(HitStopTimer);
		}
```

- [ ] **Step 2: ResetForRetry 增加待命中事件清理**

在 `ResetForRetry` 中 `PendingClashResult = EClashResult::None;` 后加：

```cpp
	ClearPendingHits();
	LastClashInputTime = -1.0f;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(HitStopTimer);
	}
```

- [ ] **Step 3: 编译**

Run: 构建命令
Expected: 0 error / 0 new warning。

- [ ] **Step 4: 提交**

```bash
git add Hole/Source/Hole/Private/Combat/BattleComponent.cpp
git commit -m "fix(combat-dmg): clear pending hit events and defender timers on battle cleanup"
```

---

### Task 7: 文档同步

**Files:**
- Modify: `DataTable_Spec.md`、`GDD_Outline.md`、`AGENTS.md`、`DevLog.md`

- [ ] **Step 1: DataTable_Spec.md**：§3.2 列定义与 §3.3 行数据新增 `ClashInputCooldown`（0.15）、`RedDefenseLeadTime`（0.3）、`HitStopDuration`（0.12）；§15 列定义与行数据新增 `BlockedReaction`；版本升 v0.10 → v0.11，修订记录加一行；`关联策划案` 同步 GDD 版本。
- [ ] **Step 2: GDD_Outline.md**：§5.2.5 补充"命中通知驱动伤害、格挡/闪避窗口 = [命中通知时间 − 窗口, 命中通知时间]、输入冷却 `ClashInputCooldown`、格挡/闪避/红防反击成功停帧、被格挡动画 `BlockedReaction`"；版本升 v0.10 → v0.11，修订记录加一行。
- [ ] **Step 3: AGENTS.md**：战斗动画约定追加"伤害延迟到 `UAnimNotify_CombatDamage` 命中帧；待命中事件槽先到先得；碰撞窗口以 `ClashTelegraphHit` 为锚；输入冷却；停帧 `HitStopDuration`；被格挡动画 `BlockedReaction`"；版本引用同步。
- [ ] **Step 4: DevLog.md**：合并一条 2026-08-06 记录（伤害绑定动画通知、窗口锚点、输入冷却、蓝红预排、停帧与被格挡动画）。
- [ ] **Step 5: 提交**

```bash
git add DataTable_Spec.md GDD_Outline.md AGENTS.md DevLog.md
git commit -m "docs(combat-dmg): sync damage-notify rules, clash windows and new params"
```

---

### Task 8: 编辑器通知摆放与 PIE 验证（用户手动）

**Files（编辑器内，不入 git）：** 玩家/敌人攻击与前摇 Montage、红防 Montage。

- [ ] **Step 1: 重启编辑器** 加载新 DLL。
- [ ] **Step 2: 挂通知**：
  - 玩家/敌人 `WhiteAttack`/`BlueAttack`/`GoldCounter` Montage：挥击帧挂 `AnimNotify_CombatDamage`，`EventName` 分别为 `WhiteAttackHit`/`BlueAttackHit`/`GoldCounterHit`。
  - 敌方 `ClashTelegraphBlue`/`ClashTelegraphWhite`：攻击判定帧挂 `AnimNotify_CombatDamage`，`EventName = ClashTelegraphHit`。
  - 红防 Montage：举剑防御帧挂 `AnimNotify_CombatMarker`，`MarkerName = GuardReady`。
  - `DT_CombatAnimConfig` 的 `drifter`/`satan` 行补充 `BlockedReaction` 列（被格挡动画资产）。
- [ ] **Step 3: PIE 验证清单**

| 验证点 | 操作 | 期望 |
|---|---|---|
| 命中帧扣血 | 白攻打中 | 白攻 `WhiteAttackHit` 帧才扣血，受击动画同帧 |
| 蓝红预排 | 红防 vs 蓝攻 | 红防提前启动，`GuardReady` 帧与 `BlueAttackHit` 帧对齐（日志时间戳） |
| 金色反击 | 红防克蓝攻 | 红防结束接金色反击，`GoldCounterHit` 帧敌方扣血+受击 |
| 2 层正面承受 | 敌方 2 层蓝攻 vs 红防 | `BlueAttackHit` 帧扣血，红防结束接受击 |
| 蓄力抵抗 | 白攻 vs 蓄力 | 微量伤害在命中帧扣，抵抗方保持蓄力姿态 |
| 蓄力打断 | 蓝攻 vs 蓄力 | `BlueAttackHit` 帧扣血并播 `ChargeInterrupted` |
| 碰撞窗口 | 蓝/白碰撞 | 敌方前摇命中前按各自窗口（格挡 0.25s / 闪避 0.35s）判定成功；更早按不算 |
| 输入冷却 | 碰撞中连按 | 冷却内第二次输入被忽略（日志） |
| 停帧 | 格挡/闪避成功或红防反击成功 | 命中瞬间双方动作暂停 `HitStopDuration` 后恢复 |
| 被格挡动画 | 敌方蓝攻被红防/格挡 | 攻击者立即从当前攻击动画混入 `BlockedReaction` |
| 回落 | 未挂通知的占位动画 | 播完/影响计时器结算 + 警告日志 |
| 死亡时机 | 致命一击 | 死亡/结算在命中帧触发 |

- [ ] **Step 4: 验证结果回填**：PIE 问题记录到 DevLog 或作为新 bug 反馈。

---

## 计划自检记录

- 覆盖：设计文档全部条目（通知类、待命中事件槽、普通回合注册、蓝红预排、碰撞窗口、输入冷却、停帧与被格挡动画、回落、参数、脚本、文档、编辑器/PIE）均有对应任务。
- 无占位符：代码步骤给出完整代码或精确插入点；`RegisterBlueVsRedHit` 在 Task 3 以空实现占位并在 Task 4 补齐（已显式说明）。
- 类型一致性：`RegisterPendingHit`（末参 `bDefenderBlocked`） / `ApplyPendingHitNow` / `OnHitNotify` / `GetNotifyTime` / `ScheduleDefenderReaction` / `StartHitStop` 签名在任务间一致；`EventName` 常量（`BlueAttackHit`/`WhiteAttackHit`/`GoldCounterHit`/`ClashTelegraphHit`/`GuardReady`）全文一致。
- 风险：回合推进与动画解耦，命中通知若被下一动作打断由回落保证伤害不丢；PIE 验证后如需"等动画播完再推进"另行迭代。
