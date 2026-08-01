# Kami 项目记忆（AGENTS.md）

> 本文件是 Codex 在本仓库中的持久项目记忆，由原 Claude Code 项目记忆迁移而来。
> 更新规则：涉及项目约定、架构原则或开发流程的变更，先更新本文件，再同步到 DevLog。

## 1. 项目概览

- **引擎：** Unreal Engine 5.6，项目位于 `d:\UE5\UE_project\Kami`，UE 工程在 `Hole/` 子目录
- **游戏：** Hole（洞穴）— Roguelike 半回合制 RPG，红蓝白三色克制 + 实时格挡闪避 + 时间轮回
- **关键文档：**
  - `GDD_Outline.md` — 游戏策划案（v0.3，19 章节），设计和实现的参考基准
  - `DataTable_Spec.md` — 数据配置表规范（v0.1，11 张表），C++ USTRUCT 的定义依据
  - `DevLog.md` — 开发日志（时间线 + 关键决策 + 常见问题速查）
- **版本控制：** Git（分支 master），远程通过 GitHub MCP 管理；`Binaries/`、`Intermediate/`、`DerivedDataCache/`、`Saved/` 等生成目录不入库

## 2. 开发流程规范

### 文档约定
- `GDD_Outline.md`：设计变更时更新对应章节并同步版本号；`[待定]` = 待决策项、`[PLAYTEST]` = 需原型验证的参数，标记后不要强行确定
- `DataTable_Spec.md`：GDD 数值变更或新增系统时同步更新；策划数值落地时先查此文档再写 C++ USTRUCT
- `DevLog.md` 三类记录：时间线日志（日期 + 类别 + 问题 + 处理 + 经验）、关键决策表（⚡ 记录"当时为什么这么做"）、常见问题速查
  - 类别标签：`策划` `程序` `美术` `音频` `UE引擎` `项目管理` `Bug修复`
  - 同一对话/同一功能领域的改动合并到同一条日志，不逐条新建

### 操作约定
- **失败重试上限 3 次：** 任何任务（编译、代码修改、Bug 修复等）失败后最多重试 3 次，仍失败则立即中止并把完整报错发给用户决策；用户明确要求"一直重试直到成功"时除外
- **新建文件前先确认路径：** 创建任何新文件前，必须先向用户确认放置路径；修改现有文件不在此限
- **每次对话结束时：** 将重要信息整理回写本文件

## 3. 数据架构原则（所有实体统一遵守）

适用于角色、敌人、武器、面具、技能、消耗品、区域等**所有数据和配置表**。

### 三层分离（所有 DataTable 通用）
1. **USTRUCT (FTableRowBase)** — 纯数据 + 编辑器默认值；不可跨表查数据、不可写计算逻辑、不可 `PostLoad()`
2. **UGameInstanceSubsystem** — 跨表公式计算 / 多表数据合并（如 `UCombatFormulaSubsystem`、`UEconomySubsystem`，按领域拆分）
3. **运行时属性组件** — 存储从 DT 加载后的运行时状态（`UAttributeComponent` 玩家/敌人共用，`UInventoryComponent` 物品/装备）

判断标准：USTRUCT 里只能出现"这一行在 Excel 里这个格子的值是什么"；任何需要"看其他表"或"做运算"的逻辑都属于 Subsystem。

### 运行时属性：TMap + 修正器栈
所有可被 buff/debuff/装备/被动影响的属性统一使用 `UAttributeComponent`：

```cpp
TMap<FName, float> BaseAttributes;          // DT 加载的基值
TMap<FName, float> CachedFinalAttributes;   // 基值 + 所有 modifier 叠加后的最终值
TArray<FAttributeModifier> ActiveModifiers; // buff/debuff/装备/被动 统一栈

struct FAttributeModifier
{
    FName AttributeName;  // "MaxHP"、"WhiteAtkBonus"、"AIDifficulty" 等
    float Value;          // 修正值或修正倍率
    EModifierOp Op;       // Add / Multiply
    int32 RemainingTurns; // 0 = 永久（装备/面具/技能树被动/永久道具）
};
```

**不进入 TMap 的例外：** 高频变化的运行时状态（`CurrentHP` 等）、实体标识字段（`CharacterID`、`bIsPlayable`、`DisplayName`）、资产引用（`PortraitTexture`、`MeshAsset`）。

玩家和敌人共用 `UAttributeComponent`，仅初始化时读取不同 DataTable：
- `ABaseCharacter::InitializeAttributes()` → DT_CharacterConfig + DT_WeaponConfig + DT_MaskConfig
- `AEnemy::InitializeAttributes()` → DT_EnemyConfig

### 各实体数据流
```
DT_CharacterConfig / DT_WeaponConfig / DT_MaskConfig / DT_SkillTreeConfig
    → UCombatFormulaSubsystem → UAttributeComponent（玩家）
DT_EnemyConfig → UCombatFormulaSubsystem → UAttributeComponent（敌人）
DT_SmokeConfig → 掉落/转化系统（纯物品产出）
DT_SkillConfig → 技能系统（效果通过 AddModifier 体现，技能本身不改属性）
DT_EconomyConfig → UEconomySubsystem
DT_AreaConfig → 关卡管理（纯静态配置）
DT_ConsumableConfig → 背包系统 → 使用时 AddModifier
```

### 迭代规则
- 新增属性：DT 的 USTRUCT 加列 → AttributeNames 命名空间加常量 → 初始化时加载到 BaseAttributes（不改 Subsystem 接口、不改实体头文件）
- 新增 buff/debuff：一行 `AddModifier(AttributeName, Op, Value, Turns)`，不改任何头文件
- 新增实体类型（宠物/召唤物等）：创建 DT + FTableRowBase → 初始化时挂载 UAttributeComponent

## 4. 开发原则：模块化 + 性能优化优先

所有新代码必须主动满足：
1. **模块化优先：** 代码侧用组件模式、接口分离、单一职责；编辑器侧蓝图挂组件而非继承、DataTable 驱动而非硬编码。新功能优先考虑 `UActorComponent` 或 `UDataAsset`，避免基类膨胀
2. **性能优化优先：** 如 `ActorHasTag`（FName 索引比较 O(1)）替代 `Cast<>`（UHT 类型层级遍历 O(n)）；TMap 而非 `TArray::FindByKey`；缓存频繁访问的数据；避免 Tick 中重复计算；对象池复用

写任何新代码前先问：① 能否用组件/DataAsset 而非改基类？② 是否有更快的替代方案？

## 5. 编译环境

- UE 5.6 安装路径：`D:\Software\UnrealEngine\UE_5.6`
- 编译命令：
  ```
  D:\Software\UnrealEngine\UE_5.6\Engine\Build\BatchFiles\Build.bat HoleEditor Win64 Development "d:\UE5\UE_project\Kami\Hole\Hole.uproject"
  ```
- **修改任何 C++ 文件后立即编译验证**，尽早发现错误
