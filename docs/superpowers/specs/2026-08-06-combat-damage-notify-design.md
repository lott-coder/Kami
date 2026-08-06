# 战斗伤害动画通知绑定设计（Combat Damage Notify）— Design Spec

> **Status:** 方向已确认（2026-08-06），待用户复审
> **Project:** Hole（洞穴）
> **Related:** GDD_Outline.md §5.2.5/§5.2.7、DataTable_Spec.md §3/§15、docs/superpowers/specs/2026-08-05-combat-anim-design.md

## Overview

把战斗中的**伤害结算与防御反应**绑定到动画命中帧：攻击 Montage 在挥击帧挂 `UAnimNotify_CombatDamage`（事件名 `EventName`）；回合结算只"注册待命中事件"，通知触发时才 `ApplyDamageTo`、播放目标受击/防御反应并处理死亡。格挡/闪避的判定窗口以敌方命中通知为锚点：碰撞后敌方攻击开始即可按下（带输入冷却防连按），成功判定只落在命中前的各自窗口内。格挡/闪避/红防反击成功触发停帧反馈，被格挡的攻击者立即混入被格挡动画。

## 已确认决策

1. **伤害触发采用方案 A（AnimNotify 命中通知）**——回合制游戏的主流做法，命中帧由动画资产定义。
2. **未挂通知回落**：动作 Montage 播完时结算并打警告日志；碰撞前摇沿用 `ClashTelegraphTime`（0.8s）计时器。
3. **通知命名**：`EventName` 用动作语义（`WhiteAttackHit` / `BlueAttackHit` / `GoldCounterHit` / `ClashTelegraphHit`），玩家/敌人可复用；注册表按"攻击者 + EventName"区分。
4. **格挡/闪避**：输入在敌方攻击开始后即可按下；各自窗口 = `[命中通知时间 - 窗口时长, 命中通知时间]`；输入冷却 `ClashInputCooldown` 防连按。
5. **蓝 vs 红命中反应（2026-08-06 晚间修订）**：红防动画**提前**于蓝攻命中帧启动，使红防动画的"举剑防御"（`GuardReady` 标记帧）与 `BlueAttackHit` 命中通知帧对齐；红防播完接金色反击（或 2 层正面承受时接受击）。
6. **命中反馈（2026-08-06 追加）**：格挡成功（含红防反击成功）与闪避成功触发停帧（`HitStopDuration`，默认 0.12s，参数化）；被格挡的攻击者（含红防反击成功）立即从当前攻击动画混入 `BlockedReaction`；闪避成功不触发被格挡动画。

> **决策变更记录：** 本时序经两轮修订：①"红防与蓝攻同播"（早间）→ ②"蓝攻命中通知触发时才播红防"（午后）→ ③"红防提前启动，举剑防御帧与命中帧对齐"（晚间，当前）。版本②的"通知触发才播"会让防御动作看起来慢半拍，版本③由防御动画自身的举剑帧决定提前量。

## 架构与数据流

### 三层职责

- **USTRUCT 纯数据**：`FCombatParamsRow` 新增 `ClashInputCooldown`（默认 0.15s）与 `RedDefenseLeadTime`（默认 0.3s，红防举剑标记缺失时的回落提前量）；`FAnimRef` 不新增命中时间列（命中帧在资产里，不双份维护）。
- **Subsystem**：伤害公式仍在 `UCombatFormulaSubsystem`；窗口时长继续读属性最终值（`GetBlockWindow`/`GetDodgeWindow`，支持装备/技能修正）。
- **UBattleComponent（运行时）**：持有两侧待命中伤害槽、输入冷却状态、窗口计时与命中回调入口。

### 待命中事件槽（每侧一个，v1 最多双方各一条）

