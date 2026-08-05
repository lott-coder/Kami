# 战斗动画表设计（Combat Anim Design）— Design Spec

> **Status:** 已确认（2026-08-05）
> **Project:** Hole（洞穴）
> **Related:** GDD_Outline.md §5.2.3/§5.2.5、DataTable_Spec.md 第 13 号表 `DT_CombatAnimConfig`、Plans/combat-system.md

## Overview

为战斗系统 v1（Dale vs Satan）补齐动画表现：覆盖红防/蓝攻/白攻/蓄力四类行动、同色碰撞实时格挡/闪避、金色反击、受击/死亡/胜利等事件，以及玩家碰撞准备姿态与敌方碰撞前摇。数据以新增 13 号表 `DT_CombatAnimConfig` 承载，一行一个实体，动作列统一使用 `FAnimRef`（Montage 软引用 + Section + PlayRate + BlendOutTime）。

## 已确认决策（2026-08-05 用户拍板）

1. **收刀动画先留空**：战斗结束仍直接挂回背部 socket，不做收刀 Montage。
2. **敌人动画先占位**：Satan（Sevarog 骨架）战斗动作暂无资产，先用占位/简单姿态跑通流程。
3. **蓝攻不分蓄力层级**：0/1/2 层蓄力共用一条蓝攻动画，后续需要时再加变体。
4. **玩家增加碰撞准备姿态**：同色碰撞进入准备阶段时，玩家 Idle 切换为 `ClashReady` 姿态，结算结束后回退。
5. **敌人增加金色反击**：敌方红防成功（红防 vs 蓝攻）时播放 `GoldCounter` 动画，与玩家侧共用"金色攻击"结算体系。

## 动画清单

### 玩家（Dale / drifter）

| 事件 ID | 中文名 | 触发时机 | 建议形式 | 现状 |
|---|---|---|---|---|
| `entry_draw` | 入场拔刀 | 战斗开始，HUD 出现后 | Montage（`Draw` Section + `AnimNotify_Hold`） | ✅ `MTG_DrawGreatSword`（大剑动作占位） |
| `sheathe` | 收刀 | 战斗结束 | Montage | ⬜ 留空 |
| `red_defense` | 红防姿态 | 选择红防时 | 循环姿态 | ❌ 待资产 |
| `gold_counter` | 金色反击 | 红防克蓝攻 / 格挡成功后衔接 | 一次性挥砍 Montage | ❌ 待资产 |
| `blue_attack` | 蓝攻 | 选择蓝攻（≥1 蓄力） | Montage + 伤害通知 | ❌ 待资产 |
| `white_attack` | 白攻 | 选择白攻 | 快速 Montage | ❌ 待资产 |
| `charge` | 蓄力姿态 | 选择蓄力后持续 | 循环动画 | ❌ 待资产 |
| `charge_interrupted` | 蓄力被打断 | 蓝攻打断蓄力 / 蓝攻被红防克制清层 | Montage | ❌ 可先复用 `hurt` |
| `hurt` | 受击 | 受到伤害（含格挡/闪避失败） | Montage | ❌ 待资产 |
| `block_success` | 格挡成功 | 同色碰撞按 E 成功 | Montage（弹反动作，播完接 `gold_counter`） | ❌ 待资产 |
| `block_fail` | 格挡失败 | 按 E 失败或未按 | 复用 `hurt` | 可复用 |
| `dodge_success` | 闪避成功 | 同色碰撞按 Shift 成功 | Montage（侧闪/翻滚） | ❌ 待资产 |
| `dodge_fail` | 闪避失败 | Shift 失败 | 复用 `hurt` | 可复用 |
| `clash_ready` | 碰撞准备姿态 | 同色碰撞准备阶段 | 循环姿态（状态型） | ❌ 待资产 |
| `death` | 倒地/失败 | HP 归零 | Montage | ❌ 待资产 |
| `victory` | 胜利姿态 | 战斗胜利 | Montage | ⬜ 可空 |
| `skill` | 技能动作 | 技能系统启用后 | Montage | ⬜ 预留留空 |

### 敌人（Satan）

| 事件 ID | 中文名 | 触发时机 | 建议形式 | 现状 |
|---|---|---|---|---|
| `intro_roar` | 开场咆哮 | Boss 剧情动画 | Sequence / 动画 | ✅ `Roar` + `LS_SatanIntro`（不入本表） |
| `idle` | 战斗待机 | 战斗全程 | ABP 状态 | ✅ ABP_Satan 已有 |
| `red_defense` | 红防姿态 | 敌人选红防 | Montage/循环 | ❌ 待资产 |
| `gold_counter` | 金色反击 | 敌方红防成功（红防 vs 蓝攻） | 一次性挥砍 Montage | ❌ 待资产 |
| `blue_attack` | 蓝攻 | 敌人选蓝攻 | Montage + 伤害通知 | ❌ 待资产 |
| `white_attack` | 白攻 | 敌人选白攻 | Montage | ❌ 待资产 |
| `charge` | 蓄力姿态 | 敌人蓄力 | 循环动画 | ❌ 待资产 |
| `clash_telegraph_blue` | 蓝碰撞前摇 | 蓝 vs 蓝同色碰撞 | Montage | ❌ 关键项：替代 HUD 文字提示 |
| `clash_telegraph_white` | 白碰撞前摇 | 白 vs 白同色碰撞 | Montage | ❌ 关键项：对齐 `ClashTelegraphTime`（0.8s） |
| `hurt` | 受击 | 敌人吃伤害 | Montage | ❌ 待资产 |
| `death` | 死亡 | HP 归零 | Montage | ❌ 待资产 |
| `victory` | 胜利姿态 | 玩家失败 | Montage | ⬜ 可空 |

