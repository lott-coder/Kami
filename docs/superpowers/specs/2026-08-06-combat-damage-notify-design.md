# 战斗伤害动画通知绑定设计（Combat Damage Notify）— Design Spec

> **Status:** 方向已确认（2026-08-06），待用户复审
> **Project:** Hole（洞穴）
> **Related:** GDD_Outline.md §5.2.5/§5.2.7、DataTable_Spec.md §3/§15、docs/superpowers/specs/2026-08-05-combat-anim-design.md

## Overview

把战斗中的**伤害结算与防御反应**绑定到动画命中帧：攻击 Montage 在挥击帧挂 `UAnimNotify_CombatDamage`（事件名 `EventName`）；回合结算只"注册待命中事件"，通知触发时才 `ApplyDamageTo`、播放目标受击/防御反应并处理死亡。格挡/闪避的判定窗口以敌方命中通知为锚点：碰撞后敌方攻击开始即可按下（带输入冷却防连按），成功判定只落在命中前的各自窗口内。

## 已确认决策

1. **伤害触发采用方案 A（AnimNotify 命中通知）**——回合制游戏的主流做法，命中帧由动画资产定义。
2. **未挂通知回落**：动作 Montage 播完时结算并打警告日志；碰撞前摇沿用 `ClashTelegraphTime`（0.8s）计时器。
3. **通知命名**：`EventName` 用动作语义（`WhiteAttackHit` / `BlueAttackHit` / `GoldCounterHit` / `ClashTelegraphHit`），玩家/敌人可复用；注册表按"攻击者 + EventName"区分。
4. **格挡/闪避**：输入在敌方攻击开始后即可按下；各自窗口 = `[命中通知时间 - 窗口时长, 命中通知时间]`；输入冷却 `ClashInputCooldown` 防连按。
5. **蓝 vs 红命中反应（2026-08-06 修订）**：红防动画不再与蓝攻同播，改为蓝攻 `BlueAttackHit` 通知触发时才播放；红防播完接金色反击（或 2 层正面承受时接受击）。

> **决策变更记录：** 上一版"红防与蓝攻动画同时播放"（2026-08-06 早间定案）被本版取代——防御反应跟随攻击命中帧，观感更"即时反应"。

## 架构与数据流

### 三层职责

- **USTRUCT 纯数据**：`FCombatParamsRow` 新增 `ClashInputCooldown`（默认 0.15s）；`FAnimRef` 不新增命中时间列（命中帧在资产里，不双份维护）。
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
  → 播放攻击 Montage（含命中通知）
          │
          ├─ AnimNotify_CombatDamage 触发（命中帧）
          │    → OnHitNotify(攻击者, EventName)
          │    → 查对应侧槽：
          │        · 有伤害 → ApplyDamageTo → 播放 HitReaction → 死亡/结算
          │        · 有 Defender → 播放 DefenderReaction → 播完接 DefenderFollowUp
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

> 蓄力抵抗白攻：注册伤害时 `HitReaction = Charge`（保持蓄力姿态，不播受击）；蓝攻打断蓄力：`HitReaction = ChargeInterrupted`。
> **蓝 vs 红：** `BlueAttackHit` 命中事件同时携带防御反应——Defender 播 `RedDefense`，播完接 `GoldCounter`（红防克蓝攻）；若为 2 层强化蓝攻正面承受，Defender 播 `RedDefense` 后接 `Hurt`，且同一通知按蓝攻伤害扣血。

### 蓝 vs 红命中反应时序

```
蓝攻开始（玩家或敌方）
  → BlueAttackHit 通知（蓝攻命中帧）
      ├─ Defender 播 RedDefense（不再与蓝攻同播，改为通知触发）
      ├─ RedDefense 播完 → GoldCounter（红防克蓝攻）或 Hurt（2 层正面承受）
      └─ 蓝攻自身有伤害（2 层正面承受）→ 同一通知扣血
  → GoldCounterHit 通知（金色反击命中帧）→ 对方扣血 + 受击
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

## 文件改动

| 文件 | 动作 |
|---|---|
| `Hole/Source/Hole/Public/Animation/AnimNotifies/Combat/AnimNotify_CombatDamage.h` + `Private/.../AnimNotify_CombatDamage.cpp` | 新建：命中通知 |
| `Hole/Source/Hole/Public/Combat/BattleComponent.h` + `Private/Combat/BattleComponent.cpp` | 修改：待命中伤害槽、OnHitNotify、窗口重做（HitTime 锚定 + 每键独立窗口）、输入冷却、Montage 播完回落 |
| `Hole/Source/Hole/Public/DataTable/CombatParamsTable.h` | 修改：新增 `ClashInputCooldown` |
| `Scripts/create_datatables.py` | 修改：参数表新增列与默认值（不运行重建） |
| `DataTable_Spec.md` / `GDD_Outline.md` / `AGENTS.md` / `DevLog.md` | 修改：参数、规则、约定同步 |
| 编辑器资产 | 用户手动：给白攻/蓝攻/金色反击/敌方前摇 Montage 挂 `AnimNotify_CombatDamage` 并填 EventName |

## PIE 验证清单

| 验证点 | 操作 | 期望 |
|---|---|---|
| 命中帧扣血 | 白攻打中（无克制） | 伤害在白攻命中通知帧才扣，HP 与受击动画同步 |
| 受击同步 | 任意命中 | 目标受击动画与扣血同时出现（日志时间戳一致） |
| 金色反击 | 红防 vs 蓝攻 | 蓝攻命中通知帧才播红防 → 红防结束接金色反击 → 金色反击命中帧敌方扣血+受击 |
| 格挡窗口 | 蓝/白碰撞 | 敌方前摇命中前 0.25s 内按 E 成功；更早按 E 不算成功 |
| 闪避窗口 | 蓝/白碰撞 | 命中前 0.35s 内按 Shift 成功；窗口外提前按不算 |
| 输入冷却 | 碰撞中快速连按 | 冷却时间内第二次输入被忽略（日志） |
| 蓄力抵抗 | 白攻 vs 蓄力 | 微量伤害在命中帧扣，抵抗方保持蓄力姿态、不播受击 |
| 回落 | 未挂通知的占位动画 | 播完结算 + 警告日志 |
| 死亡时机 | 致命一击 | 死亡/结算在命中帧触发 |

## Out of Scope

- 多段命中（一个动画多个通知）——槽结构预留，后续扩展
- 伤害数字 UI、命中特效、音效、受击闪白
- 敌人 AI 实时躲避（v1 仅玩家单边格挡/闪避）
- 连招/取消动作系统