```cpp
struct FPendingHitEvent
{
	FName EventName;                 // 匹配 AnimNotify_CombatDamage.EventName

	// 伤害部分（Amount <= 0 表示本事件不扣血）
	ABaseCharacter* Target;          // 承受伤害方
	float Amount;
	AActor* Causer;
	FAnimRef HitReaction;            // 命中时 Target 播放（Hurt / Charge / ChargeInterrupted）

	// 防御反应部分（蓝 vs 红）
	ABaseCharacter* Defender;        // 命中时做出防御反应的角色（可为空）
	FAnimRef DefenderReaction;       // 如 RedDefense
	FAnimRef DefenderFollowUp;       // 红防播完接续：GoldCounter（反击）或 Hurt（2 层正面承受）

	UAnimMontage* FallbackMontage;   // 未挂通知时在该 Montage 播完结算
	bool bDefenderBlocked = false;   // 本攻击被格挡（含红防反击成功）：命中时攻击者播 BlockedReaction + 停帧
};

UPROPERTY(Transient)
FPendingHitEvent PlayerPendingHit;  // 攻击者=玩家（命中事件归属玩家蓝攻/白攻/金色反击）
UPROPERTY(Transient)
FPendingHitEvent EnemyPendingHit;   // 攻击者=敌人（命中事件归属敌方蓝攻/白攻/碰撞前摇）
```

### 流程

```
结算（ResolveNormalTurn/ResolveExtraTurn/ResolveClash）
  → 算出伤害值（公式不变）
  → 注册待命中事件（不扣血；含可选防御反应）
  → 播放攻击 Montage（含命中通知）；若含防御反应，按提前量预排防御动画
          │
          ├─ AnimNotify_CombatDamage 触发（命中帧）
          │    → OnHitNotify(攻击者, EventName)
          │    → 查对应侧槽：
          │        · 有伤害 → ApplyDamageTo → 播放 HitReaction → 死亡/结算
          │        · bDefenderBlocked → 攻击者播 BlockedReaction + 停帧
          │        · 有 Defender → DefenderReaction 已按提前量播放中 → 播完接 DefenderFollowUp
          │
          └─ 回落：Montage 播完仍未被通知消费 → 播完结算 + 警告日志
```

### 各动作的伤害通知归属

| 动作 | 携带通知的 Montage | EventName | 命中目标 |
|---|---|---|---|
| 玩家/敌人 白攻 | `WhiteAttack` | `WhiteAttackHit` | 对方 |
| 玩家/敌人 蓝攻 | `BlueAttack` | `BlueAttackHit` | 对方 |
| 玩家/敌人 金色反击 | `GoldCounter` | `GoldCounterHit` | 对方 |
| 敌人碰撞前摇（蓝/白） | `ClashTelegraphBlue/White` | `ClashTelegraphHit` | 玩家（按格挡/闪避结果） |
| 格挡成功弹反 | `BlockSuccess` | 无（纯动作，伤害走 GoldCounter） | — |
| 红防（蓝 vs 红防御反应） | `RedDefense` | `GuardReady`（标记帧，不造成伤害） | — |

> 蓄力抵抗白攻：注册伤害时 `HitReaction = Charge`（保持蓄力姿态，不播受击）；蓝攻打断蓄力：`HitReaction = ChargeInterrupted`。
> **蓝 vs 红：** `BlueAttackHit` 命中事件同时携带防御反应——Defender 的 `RedDefense` 按提前量预排启动（`GuardReady` 帧与 `BlueAttackHit` 帧对齐），播完接 `GoldCounter`（红防克蓝攻）；若为 2 层强化蓝攻正面承受，Defender 播 `RedDefense` 后接 `Hurt`，且 `BlueAttackHit` 同一通知按蓝攻伤害扣血。
> **被格挡动画：** `FCombatAnimRow` 新增 `BlockedReaction` 列；在格挡成功/红防反击成功的命中时刻，由攻击者立即混入（打断当前攻击动画），无独立通知。

### 蓝 vs 红命中反应时序