## 同色碰撞时序

```
双方同色（蓝vs蓝 / 白vs白）
  → 敌方播放 ClashTelegraph{Blue|White} 前摇（对齐 ClashTelegraphTime = 0.8s）
  → 玩家进入 ClashReady 准备姿态（战斗 Idle 切换，AnimInstance 暴露 bClashReady）
  → 判定窗口：格挡 E（0.25s）/ 闪避 Shift（0.35s）
      ├─ 格挡成功 → BlockSuccess（弹反）→ GoldCounter（金色反击）
      ├─ 闪避成功 → DodgeSuccess
      └─ 失败/未按 → Hurt（闪避失败伤害 ×1.2）
  → 双方回战斗 Idle（bClashReady 复位）
```

红 vs 红、双蓄力等"无事发生"回合不播动作（或短暂保持红防/蓄力姿态后回 Idle）。

## 数据结构

### 13 号表 `DT_CombatAnimConfig` / `FCombatAnimRow`

- 行 ID：实体 ID（v1：`drifter`、`satan`）
- 列：`DisplayName` + 19 个 `FAnimRef` 动作列（`Entry / Sheathe / RedDefense / GoldCounter / BlueAttack / WhiteAttack / Charge / ChargeInterrupted / Hurt / BlockSuccess / BlockFail / DodgeSuccess / DodgeFail / Death / Victory / Skill / ClashReady / ClashTelegraphBlue / ClashTelegraphWhite`）
- 回落约定（运行时逻辑）：`BlockFail`/`DodgeFail`/`ChargeInterrupted` 空 = 播 `Hurt`；格挡成功先播 `BlockSuccess`（弹反），播完再接 `GoldCounter`（金色反击）

### `FAnimRef` 嵌套结构

```cpp
USTRUCT(BlueprintType)
struct FAnimRef
{
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UAnimMontage> Montage;   // 空 = 不播放或按约定回落

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName SectionName = NAME_None;          // 空 = 从头播放

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PlayRate = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BlendOutTime = 0.25f;
};
```

符合三层架构：USTRUCT 只存"这一格的值"；读表、选动画、回落等运行时逻辑归战斗动画组件/子系统。

## 接入架构建议（实现阶段）

1. **C++**：新增 `FCombatAnimRow` + `FAnimRef`（`Public/DataTable/CombatAnimConfigTable.h`）；`UCombatFormulaSubsystem` 或新战斗动画组件提供 `GetCombatAnimRow(EntityID)`；`UBattleComponent` 在回合结算/碰撞阶段按结果播放对应 Montage。
2. **状态型动画**：`UBaseCharacterAnimInstance` 暴露 `bClashReady`（镜像 `bWeaponDrawn` 的同步方式），ABP_Dale 增加碰撞准备状态切换；`UBattleComponent` 在 `StartClash` 置位、`ResolveClash`/清理时复位。
3. **格挡成功连播**：格挡成功时先播 `BlockSuccess`（弹反动作），Montage 播完回调再播 `GoldCounter`（金色反击），两者各自独立配置在表内。
4. **入口配置迁移**：玩家入场 Montage 从 `UBattleComponent::PlayerEntryMontage` 改为读表 `Entry`（BP 字段保留作回退）。
5. **通知约定**：伤害/命中帧用 Montage 内 AnimNotify 表达；新通知类按类别归档在 `Animation/AnimNotifies/<类别>/`。
6. **脚本同步**：`Scripts/create_datatables.py`、`verify_datatables.py`、`export_datatables.py` 增加 13 号表（等 C++ 结构体落地后再启用，避免重建失败）。
7. **编辑器资产**：`DT_CombatAnimConfig` 建表并填 `drifter`/`satan` 两行；动画资产按上表补齐（先占位跑通流程）。

## 资产现状与获取

- 已有：玩家入场拔刀 Montage（占位）、拔刀 Idle、移动/跳跃动画、Boss 咆哮 + 开场 Sequence、敌人 ABP 模板与 ABP_Satan。
- 待获取/制作：玩家战斗动作（红防/蓝攻/白攻/蓄力/受击/格挡/闪避/死亡/碰撞准备）、敌人对应动作与碰撞前摇。
- 占位策略：先用 Mixamo/现有资产替换最接近的动作（如用 Y_Bot 动作占位），动画表结构不因此改变。

## Out of Scope

- 技能动画（`Skill` 列预留留空）
- 武器类别（单手剑/大剑/锤子）动画分支
- 探索模式翻滚动画（`dodge_success` 可复用时再接入）
- 收刀动画、蓝攻蓄力层级变体
- 战斗系统玩法规则改动
