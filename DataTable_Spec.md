# 数据配置表规范（DataTable Specification）

> **版本：** v0.4
> **日期：** 2026-08-01
> **关联策划案：** GDD_Outline.md v0.3
> **用途：** 定义所有需要暴露给策划的 DataTable 结构，作为 C++ 结构体定义的依据

---

## 目录

1. [概述与设计原则](#1-概述与设计原则)
2. [表索引](#2-表索引)
3. [01 — 全局战斗参数 (DT_CombatParams)](#3-01--全局战斗参数)
4. [02 — 角色配置 (DT_CharacterConfig)](#4-02--角色配置)
5. [03 — 敌人配置 (DT_EnemyConfig)](#5-03--敌人配置)
6. [04 — 武器配置 (DT_WeaponConfig)](#6-04--武器配置)
7. [05 — 面具配置 (DT_MaskConfig)](#7-05--面具配置)
8. [06 — 烟分类配置 (DT_SmokeConfig)](#8-06--烟分类配置)
9. [07 — 技能配置 (DT_SkillConfig)](#9-07--技能配置)
10. [08 — 技能树节点 (DT_SkillTreeConfig)](#10-08--技能树节点)
11. [09 — 区域配置 (DT_AreaConfig)](#11-09--区域配置)
12. [10 — 消耗品配置 (DT_ConsumableConfig)](#12-10--消耗品配置)
13. [11 — 货币/经济配置 (DT_EconomyConfig)](#13-11--货币经济配置)
14. [12 — 数据关联总览](#14-12--数据关联总览)

---

## 1. 概述与设计原则

### 1.1 为什么使用 DataTable

- **策划可直接在 UE 编辑器中修改数值**，无需接触代码
- 支持 **CSV 导入/导出**，策划可用 Excel 批量编辑
- 每个 Row Struct 对应一个 `USTRUCT`，在 C++ 中定义，编辑器内可视化
- 所有行结构继承 `FTableRowBase`

### 1.2 命名约定

| 项目 | 约定 | 示例 |
|------|------|------|
| C++ 结构体 | `FTableNameRow` | `FCombatParamsRow` |
| DataTable 资产 | `DT_DisplayName` | `DT_CombatParams` |
| 资产路径 | `/Game/DataTable/` | `/Game/DataTable/DT_CombatParams` |
| 行 ID | `snake_case` | `white_attack`、`enemy_apprentice` |

### 1.3 GDD 标记说明

| GDD 标记 | 在数据表中的处理 |
|----------|-----------------|
| `[待定]` | 保留，值域填 `[TBD]`，策划后续确定 |
| `[PLAYTEST]` | 填初始猜测值，备注标注"需原型验证" |
| `[待定平衡]` | 填占位值，备注标注"平衡性待调" |

### 1.4 与已有 C++ 代码的关联

当前代码中已存在以下枚举，DataTable 将引用它们：

```cpp
// BaseCharacter.h — 三色属性枚举
enum class EElementalColor : uint8 { None, Red, Blue, White };
```

已实现枚举（2026-08-01 确认，均在各 DataTable 头文件中定义）：

```cpp
enum class EElementalColor     : uint8 { ... };  // BaseCharacter.h — 三色属性
enum class EEnemyTier          : uint8 { ... };  // EnemyConfigTable.h — 敌人等级
enum class EEnemyAIPreference  : uint8 { ... };  // EnemyConfigTable.h — AI 偏好
enum class EWeaponCategory     : uint8 { ... };  // WeaponConfigTable.h — 武器类别
enum class EMaskRarity         : uint8 { ... };  // MaskConfigTable.h — 面具稀有度
enum class ESmokeSource        : uint8 { ... };  // SmokeConfigTable.h — 烟来源
enum class ESkillCategory      : uint8 { ... };  // SkillConfigTable.h — 技能类别
enum class ESkillTarget        : uint8 { ... };  // SkillConfigTable.h — 技能目标
enum class ESkillTreeBranch    : uint8 { ... };  // SkillTreeConfigTable.h — 技能树分支
enum class EConsumableType     : uint8 { ... };  // ConsumableConfigTable.h — 消耗品类型
```

> **不采用的枚举：** 区域以 RowName 标识（不引入 `EAreaID`）；经济参数类别用 `FString Category`（不引入 `ECurrencyType`）。

---

## 2. 表索引

| #   | 资产名 | C++ 结构体 | Class 列 | 行数 | 描述 |
|----|--------|-----------|----------|------|------|
| 01 | `DT_CombatParams` | `FCombatParamsRow` | — | 1（单例） | 战斗全局数值平衡参数 |
| 02 | `DT_CharacterConfig` | `FCharacterConfigRow` | `CharacterClass` | 5 | 可玩角色基础属性 |
| 03 | `DT_EnemyConfig` | `FEnemyConfigRow` | `EnemyClass` | 8 | 敌人类型与 AI 行为 |
| 04 | `DT_WeaponConfig` | `FWeaponConfigRow` | `WeaponClass` | 4（目标8-12） | 武器类型与战斗修正 |
| 05 | `DT_MaskConfig` | `FMaskConfigRow` | — | 5（目标15-20） | 面具类型与稀有度效果 |
| 06 | `DT_SmokeConfig` | `FSmokeConfigRow` | — | 9 | 烟分类与力量印记 |
| 07 | `DT_SkillConfig` | `FSkillConfigRow` | `SkillClass` | 10（目标20-30） | 技能定义与消耗 |
| 08 | `DT_SkillTreeConfig` | `FSkillTreeConfigRow` | — | 15 | 局外技能树节点 |
| 09 | `DT_AreaConfig` | `FAreaConfigRow` | — | 6 | 世界区域属性 |
| 10 | `DT_ConsumableConfig` | `FConsumableConfigRow` | `ConsumableClass` | 10 | 消耗品定义 |
| 11 | `DT_EconomyConfig` | `FEconomyConfigRow` | — | 8 | 货币兑换率与价格基数 |

> **实况说明（2026-08-01）：** 上表"行数"为当前资产实况；目标行数见各章节"基本信息"。

---

## 3. 01 — 全局战斗参数

### 3.1 基本信息

| 项 | 值 |
|----|-----|
| 资产名 | `DT_CombatParams` |
| 结构体 | `FCombatParamsRow` |
| 行 ID 前缀 | 参数的功能类别 |
| 行数 | ~15 |
| 来源 | GDD §5.2.7 战斗参数表 |

### 3.2 列定义

| 列名 | 类型 | 默认值 | 来源/计算公式 | 备注 |
|------|------|--------|--------------|------|
| `WhiteAttackDamageMin` | `float` | 15.0 | GDD §5.2.7 | 白色攻击最小伤害 |
| `WhiteAttackDamageMax` | `float` | 25.0 | GDD §5.2.7 | 白色攻击最大伤害 |
| `BlueAttackDamageMin_0Charge` | `float` | 20.0 | GDD §5.2.7 | 蓝色攻击最小伤害（0蓄力） |
| `BlueAttackDamageMax_0Charge` | `float` | 30.0 | GDD §5.2.7 | 蓝色攻击最大伤害（0蓄力） |
| `ChargeDamageMultiplier_1` | `float` | 1.5 | GDD §5.2.4 | 1层蓄力伤害倍率 |
| `ChargeDamageMultiplier_2` | `float` | 2.25 | GDD §5.2.4 | 2层蓄力伤害倍率（= 1.5²） |
| `MaxChargeStacks` | `int32` | 2 | GDD §5.2.4 | 最大蓄力层数 |
| `WhiteInterruptChargeDamageScale` | `float` | 0.3 | GDD §5.2.4 | 白攻打断蓄力时的伤害倍率 |
| `BlockWindowSeconds` | `float` | 0.25 | GDD §5.2.7 | 格挡判定窗口（秒），[PLAYTEST] |
| `DodgeWindowSeconds` | `float` | 0.35 | GDD §5.2.7 | 闪避判定窗口（秒），[PLAYTEST] |
| `DodgeFailDamageScale` | `float` | 1.2 | GDD §5.2.7 | 闪避失败额外伤害倍率 |
| `CritDamageMultiplier` | `float` | 1.5 | GDD §5.2.7 | 蓝攻暴击伤害倍率，[PLAYTEST] |
| `DodgeBuffDamageScale` | `float` | 1.2 | GDD §5.2.5 | 闪避成功后下回合伤害倍率，[PLAYTEST] |
| `DodgeBuffTurns` | `int32` | 1 | GDD §5.2.5 | 闪避Buff持续回合数 |
| `FirstStrikeDamageScale` | `float` | 0.3 | GDD §5.2.1 | 玩家先制攻击伤害比例（相对白攻基础），[PLAYTEST] |
| `GoldAttackDamageMin` | `float` | 25.0 | GDD §5.2.5 | 金色攻击（反击）最小伤害，[PLAYTEST] |
| `GoldAttackDamageMax` | `float` | 35.0 | GDD §5.2.5 | 金色攻击（反击）最大伤害，[PLAYTEST] |
| `FirstStrikeDisableChargeTurns` | `int32` | 1 | GDD §5.2.7 | 先制攻击使敌人禁用蓄力的回合数 |
| `PlayerDefaultHP` | `float` | 100.0 | GDD §5.2.7 | 玩家初始 HP（用于新角色默认值） |
| `RunAwayHPThreshold` | `float` | 0.3 | §5.2.1 类比 | 教学战斗敌人逃跑血量百分比阈值，[待定] |
| `Movement_BackwardSpeedScale` | `float` | 0.6 | GDD §5.1 | 后退速度倍率 |

> **注意：** WalkSpeed/SprintSpeed 已迁移至 DT_CharacterConfig（每个角色可独立设置移动速度）。
> **注意：** 此表用**单例模式**——一张表只有一行（RowName = `Default`），策划一次修改影响全局。
> **注意：** `PlayerDefaultHP` 仅是全局回退值；具体角色的 MaxHP 以 DT_CharacterConfig 行值为准（如主角 drifter = 120，见 §4.3）。
> 如果是不同难度分别配置，后续可扩展为多行（RowName = `Normal` / `Hard` / `Nightmare`）。

---

## 4. 02 — 角色配置

### 4.1 基本信息

| 项 | 值 |
|----|-----|
| 资产名 | `DT_CharacterConfig` |
| 结构体 | `FCharacterConfigRow` |
| 行 ID | 角色英文名 |
| 行数 | ~5（随开发增加） |
| 来源 | GDD §8.3 关键角色、§5.2.7 战斗参数表 |

### 4.2 列定义

| 列名 | 类型 | 默认值 | 备注 |
|------|------|--------|------|
| `DisplayName` | `FText` | — | 角色显示名（中文） |
| `CharacterClass` | `TSubclassOf<ABaseCharacter>` | — | 对应 C++/BP 类 |
| `MaxHP` | `float` | 100.0 | 最大生命值 |
| `MaxSmokeReserve` | `float` | 10.0 | `[待定]` 最大烟储备值 |
| `BaseDamageScale` | `float` | 1.0 | 角色基础伤害总倍率（乘法），叠加在全局基础之上 |
| `BlueAttackBonus` | `float` | 0.0 | 蓝色攻击固定加成（加法），叠加在倍率计算之后 |
| `WhiteAttackBonus` | `float` | 0.0 | 白色攻击固定加成（加法），叠加在倍率计算之后 |
| `WalkSpeed` | `float` | 300.0 | 默认走路速度（cm/s），不按 Shift 时的移动速度 |
| `SprintSpeed` | `float` | 600.0 | 默认跑动速度（cm/s），按住 Shift 时的移动速度 |
| `LandingLockTime` | `float` | 0.3 | 落地锁定时间（秒），跳跃/坠落后禁止移动的时长，防止落地动画滑步 |
| `DefaultWeaponID` | `FName` | — | 初始武器 ID（引用 DT_WeaponConfig） |
| `DefaultMaskID` | `FName` | — | 初始面具 ID（引用 DT_MaskConfig） |
| `bIsPlayable` | `bool` | false | 是否可被玩家操控 |
| `bHasSmokeGland` | `bool` | true | 是否有烟囊（魔法师=有，洞穴人类=无） |
| `DailySmokeRecoveryMin` | `float` | 0.0 | 每日自动回复烟储备最小值（艾斯=1，0=不回复） |
| `DailySmokeRecoveryMax` | `float` | 0.0 | 每日自动回复烟储备最大值（艾斯=2，0=不回复） |
| `UnlockCondition_Round` | `int32` | 1 | 第几次轮回后解锁 |
| `UnlockCondition_Desc` | `FText` | — | `[待定]` 解锁条件文字描述 |
| `PortraitTexture` | `TSoftObjectPtr<UTexture2D>` | — | 角色头像 |

### 4.3 行数据

> **数据口径（2026-08-01）：** 已按本节落库；`CharacterClass` 列保留现有蓝图引用（drifter → BP_Dale），其余角色待 BP 制作后补填。
> **艾斯烟回复：** `DailySmokeRecoveryMin/Max = 1/2`（每游戏日自动回复烟储备，GDD §5.3.3）。

| RowName | DisplayName | MaxHP | DmgScale | BlueBonus | WhiteBonus | WalkSpd | SprintSpd | bPlayable | bSmokeGland | UnlockRound | GDD 来源 |
|---------|-------------|-------|:--------:|:---------:|:----------:|:-------:|:---------:|-----------|-------------|-------------|----------|
| `drifter` | 漂泊者（主角） | 120 | 1.05 | 0.0 | 3.0 | 300 | 600 | ✅ | ❌ | 0 | §8.3 主角 |
| `ace` | 艾斯 | 100 | 0.95 | 5.0 | 0.0 | 300 | 600 | ✅ | ✅ | 0 | §8.3 艾斯 |

> **口径说明：** `UnlockRound` 从 0 开始计数（0 = 首次游戏）；艾斯第 0 次轮回即可加入。
| `inept_char` | `[待定]` 无能力者 | 90 | 1.0 | 0.0 | 0.0 | 300 | 600 | ✅ | ❌ | `[待定]` | §8.3 无能力者角色 |
| `doctor` | 博士 | 80 | 0.8 | 0.0 | 0.0 | 300 | 600 | ❌ | ✅ | 0 | §8.3 博士 (NPC) |
| `time_mage` | 时间魔法师 | `[待定]` | 1.3 | `[待定]` | `[待定]` | 300 | 600 | ❌ | ✅ | 0 | §8.3 时间魔法师 (NPC/Boss) |

### 4.4 伤害计算公式与数据流

#### 4.4.1 完整伤害公式

```
蓝色攻击伤害 = (
    DT_CombatParams.BlueAttackDamage(随机 20~30)
  × DT_CharacterConfig.BaseDamageScale       ← 角色天赋倍率
  × DT_WeaponConfig.BlueAttackDamageScale     ← 武器蓝攻倍率
  × 蓝攻面具倍率（DT_MaskConfig.ColorDamageScale_Blue）
  × 下回合伤害倍率（闪避Buff：NextAttackDamageScale）
  × 蓄力倍率(1.0 / 1.5 / 2.25)
) + DT_CharacterConfig.BlueAttackBonus       ← 角色蓝攻固定加成
  + DT_WeaponConfig.BlueAttackDamageMod       ← 武器蓝攻固定加成
  + DT_SkillTreeConfig 已解锁蓝攻被动         ← 局外永久加成
暴击：若 BlueCritChance 判定成功 → 最终伤害 × CritDamageMultiplier

白色攻击伤害 = (
    DT_CombatParams.WhiteAttackDamage(随机 15~25)
  × DT_CharacterConfig.BaseDamageScale
  × DT_WeaponConfig.WhiteAttackDamageScale
  × 白攻面具倍率（DT_MaskConfig.ColorDamageScale_White）
  × 下回合伤害倍率（闪避Buff：NextAttackDamageScale）
) + DT_CharacterConfig.WhiteAttackBonus
  + DT_WeaponConfig.WhiteAttackDamageMod
  + WhiteDmgBonus（白攻被动）
  + DT_SkillTreeConfig 已解锁白攻被动

金色攻击（反击）伤害 = Rand(GoldAttackDamageMin~Max) × (1 + CounterDmgBonus)
先制伤害 = 白攻基础伤害 × FirstStrikeDamageScale（玩家先制时）
```

#### 4.4.2 三层数据流 —— 默认值、配置值与运行时状态

先澄清一个容易混淆的点：**USTRUCT 成员写 `float MaxHP = 100.0f;` 是正确且必要的。** 这和"在 USTRUCT 里写计算逻辑"是两回事。

```
┌──────────────────────────────────────────────────────────┐
│ ① USTRUCT 成员默认值（C++ 编译层面，编辑器辅助）            │
│                                                          │
│   USTRUCT()                                               │
│   struct FCharacterConfigRow : FTableRowBase              │
│   {                                                       │
│       UPROPERTY(EditAnywhere)                             │
│       float MaxHP = 100.0f;     // ✅ 正确：编辑器默认值    │
│                                                          │
│       UPROPERTY(EditAnywhere)                             │
│       float BaseDamageScale = 1.0f;                      │
│   };                                                     │
│                                                          │
│   作用：                                                  │
│   · 编辑器中新增行时自动填充，减少策划漏填                   │
│   · FindRow() 失败时的安全回退                             │
│   · 不包含跨表引用、不包含计算公式                          │
└──────────────────────────────────────────────────────────┘
                         │
                         │ FindRow() + 跨表组合
                         ▼
┌──────────────────────────────────────────────────────────┐
│ ② DataTable 资产中的行值（策划数据，运行时的唯一真值）       │
│                                                          │
│   DT_CharacterConfig                                     │
│   ┌──────────┬────────┬──────────┐                       │
│   │ RowName  │ MaxHP  │ DmgScale │                       │
│   ├──────────┼────────┼──────────┤                       │
│   │ drifter  │ 120    │ 1.05     │ ← 这才是游戏数据        │
│   │ ace      │ 100    │ 0.95     │                       │
│   └──────────┴────────┴──────────┘                       │
│                                                          │
│   作用：策划在编辑器中修改，运行时读取                       │
└──────────────────────────────────────────────────────────┘
                         │
                         │ 由 Subsystem 读取、合并计算
                         ▼
┌──────────────────────────────────────────────────────────┐
│ ③ 运行时状态（CombatComponent / ABaseCharacter）           │
│                                                          │
│   class ABaseCharacter                                   │
│   {                                                      │
│       // ❌ 不应该出现：                                   │
│       // float MaxHP = 100.0f;  ← 硬编码游戏数据           │
│                                                          │
│       // ✅ 正确做法：字段存在，但值来自 DataTable           │
│       float MaxHP;     // 由 Subsystem 在初始化时赋值      │
│       float CurrentHP; // 运行时状态，每回合变化            │
│   };                                                    │
│                                                          │
│   初始化流程：                                             │
│   void UCombatComponent::InitializeFromConfig()           │
│   {                                                      │
│       const auto* Row = DT->FindRow<...>(CharacterID);    │
│       MaxHP = Row->MaxHP + SkillTreeBonus;  // 来自表     │
│       CurrentHP = MaxHP;                                 │
│   }                                                      │
└──────────────────────────────────────────────────────────┘
```

**USTRUCT 能做 vs 不能做的事：**

| ✅ 可以做 | ❌ 不能做 |
|----------|----------|
| `float MaxHP = 100.0f;` — 成员默认值 | 在 USTRUCT 里调用 `FindRow()` 跨表读数据 |
| `UPROPERTY(EditAnywhere)` — 编辑器暴露 | 在 USTRUCT 构造函数里做伤害计算 |
| `FText DisplayName;` — 纯数据字段 | 写 `PostLoad()` / `Initialize()` 自定义初始化函数 |
| `TSoftObjectPtr<UTexture2D>` — 资产引用 | 持有指向其他 DataTable 或 Subsystem 的指针 |

**简单判断标准：** USTRUCT 里能出现的 C++ 代码，只限于"如果这一行在 CSV/Excel 里，这个格子的值是什么"。一切需要"看其他表"或"做运算"的东西，都属于 Subsystem。

#### 4.4.3 运行时初始化流程

```
1. UCombatFormulaSubsystem::Initialize()
   ├── 加载 DT_CombatParams → 缓存到 TMap
   ├── 加载 DT_CharacterConfig → 缓存到 TMap<FName, FCharacterConfigRow*>
   ├── 加载 DT_WeaponConfig → 缓存到 TMap<FName, FWeaponConfigRow*>
   ├── 加载 DT_MaskConfig → 缓存到 TMap<FName, FMaskConfigRow*>
   └── 加载 DT_SkillTreeConfig → 缓存已解锁节点列表

2. 角色进入战斗时:
   ABaseCharacter::CombatComponent
   ├── CurrentWeaponID = "sword"      ← 来自装备槽
   ├── CurrentMaskID   = "mask_common" ← 来自装备槽
   └── 战斗选项被选中时调用:
       CombatFormulaSubsystem->CalculateBlueDamage(
           CharacterID  = "drifter",
           WeaponID     = CurrentWeaponID,
           ChargeStacks = 1
       )
```

#### 4.4.4 具体计算示例

**场景：** 漂泊者（主角）装备单手剑，1层蓄力，使用蓝攻，已解锁 `blue_crit_15` 技能树节点（+5% 暴击，简化假设 +5 固定伤害）。

```
全局基础         = RandomRange(20, 30) = 25
角色 BaseScale   = 1.05                 ← 漂泊者
角色 BlueBonus   = 0.0                  ← 漂泊者无蓝攻加成
武器 BlueScale   = 1.0                  ← 单手剑
武器 BlueMod     = 0.0                  ← 单手剑无额外加成
蓄力倍率         = 1.5                  ← 1层蓄力
技能树加值       = 5.0                  ← blue_crit_15 简化为 +5

最终伤害 = (25 × 1.05 × 1.0 × 1.5) + 0.0 + 0.0 + 5.0
        = 39.375 + 5.0
        = 44.375
```

**换大剑后：**
```
最终伤害 = (25 × 1.05 × 1.3 × 1.5) + 0.0 + 0.0 + 5.0
        = 51.2 + 5.0
        = 56.2
```

**同场景换艾斯（单手剑，无蓝攻被动）：**
```
最终伤害 = (25 × 0.95 × 1.0 × 1.5) + 5.0 + 0.0 + 0.0
        = 35.625 + 5.0
        = 40.625
```

> **数值设计意图：** 漂泊者 BaseScale 1.05 > 艾斯 0.95，但艾斯有 +5.0 蓝攻固定加成——在低蓄力时漂泊者更强（猎手爆发），在高基础值 / 高蓄力层数时由于乘法叠加速度更快，漂泊者持续领先。但艾斯身为魔法师拥有烟囊可自回，技能使用更频繁，总输出通过技能频率弥补。

#### 4.4.5 属性存储方案：显式字段 vs TMap 属性容器

三种做法：

**做法 A（显式 float 字段 + 缓存）**

```cpp
class ABaseCharacter : public ACharacter
{
    float MaxHP;      // 由 DT + 技能树赋值，不写游戏默认值
    float CurrentHP;  // 运行时状态
};
```

**做法 C（TMap + 属性修正器）**

```cpp
class UAttributeComponent : public UActorComponent
{
    TMap<FName, float> BaseAttributes;         // DT 加载的基值: {"MaxHP": 120, ...}
    TMap<FName, float> CachedFinalAttributes;  // 基值 + 所有 modifier 后的最终值

    TArray<FAttributeModifier> ActiveModifiers; // buff/debuff 栈
};

struct FAttributeModifier
{
    FName   AttributeName;   // "WhiteAttackBonus"
    float   Value;           // +5.0 或 1.2
    EModifierOp Op;          // Add / Multiply
    int32   RemainingTurns;  // 持续回合数，0 = 永久（装备/技能树）
};
```

**结论：Hole 选做法 C。** 不是性能原因（回合制下差异不可感知），而是 buff/debuff 系统的适配。

Hole 的 GDD 已定义了多种需要临时修改属性的机制：

| GDD 来源 | 效果 | 需要的属性修正 |
|----------|------|---------------|
| §5.2.5 闪避成功 | 下回合伤害 +20% | `WhiteAtkBonus` ×1.2，持续 1 回合 |
| §6.3.2 面具 | 特定颜色攻击伤害 +15% | 装备驱动的永久修正 |
| §7.4 肉体强化技能 | 减伤 30% 持续 2 回合 | `DamageTakenScale` ×0.7，持续 2 回合 |
| §7.4 肉体强化技能 | 下一击伤害 +50% | `NextAtkBonus` ×1.5，持续 1 回合 |
| §5.3.2 边境之烟道具 | 格挡窗口扩大 | `BlockWindow` +0.03s，永久 |

**做法 A 面对这些 buff 的困境**：

```cpp
// 每加一种 buff，就要：
// ① 在 Character 头文件中加字段
// ② 手写 ApplyXxx()
// ③ 手写 RemoveXxx()
// ④ 手写回合倒数逻辑
// ⑤ 重编译

void ABaseCharacter::ApplyDodgeBuff()   { WhiteAtkBonus *= 1.2f; /* + 计时器 + 还原 */ }
void ABaseCharacter::ApplyPhysDefBuff() { DamageTakenScale *= 0.7f; /* + 计时器 + 还原 */ }
// ... 10 种 buff = 10 套样板代码
```

**做法 C 统一处理**：

```cpp
// 每种 buff = 一行调用。不改头文件，不重编译，不加字段。

// 闪避 buff
AttributeComp->AddModifier("WhiteAtkBonus", Multiply, 1.2f, 1);

// 减伤 buff
AttributeComp->AddModifier("DamageTakenScale", Multiply, 0.7f, 2);

// 面具效果（永久修正，RemainingTurns = 0）
AttributeComp->AddModifier("BlueAtkBonus", Multiply, 1.15f, 0);

// 回合结束时自动倒计时、自动移除过期 modifier
AttributeComp->TickTurn();

// 属性读取时自动叠加所有 modifier
float final = AttributeComp->GetFinal("WhiteAtkBonus");
// = BaseAttributes["WhiteAtkBonus"] + Σ(Add modifiers) × Π(Multiply modifiers)
```

**三种方案对比**：

| 维度 | A（显式字段） | C（TMap + 修正器） |
|------|:---:|:---:|
| 添加新属性 | 改 Character 头文件 + 重编译 | 改 DT，Component 自动识别 |
| 添加一种 buff | ~20 行样板代码 | 1 行 `AddModifier()` |
| 修改器（buff/debuff/装备/被动） | 需额外造一套系统 | ✅ 天然内置 |
| 编译依赖 | 属性越多头文件越重 | Component 固定，不改动 |
| 调试 | ✅ 断点直接看字段值 | 需查看 TMap + modifier 栈 |
| 属性名拼写 | 编译器检查 | 运行时 FName 匹配，建议写常量 |
| 回合制性能 | ~1ns/次 | ~50ns/次（不可感知） |

**防拼写错误的手段**：

```cpp
// 定义属性名常量，避免字符串硬编码
namespace AttributeNames
{
    FORCEINLINE FName MaxHP()             { return FName(TEXT("MaxHP")); }
    FORCEINLINE FName WhiteAtkBonus()     { return FName(TEXT("WhiteAtkBonus")); }
    FORCEINLINE FName BlueAtkBonus()      { return FName(TEXT("BlueAtkBonus")); }
    FORCEINLINE FName BlockWindow()       { return FName(TEXT("BlockWindow")); }
    FORCEINLINE FName DamageTakenScale()  { return FName(TEXT("DamageTakenScale")); }
}

// 使用
AttributeComp->AddModifier(AttributeNames::WhiteAtkBonus(), Multiply, 1.2f, 1);
```

**仅需保留在 Character 上的字段**：`CurrentHP`（运行时状态，每回合变化）。其他所有属性——包括 `MaxHP`、伤害修正、窗口时间——全部进入 `UAttributeComponent` 的 TMap。

---

## 5. 03 — 敌人配置

### 5.1 基本信息

| 项 | 值 |
|----|-----|
| 资产名 | `DT_EnemyConfig` |
| 结构体 | `FEnemyConfigRow` |
| 行 ID | 敌人英文类型名 |
| 行数 | ~8 |
| 来源 | GDD §7.2 敌人类型与分布 |

### 5.2 新增枚举

```cpp
UENUM(BlueprintType)
enum class EEnemyAIPreference : uint8
{
    Balanced      UMETA(DisplayName = "均衡型"),
    PreferWhite    UMETA(DisplayName = "偏好白攻"),
    PreferBlue     UMETA(DisplayName = "偏好蓝攻"),
    PreferCharge   UMETA(DisplayName = "偏好蓄力"),
    Adaptive       UMETA(DisplayName = "自适应型"),
    Random         UMETA(DisplayName = "随机型")
};

UENUM(BlueprintType)
enum class EEnemyTier : uint8
{
    Tutorial    UMETA(DisplayName = "教学级"),
    Normal      UMETA(DisplayName = "普通"),
    Elite       UMETA(DisplayName = "精英"),
    Boss        UMETA(DisplayName = "Boss"),
    FinalBoss   UMETA(DisplayName = "最终Boss")
};
```

### 5.3 列定义

| 列名 | 类型 | 默认值 | 备注 |
|------|------|--------|------|
| `DisplayName` | `FText` | — | 敌人显示名（中文） |
| `EnemyClass` | `TSubclassOf<AEnemy>` | — | 对应 C++ / Blueprint 类（敌人生成时使用） |
| `Tier` | `EEnemyTier` | Normal | 敌人等级 |
| `MaxHP` | `float` | 80.0 | 最大生命值 |
| `BaseDamageScale` | `float` | 1.0 | 伤害输出倍率（相对战斗参数基值） |
| `AIPreference` | `EEnemyAIPreference` | Balanced | AI 行为偏好 |
| `AIDifficulty` | `float` | 0.5 | AI 智能度（0=随机，1=最优决策），影响"预判玩家行为"的概率 |
| `WalkSpeed` | `float` | 300.0 | 默认走路速度（cm/s），2026-08-01 新增列（原为代码硬编码） |
| `SprintSpeed` | `float` | 600.0 | 默认跑动速度（cm/s），2026-08-01 新增列 |
| `LandingLockTime` | `float` | 0.3 | 落地锁定时间（秒），2026-08-01 新增列 |
| `DropSmokeType` | `FName` | — | 击败后掉落的烟类型 ID（引用 DT_SmokeConfig） |
| `DropSmokeCount` | `int32` | 1 | 掉落烟数量 |
| `DropCurrencyMin` | `int32` | 0 | 掉落货币最小值 |
| `DropCurrencyMax` | `int32` | 0 | 掉落货币最大值 |
| `SpawnAreas` | `FString` | — | 出现区域列表（逗号分隔 AreaID，如 `"town,market"`），`[待定]` 可改为数组 |
| `AlertRange` | `float` | 1500.0 | `[待定]` 警觉范围（cm），GDD §5.5 |
| `ChaseRange` | `float` | 900.0 | `[待定]` 追击范围（cm），GDD §5.5 |

### 5.4 行数据

| RowName | DisplayName | Tier | MaxHP | AIPreference | DropSmoke | SpawnAreas | GDD §7.2 |
|---------|-------------|------|-------|-------------|-----------|------------|----------|
| `inept` | 无能力者 | Tutorial | 50 | PreferWhite | `hunter_smoke` | `hole,town_outskirts` | 第1行 |
| `apprentice` | 低级魔法师 | Normal | 80 | Balanced | `apprentice_smoke` | `town,market` | 第2行 |
| `adept` | 高级魔法师 | Elite | 150 | Adaptive | `adept_smoke` | `market,mansion` | 第3行 |
| `commander` | 魔法师统领 | Boss | 500 | Adaptive | `commander_smoke` | `mansion` | 第4行 |
| `border_guard` | 边境守卫 | Elite | 200 | PreferCharge | `border_smoke` | `border` | 第5行 |
| `demon` | 恶魔 | Elite | 350 | Random | `demon_smoke` | `hell` | 第6行 |
| `satan` | 撒旦 | FinalBoss | `[待定]` | Adaptive | `satan_smoke` | `hell` | 第7行 |
| `friendly_creature` | 友善生物 | Tutorial | 20 | Balanced | `healing_smoke` | `all_areas_hidden` | 第8行 |

---

## 6. 04 — 武器配置

### 6.1 基本信息

| 项 | 值 |
|----|-----|
| 资产名 | `DT_WeaponConfig` |
| 结构体 | `FWeaponConfigRow` |
| 行 ID | 武器英文名 |
| 行数 | ~5（首发目标 8-12） |
| 来源 | GDD §6.3.1 武器 |

### 6.2 新增枚举

```cpp
UENUM(BlueprintType)
enum class EWeaponCategory : uint8
{
    GreatSword  UMETA(DisplayName = "大剑"),
    Hammer      UMETA(DisplayName = "锤子"),
    Sword       UMETA(DisplayName = "单手剑"),
    TBD1        UMETA(DisplayName = "[待定] 武器4"),
    TBD2        UMETA(DisplayName = "[待定] 武器5")
};
```

### 6.3 列定义

| 列名 | 类型 | 默认值 | 备注 |
|------|------|--------|------|
| `DisplayName` | `FText` | — | 武器显示名 |
| `WeaponClass` | `TSubclassOf<AWeapon>` | — | 对应 C++ / Blueprint 类（武器实例化时使用） |
| `Category` | `EWeaponCategory` | Sword | 武器类别 |
| `Description` | `FText` | — | 武器描述（碎片叙事文本） |
| `BlueAttackDamageMod` | `float` | 0.0 | 蓝色攻击伤害加成（加法） |
| `WhiteAttackDamageMod` | `float` | 0.0 | 白色攻击伤害加成（加法） |
| `BlueAttackDamageScale` | `float` | 1.0 | 蓝色攻击伤害倍率（乘法） |
| `WhiteAttackDamageScale` | `float` | 1.0 | 白色攻击伤害倍率（乘法） |
| `BlockWindowBonus` | `float` | 0.0 | 格挡窗口加成（秒） |
| `DodgeWindowBonus` | `float` | 0.0 | 闪避窗口加成（秒） |
| `RedPenetrationScale` | `float` | 0.0 | 对红色防御的穿透伤害比例（0~1） |
| `ExtraChargeTurns` | `int32` | 0 | 额外蓄力回合数（大剑 +1） |
| `Price` | `int32` | 0 | 商店售价（货币） |
| `IconTexture` | `TSoftObjectPtr<UTexture2D>` | — | 武器图标 |
| `MeshAsset` | `TSoftObjectPtr<UStaticMesh>` | — | 武器模型 |

### 6.4 行数据

| RowName | Category | BlueAtkScale | WhiteAtkScale | BlockBonus | Penetration | ExtraCharge | GDD §6.3.1 |
|---------|----------|-------------|---------------|------------|-------------|-------------|------------|
| `great_sword` | GreatSword | 1.3 | 1.0 | 0.0 | 0.0 | 1 | 第1行 |
| `hammer` | Hammer | 1.0 | 1.0 | 0.0 | 0.5 | 0 | 第2行 |
| `sword` | Sword | 1.0 | 1.0 | 0.1 | 0.0 | 0 | 第3行 |
| `tbd_weapon_1` | TBD1 | 1.0 | 1.0 | 0.0 | 0.0 | 0 | `[待定]` 第4行 |

---

## 7. 05 — 面具配置

### 7.1 基本信息

| 项 | 值 |
|----|-----|
| 资产名 | `DT_MaskConfig` |
| 结构体 | `FMaskConfigRow` |
| 行 ID | 面具英文名 |
| 行数 | ~5（首发目标 15-20） |
| 来源 | GDD §6.3.2 面具 |

### 7.2 新增枚举

```cpp
UENUM(BlueprintType)
enum class EMaskRarity : uint8
{
    Common   UMETA(DisplayName = "普通"),
    Rare     UMETA(DisplayName = "稀有"),
    Legendary UMETA(DisplayName = "传说"),
    Demonic   UMETA(DisplayName = "恶魔")
};
```

### 7.3 列定义

| 列名 | 类型 | 默认值 | 备注 |
|------|------|--------|------|
| `DisplayName` | `FText` | — | 面具显示名 |
| `Rarity` | `EMaskRarity` | Common | 稀有度 |
| `Description` | `FText` | — | 面具描述（碎片叙事文本） |
| `SmokeGainScale` | `float` | 1.0 | 烟获取量倍率（1.0=无加成，1.1=+10%） |
| `ColorDamageScale_Red` | `float` | 1.0 | 红色攻击伤害倍率（1.0=无加成，1.15=+15%） |
| `ColorDamageScale_Blue` | `float` | 1.0 | 蓝色攻击伤害倍率（1.0=无加成） |
| `ColorDamageScale_White` | `float` | 1.0 | 白色攻击伤害倍率（1.0=无加成） |
| `HPRegenOnKill` | `float` | 0.0 | 击败敌人后回复 HP 百分比（如 0.05 = 5%） |
| `SkillCostScale` | `float` | 1.0 | 技能消耗倍率（传说面具=0.5） |
| `DropChance` | `float` | 0.0 | 掉落概率（0~1），`[待定]` 击败哪种敌人可掉落 |
| `Price` | `int32` | 0 | 商店售价 |
| `IconTexture` | `TSoftObjectPtr<UTexture2D>` | — | 面具图标 |
| `MeshAsset` | `TSoftObjectPtr<USkeletalMesh>` | — | 面具模型 |

> **修正说明（2026-08-01）：** 上述倍率列默认值统一为 1.0（此前误写 0.0，会导致装备面具后属性归零）；C++ 结构体与资产已同步。

### 7.4 行数据

| RowName | Rarity | SmokeGain | HPRegen | SkillCost | Price | GDD §6.3.2 |
|---------|--------|-----------|---------|-----------|-------|------------|
| `mask_common` | Common | 1.1 | 0.0 | 1.0 | 100 | 普通面具 |
| `mask_rare_fire` | Rare | 1.0 | 0.0 | 1.0 | 800 | 稀有面具（红攻+15%） |
| `mask_legendary_lifesteal` | Legendary | 1.0 | 0.05 | 1.0 | 2000 | 传说面具（击杀回血） |
| `mask_demonic_cost` | Demonic | 1.0 | 0.0 | 0.5 | 5000 | 恶魔面具（技能减半） |
| `mask_tbd` | `[待定]` | 1.0 | 0.0 | 1.0 | `[待定]` | `[待定]` 更多面具 |

---

## 8. 06 — 烟分类配置

### 8.1 基本信息

| 项 | 值 |
|----|-----|
| 资产名 | `DT_SmokeConfig` |
| 结构体 | `FSmokeConfigRow` |
| 行 ID | 烟类型英文名 |
| 行数 | 9 |
| 来源 | GDD §5.3.2 烟的分类体系 |

### 8.2 新增枚举

```cpp
UENUM(BlueprintType)
enum class ESmokeSource : uint8
{
    Inept       UMETA(DisplayName = "无能力者"),
    Apprentice  UMETA(DisplayName = "低级魔法师"),
    Adept       UMETA(DisplayName = "高级魔法师"),
    Commander   UMETA(DisplayName = "魔法师统领"),
    BorderGuard UMETA(DisplayName = "边境守卫"),
    Demon       UMETA(DisplayName = "恶魔"),
    Satan       UMETA(DisplayName = "撒旦"),
    Friendly    UMETA(DisplayName = "友善生物"),
    Puzzle      UMETA(DisplayName = "解密/隐藏区域")
};
```

### 8.3 列定义

| 列名 | 类型 | 默认值 | 备注 |
|------|------|--------|------|
| `DisplayName` | `FText` | — | 烟显示名（中文） |
| `Source` | `ESmokeSource` | Inept | 来源敌人分类 |
| `PowerImprint` | `FText` | — | 力量印记描述（给策划看的，不显示在游戏中） |
| `bConvertToSkill` | `bool` | false | 是否自动转化为技能 |
| `ConvertedSkillID` | `FName` | — | 转化后的技能 ID（引用 DT_SkillConfig，可不填） |
| `bConvertToItem` | `bool` | false | 是否自动转化为道具 |
| `ConvertedItemID` | `FName` | — | 转化后的道具 ID（引用 DT_ConsumableConfig，可不填） |
| `bCanRefineToPassive` | `bool` | false | 是否可在博士处提炼为永久被动 |
| `bCanExchangeForCurrency` | `bool` | false | 是否可兑换为货币（仅治愈之烟为 true） |
| `CurrencyPerSmoke` | `int32` | 0 | 兑换货币数量（仅治愈之烟），`[待定]` 约 50-100 |
| `CanDirectHealSmokeReserve` | `bool` | false | 是否可直接回复烟储备（仅治愈之烟为 true） |
| `DirectHealAmount` | `float` | 0.0 | 直接回复烟储备量 |

### 8.4 行数据

| RowName | DisplayName | Source | ConvertSkill | ConvertItem | RefinePassive | ExchangeCurrency | GDD §5.3.2 |
|---------|-------------|--------|-------------|-------------|---------------|-----------------|------------|
| `hunter_smoke` | 猎手之烟 | Inept | ✅ 探测技能 | ✅ 白攻伤害道具 | ❌ | ❌ | 第1行 |
| `apprentice_smoke` | 学徒之烟 | Apprentice | ✅ 基础魔法技能 | ✅ 蓝攻基础提升 | ❌ | ❌ | 第2行 |
| `adept_smoke` | 术者之烟 | Adept | ✅ 进阶技能 | ✅ 蓄力速度道具 | ✅ | ❌ | 第3行 |
| `commander_smoke` | 统领之烟 | Commander | ✅ 专属技能 | ✅ 稀有面具 | ✅ | ❌ | 第4行 |
| `border_smoke` | 边境之烟 | BorderGuard | ❌ | ✅ 格挡窗口道具 | ✅ | ❌ | 第5行 |
| `demon_smoke` | 恶魔之烟 | Demon | ✅ 最强攻击技能 | ✅ 传说面具 | ✅ | ❌ | 第6行 |
| `satan_smoke` | 撒旦之烟 | Satan | ✅ 终极技能 | ✅ 特殊结局道具 | ❌ | ❌ | 第7行 |
| `healing_smoke` | 治愈之烟 | Friendly | ❌ | ❌ | ❌ | ✅ | 第8行 |
| `puzzle_smoke` | 谜题之烟 | Puzzle | ✅ 独特被动技能 | ✅ 剧情碎片 | ❌ | ❌ | 第9行 |

> **设计说明：** `ConvertedSkillID` / `ConvertedItemID` 是可选的外键引用——如果击败该敌人**必定**掉落某个特定技能/物品，则在此指定。如果是**随机掉落**（从该类别的技能/物品池中随机），则由掉落系统处理，不在此表中。

---

## 9. 07 — 技能配置

### 9.1 基本信息

| 项 | 值 |
|----|-----|
| 资产名 | `DT_SkillConfig` |
| 结构体 | `FSkillConfigRow` |
| 行 ID | 技能英文名 |
| 行数 | ~25（首发目标 20-30） |
| 来源 | GDD §7.4 技能/能力分类 |

### 9.2 新增枚举

```cpp
UENUM(BlueprintType)
enum class ESkillCategory : uint8
{
    Healing      UMETA(DisplayName = "治愈类"),
    PhysEnhance  UMETA(DisplayName = "肉体强化类"),
    Attack       UMETA(DisplayName = "攻击类"),
    Mental       UMETA(DisplayName = "精神类"),
    Exclusive    UMETA(DisplayName = "专属技能"),
    TimeMagic    UMETA(DisplayName = "时间魔法类")
};

UENUM(BlueprintType)
enum class ESkillTarget : uint8
{
    Self         UMETA(DisplayName = "自身"),
    SingleEnemy  UMETA(DisplayName = "单体敌人"),
    AllEnemies   UMETA(DisplayName = "全体敌人"),
    Ally         UMETA(DisplayName = "友方"),
    AllAllies    UMETA(DisplayName = "全体友方")
};
```

### 9.3 列定义

| 列名 | 类型 | 默认值 | 备注 |
|------|------|--------|------|
| `DisplayName` | `FText` | — | 技能显示名 |
| `SkillClass` | `TSubclassOf<ASkill>` | — | 对应 C++ / Blueprint 类（技能执行时使用） |
| `Category` | `ESkillCategory` | Attack | 技能类别 |
| `Description` | `FText` | — | 技能描述（游戏内显示） |
| `SmokeCost` | `float` | 1.0 | 烟储备消耗量 |
| `CooldownTurns` | `int32` | 0 | 冷却回合数（0=无冷却） |
| `TargetType` | `ESkillTarget` | SingleEnemy | 目标类型 |
| `BaseEffectValue` | `float` | 10.0 | 基础效果数值（伤害量/回复量/增益百分比等） |
| `EffectDurationTurns` | `int32` | 0 | 效果持续回合数（0=即时效果） |
| `bIgnoresElementalColor` | `bool` | true | 是否无视颜色克制（GDD §5.2.6：所有技能均无视） |
| `bRetainedAcrossLoops` | `bool` | true | 技能是否在轮回间保留 |
| `ObtainFromSmokeType` | `FName` | — | 获取来源烟类型（引用 DT_SmokeConfig） |
| `ObtainDescription` | `FText` | — | 获取方式文字描述 |
| `MaxUsesPerLoop` | `int32` | -1 | 每轮轮回最大使用次数（-1=无限） |
| `IconTexture` | `TSoftObjectPtr<UTexture2D>` | — | 技能图标 |

### 9.4 行数据（示例）

| RowName | Category | SmokeCost | Cooldown | Target | BaseValue | Duration | ObtainFrom | GDD §7.4 |
|---------|----------|-----------|----------|--------|-----------|----------|------------|----------|
| `heal_30` | Healing | 2.5 | 3 | Self | 30.0（HP%） | 0 | `healing_smoke` | 治愈类 |
| `heal_clear_debuff` | Healing | 3.0 | 4 | Self | 0 | 0 | `healing_smoke` | 治愈类 |
| `phys_dmg_up_50` | PhysEnhance | 3.5 | 2 | Self | 50.0（伤害+%） | 1 | `border_smoke` | 肉体强化 |
| `phys_def_up_30` | PhysEnhance | 3.0 | 2 | Self | 30.0（减伤%） | 2 | `border_smoke` | 肉体强化 |
| `atk_fixed_dmg` | Attack | 3.0 | 1 | SingleEnemy | 40.0 | 0 | `adept_smoke` | 攻击类 |
| `atk_aoe` | Attack | 5.0 | 3 | AllEnemies | 25.0 | 0 | `adept_smoke` | 攻击类 |
| `mental_read_next` | Mental | 2.0 | 2 | SingleEnemy | 0 | 1 | `hunter_smoke` | 精神类 |
| `mental_lower_ai` | Mental | 3.0 | 3 | SingleEnemy | 0 | 1 | `hunter_smoke` | 精神类 |
| `exclusive_barrier` | Exclusive | 6.5 | 5 | Self | 0 | 3 | `commander_smoke` | 专属技能 |
| `time_rewind_turn` | TimeMagic | 10.0 | 0 | Self | 0 | 0 | `[TBD]` | 时间魔法（1次/轮回） |

> `[待定]` 完整技能表需策划填充 ~15 个额外技能条目。
> **`[暂缓]`：** 时间回溯技能暂不实现（时间轮回 = 新一轮游戏），`time_rewind_turn` 行保留但停用。

---

## 10. 08 — 技能树节点

### 10.1 基本信息

| 项 | 值 |
|----|-----|
| 资产名 | `DT_SkillTreeConfig` |
| 结构体 | `FSkillTreeConfigRow` |
| 行 ID | 节点英文名 |
| 行数 | ~12 |
| 来源 | GDD §6.2 局外技能树 |

### 10.2 新增枚举

```cpp
UENUM(BlueprintType)
enum class ESkillTreeBranch : uint8
{
    Foundation   UMETA(DisplayName = "基础强化"),
    RedSpecialty  UMETA(DisplayName = "红色专精"),
    BlueSpecialty UMETA(DisplayName = "蓝色专精"),
    WhiteSpecialty UMETA(DisplayName = "白色专精"),
    DefenseSpecialty UMETA(DisplayName = "防御专精"),
    CounterSpecialty UMETA(DisplayName = "反击专精"),
    DodgeSpecialty UMETA(DisplayName = "闪避专精")
};
```

### 10.3 列定义

| 列名 | 类型 | 默认值 | 备注 |
|------|------|--------|------|
| `DisplayName` | `FText` | — | 节点显示名 |
| `Branch` | `ESkillTreeBranch` | Foundation | 所属分支 |
| `Tier` | `int32` | 0 | 层级（0=根，2=最深） |
| `ParentNodeID` | `FName` | — | 前置节点 RowName（空=根节点） |
| `Description` | `FText` | — | 效果描述 |
| `RequiredSmokeType` | `FName` | — | 解锁所需的烟类型（引用 DT_SmokeConfig） |
| `RequiredSmokeCount` | `int32` | 1 | 需要消耗的烟数量 |
| `CurrencyCost` | `int32` | 100 | 博士提炼费用（货币） |
| `EffectType` | `FString` | — | 效果类型标识（代码用），如 `"MaxHP"`, `"BlockWindow"` |
| `EffectValue` | `float` | 0.0 | 效果数值 |

### 10.4 行数据

| RowName | Branch | Tier | Parent | ReqSmoke | EffectType | EffectValue | GDD §6.2 |
|---------|--------|------|--------|----------|------------|-------------|----------|
| `foundation_hp_1` | Foundation | 0 | — | `adept_smoke` | MaxHP | 20.0 | 基础强化 HP+20 |
| `foundation_hp_2` | Foundation | 0 | `foundation_hp_1` | `adept_smoke` | MaxHP | 40.0 | 基础强化 HP+40 |
| `red_extend_window_1` | RedSpecialty | 1 | `foundation_hp_1` | `border_smoke` | BlockWindow | 0.1s | 红色抵抗时间+0.1s |
| `red_extend_window_2` | RedSpecialty | 1 | `red_extend_window_1` | `border_smoke` | BlockWindow | 0.2s | 红色抵抗时间+0.2s |
| `blue_charge_speed` | BlueSpecialty | 1 | `foundation_hp_1` | `adept_smoke` | ChargeSpeedBonus | 1 | 蓄力速度+1 |
| `blue_crit_15` | BlueSpecialty | 1 | `blue_charge_speed` | `adept_smoke` | BlueCritChance | 15% | 蓝攻暴击15% |
| `white_dmg_5` | WhiteSpecialty | 1 | `foundation_hp_1` | `hunter_smoke` | WhiteDmgBonus | 5.0 | 白攻伤害+5 |
| `white_interrupt_double` | WhiteSpecialty | 1 | `white_dmg_5` | `hunter_smoke` | InterruptDmgScale | 2.0 | 打断伤害翻倍 |
| `defense_block_window` | DefenseSpecialty | 2 | `red_extend_window_1` | `border_smoke` | BlockWindow | 0.05s | 格挡窗口+ |
| `defense_block_reduce` | DefenseSpecialty | 2 | `defense_block_window` | `border_smoke` | BlockDmgReduce | 20% | 格挡减伤+ |
| `counter_dmg_up` | CounterSpecialty | 2 | `blue_crit_15` | `adept_smoke` | CounterDmgBonus | 30% | 反击伤害+ |
| `counter_heal` | CounterSpecialty | 2 | `counter_dmg_up` | `adept_smoke` | CounterHealPercent | 10% | 反击回血 |
| `dodge_window_up` | DodgeSpecialty | 2 | `white_dmg_5` | `hunter_smoke` | DodgeWindow | 0.05s | 闪避窗口+ |
| `dodge_buff_up` | DodgeSpecialty | 2 | `dodge_window_up` | `hunter_smoke` | DodgeBuffBonus | 10% | 闪避Buff+ |
| `legendary_node` | Foundation | 3 | `defense_block_reduce` | `demon_smoke` | `[待定]` | `[待定]` | 最深传说节点 |

### 10.5 EffectType 字典（2026-08-01 定案）

技能树节点的 `EffectType` 字符串与运行时属性常量的映射，代码按此表统一解析：

| EffectType 字符串 | AttributeNames 常量 | 基值 | 语义 |
|-------------------|---------------------|------|------|
| `MaxHP` | `MaxHP` | 由角色行 | 最大生命值 |
| `BlockWindow` | `BlockWindow` | 由战斗参数（0.25s） | 格挡判定窗口 |
| `ChargeSpeedBonus` | `ChargeSpeedBonus` | 0 | 蓄力速度加成 |
| `BlueCritChance` | `BlueCritChance` | 0 | 蓝攻暴击率（0~1） |
| `WhiteDmgBonus` | `WhiteDmgBonus` | 0 | 白攻固定伤害加成 |
| `InterruptDmgScale` | `InterruptDmgScale` | 1.0 | 白攻 vs 蓄力抵抗伤害倍率 |
| `BlockDmgReduce` | `BlockDmgReduce` | 0 | 格挡减伤比例 |
| `CounterDmgBonus` | `CounterDmgBonus` | 0 | 反击伤害加成（金色攻击 ×(1+值)） |
| `CounterHealPercent` | `CounterHealPercent` | 0 | 反击回血百分比 |
| `DodgeWindow` | `DodgeWindow` | 由战斗参数（0.35s） | 闪避判定窗口 |
| `DodgeBuffBonus` | `DodgeBuffBonus` | 0 | 闪避Buff强度加成 |

---

## 11. 09 — 区域配置

### 11.1 基本信息

| 项 | 值 |
|----|-----|
| 资产名 | `DT_AreaConfig` |
| 结构体 | `FAreaConfigRow` |
| 行 ID | 区域英文名 |
| 行数 | 6 |
| 来源 | GDD §7.1 关卡/地图/世界区域 |

### 11.2 列定义

| 列名 | 类型 | 默认值 | 备注 |
|------|------|--------|------|
| `DisplayName` | `FText` | — | 区域显示名（中文） |
| `UnlockRound` | `int32` | 1 | 第几次轮回起解锁 |
| `DifficultyStars` | `int32` | 1 | 难度星级（1~5） |
| `VisualTheme` | `FText` | — | 视觉主题描述（给美术参考） |
| `PrimaryColor` | `FColor` | — | 主色调（RGB） |
| `bIsSafeZone` | `bool` | false | 是否为安全区（无强制战斗） |
| `bIsLinear` | `bool` | false | 是否为线性关卡（不可返回） |
| `bFirstLoopOnly` | `bool` | false | 仅第0次轮回（首次游戏）可进入（序章洞穴=true） |
| `SpecialMechanics` | `FText` | — | 特殊机制描述 |
| `EnemyLevelScale` | `float` | 1.0 | 敌人等级缩放（影响 HP/伤害） |
| `LevelAsset` | `TSoftObjectPtr<UWorld>` | — | 关卡资产路径 |

### 11.3 行数据

| RowName | DisplayName | Round | Stars | SafeZone | Linear | GDD §7.1 |
|---------|-------------|-------|-------|----------|--------|----------|
| `hole` | 洞穴 | 0（仅第0次轮回） | 教学 | ❌ | ❌ | 第1行 |
| `town` | 小镇 | 1 | ★☆☆☆☆ | ✅ | ❌ | 第2行 |
| `market` | 市中心 | 1 | ★★☆☆☆ | ❌ | ❌ | 第3行 |
| `mansion` | 统领者宅院 | 1 | ★★★☆☆ | ❌ | ✅ | 第4行 |
| `border` | 边境 | 3 | ★★★★☆ | ❌ | ❌ | 第5行 |
| `hell` | 地狱 | 3 | ★★★★★ | ❌ | ✅ | 第6行 |

> **口径说明：** `UnlockRound` 从 0 开始计数；边境/地狱第 3 次轮回解锁 = 第 4 次游戏。

> `[待定]` 序章出现的"洞穴"区域是否需要单独的关卡资产，或是作为过场动画处理。

---

## 12. 10 — 消耗品配置

### 12.1 基本信息

| 项 | 值 |
|----|-----|
| 资产名 | `DT_ConsumableConfig` |
| 结构体 | `FConsumableConfigRow` |
| 行 ID | 消耗品英文名 |
| 行数 | ~10 |
| 来源 | GDD §7.3 物品/装备分类、§5.3.5 烟使用汇总 |

### 12.2 新增枚举

```cpp
UENUM(BlueprintType)
enum class EConsumableType : uint8
{
    HPRecovery   UMETA(DisplayName = "HP回复"),
    SmokeRecovery UMETA(DisplayName = "烟储备回复"),
    TempBuff     UMETA(DisplayName = "临时增益"),
    DamageItem   UMETA(DisplayName = "伤害道具"),
    StatBoost    UMETA(DisplayName = "属性提升"),
    KeyItem      UMETA(DisplayName = "关键道具")
};
```

### 12.3 列定义

| 列名 | 类型 | 默认值 | 备注 |
|------|------|--------|------|
| `DisplayName` | `FText` | — | 消耗品显示名 |
| `ConsumableClass` | `TSubclassOf<AConsumable>` | — | 对应 C++ / Blueprint 类（消耗品使用时实例化） |
| `Type` | `EConsumableType` | HPRecovery | 消耗品类型 |
| `Description` | `FText` | — | 物品描述（碎片叙事文本） |
| `EffectValue` | `float` | 0.0 | 基础效果数值 |
| `EffectDurationTurns` | `int32` | 0 | 效果持续回合数（0=即时） |
| `MaxCarryCount` | `int32` | 5 | 最大携带量 |
| `Price` | `int32` | 0 | 商店售价 |
| `bUsableInCombat` | `bool` | true | 是否可在战斗中使用 |
| `bRetainedAcrossLoops` | `bool` | false | 是否在轮回间保留 |
| `ObtainFromSmokeType` | `FName` | — | 从哪种烟转化而来（引用 DT_SmokeConfig） |
| `IconTexture` | `TSoftObjectPtr<UTexture2D>` | — | 物品图标 |

### 12.4 行数据（示例）

| RowName | Type | EffectValue | Duration | Price | UsableCombat | Retain | ObtainFrom | 备注 |
|---------|------|-------------|----------|-------|-------------|--------|------------|------|
| `hp_potion_small` | HPRecovery | 25.0（HP%） | 0 | 30 | ✅ | ❌ | — | `[待定]` |
| `hp_potion_large` | HPRecovery | 60.0（HP%） | 0 | 80 | ✅ | ❌ | — | `[待定]` |
| `smoke_vial_small` | SmokeRecovery | 2.0 | 0 | 50 | ✅ | ❌ | — | `[待定]` |
| `atk_buff_scroll` | TempBuff | 20.0（攻击+%） | 3 | 40 | ✅ | ❌ | — | `[待定]` |
| `def_buff_scroll` | TempBuff | 20.0（防御+%） | 3 | 40 | ✅ | ❌ | — | `[待定]` |
| `white_dmg_booster` | StatBoost | 5.0（白攻+） | 永久 | — | — | ✅ | `hunter_smoke` | §5.3.2 猎手之烟转化 |
| `blue_dmg_booster` | StatBoost | 5.0（蓝攻+） | 永久 | — | — | ✅ | `apprentice_smoke` | §5.3.2 学徒之烟转化 |
| `charge_speed_booster` | StatBoost | 1 | 永久 | — | — | ✅ | `adept_smoke` | §5.3.2 术者之烟转化 |
| `block_window_booster` | StatBoost | 0.03s | 永久 | — | — | ✅ | `border_smoke` | §5.3.2 边境之烟转化 |
| `healing_smoke_direct` | SmokeRecovery | 3.0 | 0 | — | ✅ | ❌ | `healing_smoke` | §5.3.2 治愈之烟直接回复 |

---

## 13. 11 — 货币/经济配置

### 13.1 基本信息

| 项 | 值 |
|----|-----|
| 资产名 | `DT_EconomyConfig` |
| 结构体 | `FEconomyConfigRow` |
| 行 ID | 经济参数类别 |
| 行数 | ~8 |
| 来源 | GDD §14 经济系统 |

### 13.2 列定义

| 列名 | 类型 | 默认值 | 备注 |
|------|------|--------|------|
| `DisplayName` | `FText` | — | 参数显示名 |
| `Category` | `FString` | — | 参数类别（"Exchange" / "Refine" / "Price" / "Drop"） |
| `Value` | `float` | 0.0 | 参数值 |
| `Description` | `FText` | — | 用途说明（给策划看） |

### 13.3 行数据

| RowName | Category | Value | GDD 来源 | 说明 |
|---------|----------|-------|----------|------|
| `healing_smoke_to_currency` | Exchange | 80.0 | §14.2 | 1个治愈之烟兑换货币量 |
| `refine_cost_base` | Refine | 100.0 | §14.3 | 博士提炼基础费用/次 |
| `refine_cost_advanced` | Refine | 200.0 | §14.3 | 博士提炼高级费用/次 |
| `price_common_equipment` | Price | 200.0 | §14.3 | 基础装备价格基数 |
| `price_rare_mask` | Price | 1000.0 | §14.3 | 稀有面具价格基数 |
| `price_consumable` | Price | 35.0 | §14.3 | 消耗品价格基数 |
| `currency_start` | Drop | 50.0 | §14 | `[待定]` 初始货币量 |
| `healing_smoke_per_loop` | Drop | 4.5 | §14.3 | `[待定]` 每轮回平均治愈之烟产出（3-6） |

---

## 14. 12 — 数据关联总览

### 14.1 外键引用关系

```
DT_CombatParams (单例，无外键)
     │
     ├── DT_CharacterConfig.defaults 引用战斗参数基值
     ├── DT_EnemyConfig 引用战斗参数基值
     │      └── DropSmokeType ──────► DT_SmokeConfig
     │
     ├── DT_WeaponConfig (独立，修改战斗参数)
     ├── DT_MaskConfig (独立，修改战斗参数)
     │
     ├── DT_SmokeConfig
     │      ├── ConvertedSkillID ────► DT_SkillConfig
     │      └── ConvertedItemID ────► DT_ConsumableConfig
     │
     ├── DT_SkillConfig
     │      └── ObtainFromSmokeType ► DT_SmokeConfig
     │
     ├── DT_SkillTreeConfig
     │      ├── ParentNodeID ───────► DT_SkillTreeConfig (自身)
     │      └── RequiredSmokeType ──► DT_SmokeConfig
     │
     ├── DT_AreaConfig (独立)
     ├── DT_ConsumableConfig
     │      └── ObtainFromSmokeType ─► DT_SmokeConfig
     │
     └── DT_EconomyConfig (单例，无外键)
```

### 14.2 GDD 章节 → 配置表映射

| GDD 章节 | 主要内容 | 对应配置表 |
|----------|----------|-----------|
| §5.1 移动系统 | 移动速度、操作按键 | DT_CombatParams (速度), DT_CharacterConfig (角色差异化) |
| §5.2.3-5.2.5 战斗 | 三色克制、蓄力、同色碰撞 | DT_CombatParams |
| §5.2.6 技能系统 | 技能类型 | DT_SkillConfig |
| §5.2.7 战斗参数表 | 初始数值 | DT_CombatParams |
| §5.3.2 烟分类体系 | 9种烟 | DT_SmokeConfig |
| §5.3.4 货币系统 | 货币名称/获取 | DT_EconomyConfig |
| §5.5 AI行为 | 敌人AI | DT_EnemyConfig (AIPreference, AIDifficulty, 范围) |
| §6.1 等级系统 | 无等级，烟驱动成长 | DT_SmokeConfig + DT_SkillTreeConfig |
| §6.2 局外技能树 | 被动增益节点 | DT_SkillTreeConfig |
| §6.3.1 武器 | 武器类型/效果 | DT_WeaponConfig |
| §6.3.2 面具 | 面具稀有度/效果 | DT_MaskConfig |
| §6.4 解锁节奏 | 轮回解锁内容 | DT_AreaConfig (区域), DT_CharacterConfig (角色) |
| §7.1 世界区域 | 6个区域 | DT_AreaConfig |
| §7.2 敌人类型 | 8种敌人 | DT_EnemyConfig |
| §7.3 物品/装备 | 装备槽位/消耗品 | DT_ConsumableConfig |
| §7.4 技能分类 | 5类技能 | DT_SkillConfig |
| §8.3 关键角色 | 6个角色 | DT_CharacterConfig |
| §14 经济系统 | 货币、产出/消耗 | DT_EconomyConfig |

### 14.3 `[待定]` 项清单

| 编号 | 位置 | 内容 | 影响范围 |
|------|------|------|----------|
| TBD-01 | DT_CharacterConfig | 无能力者角色姓名 | 角色配置 |
| TBD-02 | DT_CharacterConfig | 时间魔法师 HP | Boss 属性 |
| TBD-03 | DT_WeaponConfig | 第4-5种武器类型 | 武器系统 |
| TBD-04 | DT_MaskConfig | 更多面具条目（目标15-20） | 面具系统 |
| TBD-05 | DT_SkillConfig | 完整技能表（目标20-30） | 技能系统 |
| TBD-06 | DT_SkillTreeConfig | 最深传说节点效果 | 技能树 |
| TBD-07 | DT_ConsumableConfig | 回复药/增益道具的完整参数 | 消耗品 |
| ~~TBD-08~~ | DT_EconomyConfig | 货币名称 → **金币**（2026-08-01 定案） | 全局命名 |
| TBD-09 | DT_CombatParams | 耐力系统参数（奔跑消耗） | 移动系统 |
| TBD-10 | DT_CombatParams | 翻滚无敌帧数 | 探索模式 |

---

> 📝 **下一步行动：**
> 1. 策划确认所有 `[待定]` 项的值
> 2. 根据此规范创建 C++ 结构体（`DataTable/` 目录下的 .h 文件）
> 3. 在 UE 编辑器中创建 DataTable 资产并填入初始数据
> 4. `[PLAYTEST]` 标记项在原型阶段实际测试调优

---

> **修订记录：**
> | 版本 | 日期 | 修订内容 |
> |------|------|----------|
> | v0.1 | 2026-07-29 | 初始版本，覆盖 GDD §5-§8, §14 全部数值配置 |
> | v0.2 | 2026-08-01 | 数据落库与一致性修订：敌人移动列、面具倍率默认值（0.0→1.0）、表行数实况、枚举清单更新；11 张表全部生成并校验 |
> | v0.3 | 2026-08-01 | 解锁轮回口径修正：艾斯 UnlockRound 0、边境/地狱 UnlockRound 3（第0次轮回 = 首次游戏） |
> | v0.4 | 2026-08-01 | 战斗方案定案落库：新增暴击/闪避Buff/先制/金色攻击参数、艾斯每日烟回复、区域 bFirstLoopOnly、武器槽属性与公式改造、EffectType 字典；货币定名金币 |