```
蓝攻开始（玩家或敌方）
  ├─ 预排：Defender 的 RedDefense 在 (BlueAttackHitTime - GuardReadyTime) 启动
  ├─ 时间轴：
  │     [RedDefense 启动] ──→ [GuardReady 标记帧 = BlueAttackHit 帧] ──→ [RedDefense 播完]
  │                                  │
  │                                  └─ 2 层正面承受：同一帧按蓝攻伤害扣血
  ├─ RedDefense 播完 → GoldCounter（红防克蓝攻）或 Hurt（2 层正面承受）
  └─ GoldCounterHit 通知（金色反击命中帧）→ 对方扣血 + 受击

> **提前量计算：** `RedDefenseStartDelay = BlueAttackHitTime - GuardReadyTime`（取非负值，通过 Timer 启动，<=0 立即播放）。`BlueAttackHitTime` 扫描蓝攻 Montage 的 `BlueAttackHit` 通知；`GuardReadyTime` 扫描红防 Montage 的 `GuardReady` 标记。红防无标记时回落 `RedDefenseLeadTime`（默认 0.3s，入 `FCombatParamsRow`）；蓝攻无命中通知时回落"红防立即启动"并打警告。

### 命中反馈（停帧与被格挡动画）

- **停帧**：`StartHitStop(Duration)` 暂停玩家与敌人 AnimInstance 的活动 Montage，`Duration` 后恢复；`Duration <= 0` 跳过。参数 `HitStopDuration`（默认 0.12s，入 `FCombatParamsRow`）。
- **触发点**：
  - 碰撞格挡成功：敌方播 `BlockedReaction` + 玩家播 `BlockSuccess`（弹反）→ 停帧 → 恢复后按现有链接 `GoldCounter`。
  - 红防反击成功（蓝 vs 红）：`BlueAttackHit` 命中帧 → 攻击者播 `BlockedReaction` + 停帧 → 红防链继续 → `GoldCounterHit` 扣血+受击。
  - 闪避成功：停帧 + `DodgeSuccess`，不播被格挡动画。
- **混入方式**：对攻击者 `PlayAnimMontage(BlockedReaction)`，引擎自动把当前攻击 Montage 混合过渡到被格挡反应（无需额外代码）。
```

### 格挡/闪避窗口（以命中通知为锚）

```
敌方攻击开始（StartClash，播 ClashTelegraph*）
  ├─ 输入允许按下（E/Shift），带 ClashInputCooldown 防连按
  ├─ 判定窗口（各自独立）：
  │    格挡：[HitTime - BlockWindowSeconds, HitTime]
  │    闪避：[HitTime - DodgeWindowSeconds, HitTime]
  │    窗口内按下 → ResolveClash(成功)
  │    窗口外提前按下 → 记录该键失败（可被窗口内另一键覆盖，沿用现有 PendingClashResult）
  └─ HitTime = 敌方前摇 Montage 中 ClashTelegraphHit 通知的时间
           （未挂通知 → HitTime = ClashTelegraphTime）
```

- `GetHitNotifyTime(UAnimMontage*, FName)`：扫描 Montage 的 Notifies，返回匹配通知时间（秒）；找不到返回 `-1`。
- 伤害触发：`ClashTelegraphHit` 通知触发时按结果结算——格挡/闪避成功 → 0 伤害（闪避成功加 Buff，格挡成功注册金色反击伤害给 GoldCounterHit）；失败 → 全额或 ×1.2。
- 回落：通知缺失时沿用现有 `ClashImpactTimer`（HitTime 后按未按键=格挡失败处理）。
- 输入冷却：`OnBlockPressed/OnDodgePressed` 先检查 `ClashInputCooldown`（距上次输入不足则忽略），避免连按；`ResolveClash` 后 `bClashResolved` 继续拦截后续输入。

## 通知类

新增 `UAnimNotify_CombatDamage`（归档 `Animation/AnimNotifies/Combat/`）：

```cpp
UCLASS()
class HOLE_API UAnimNotify_CombatDamage : public UAnimNotify
{
	GENERATED_BODY()

	/** 与注册槽匹配的事件名，如 WhiteAttackHit / GoldCounterHit / ClashTelegraphHit */
	UPROPERTY(EditAnywhere, Category = "Combat")
	FName EventName;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
```

`Notify` 实现：取 `MeshComp->GetOwner()` 为攻击者 → 找到玩家的 `UBattleComponent`（按 Tag `Player` + `FindComponentByClass`）→ 调用 `OnHitNotify(攻击者, EventName)`。

### 标记通知 `UAnimNotify_CombatMarker`（新增）

用途：标注动画内**无伤害**的关键帧，供预排计算扫描，不触发战斗逻辑（`Notify` 为空实现或仅日志）。

```cpp
UCLASS()
class HOLE_API UAnimNotify_CombatMarker : public UAnimNotify
{
	GENERATED_BODY()

	/** 标记名，如 GuardReady（红防举剑防御帧） */
	UPROPERTY(EditAnywhere, Category = "Combat")
	FName MarkerName;
};
```

v1 仅用于红防 Montage 的 `GuardReady` 标记；后续可扩展 VFX/音效等时间锚点。

## 文件改动

| 文件 | 动作 |
|---|---|
| `Hole/Source/Hole/Public/Animation/AnimNotifies/Combat/AnimNotify_CombatDamage.h` + `Private/.../AnimNotify_CombatDamage.cpp` | 新建：命中通知 |
| `Hole/Source/Hole/Public/Animation/AnimNotifies/Combat/AnimNotify_CombatMarker.h` + `Private/.../AnimNotify_CombatMarker.cpp` | 新建：无伤害标记（GuardReady） |
| `Hole/Source/Hole/Public/Combat/BattleComponent.h` + `Private/Combat/BattleComponent.cpp` | 修改：待命中伤害槽、OnHitNotify、窗口重做（HitTime 锚定 + 每键独立窗口）、输入冷却、Montage 播完回落、停帧与被格挡动画 |
| `Hole/Source/Hole/Public/DataTable/CombatParamsTable.h` | 修改：新增 `ClashInputCooldown`、`RedDefenseLeadTime`、`HitStopDuration` |
| `Hole/Source/Hole/Public/DataTable/CombatAnimConfigTable.h` | 修改：`FCombatAnimRow` 新增 `BlockedReaction` 列 |
| `Scripts/create_datatables.py` | 修改：参数表新增列与默认值（不运行重建） |
| `DataTable_Spec.md` / `GDD_Outline.md` / `AGENTS.md` / `DevLog.md` | 修改：参数、规则、约定同步 |
| 编辑器资产 | 用户手动：给白攻/蓝攻/金色反击/敌方前摇 Montage 挂 `AnimNotify_CombatDamage` 并填 EventName |

## PIE 验证清单

| 验证点 | 操作 | 期望 |
|---|---|---|
| 命中帧扣血 | 白攻打中（无克制） | 伤害在白攻命中通知帧才扣，HP 与受击动画同步 |
| 受击同步 | 任意命中 | 目标受击动画与扣血同时出现（日志时间戳一致） |
| 金色反击 | 红防 vs 蓝攻 | 红防提前启动，`GuardReady` 举剑帧与 `BlueAttackHit` 帧对齐（日志时间戳验证）→ 红防结束接金色反击 → `GoldCounterHit` 帧敌方扣血+受击 |
| 格挡窗口 | 蓝/白碰撞 | 敌方前摇命中前 0.25s 内按 E 成功；更早按 E 不算成功 |
| 闪避窗口 | 蓝/白碰撞 | 命中前 0.35s 内按 Shift 成功；窗口外提前按不算 |
| 输入冷却 | 碰撞中快速连按 | 冷却时间内第二次输入被忽略（日志） |
| 停帧 | 格挡/闪避成功或红防反击成功 | 命中瞬间双方动作暂停 `HitStopDuration` 后恢复 |
| 被格挡动画 | 敌方蓝攻被红防/格挡 | 攻击者立即从当前攻击动画混入 `BlockedReaction` |
| 蓄力抵抗 | 白攻 vs 蓄力 | 微量伤害在命中帧扣，抵抗方保持蓄力姿态、不播受击 |
| 回落 | 未挂通知的占位动画 | 播完结算 + 警告日志 |
| 死亡时机 | 致命一击 | 死亡/结算在命中帧触发 |

## Out of Scope

- 多段命中（一个动画多个通知）——槽结构预留，后续扩展
- 伤害数字 UI、命中特效、音效、受击闪白
- 敌人 AI 实时躲避（v1 仅玩家单边格挡/闪避）
- 连招/取消动作系统
