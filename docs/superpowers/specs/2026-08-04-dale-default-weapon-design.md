# Dale 默认武器（漂泊者短剑）— Design Spec

> **Status:** 方案已确认，等待用户复核本文档
> **Date:** 2026-08-04
> **Project:** Hole（洞穴）
> **Related:** GDD_Outline.md §6.3.1、DataTable_Spec.md §4/§6

## Overview

为 Dale（主角 `drifter`）配置一把默认单手剑，使现有数据链路
`DT_CharacterConfig.DefaultWeaponID → ABaseCharacter::InitializeAttributes → UInventoryComponent::EquipWeapon`
可以自动装备，战斗系统可直接读取武器修正，动画侧可按武器 `Category` 分支。

本次为纯数据 + 文档改动，不修改 C++。

## Design Decisions

### 1. 新增武器行 `dale_sword`

| 字段 | 值 | 说明 |
|------|----|------|
| RowName | `dale_sword` | 角色专属默认武器 |
| `DisplayName` | 漂泊者短剑 | 与主角身份一致 |
| `Description` | 在洞穴边缘拾获的旧短剑，刃口磨损却保养得当；战斗时才会拔出。 | 碎片叙事，同时解释“不随身携带可见武器”的美术设定 |
| `Category` | `Sword`（单手剑） | 动画选择的关键分支字段 |
| `MeshAsset` | `/Game/Stylized_Dark_Sword/Assets/SwordB/Meshes/SM_Sword_B.SM_Sword_B` | 用户指定 SwordB |
| `WeaponClass` | 空 | `AWeapon` 实例化尚未落地，留空 |
| `IconTexture` | 空 | 暂无图标资源 |
| `BlueAttackDamageScale` | 1.0 | 对齐 GDD 单手剑 |
| `WhiteAttackDamageScale` | 1.0 | 对齐 GDD 单手剑 |
| `BlockWindowBonus` | 0.1 | 对齐 GDD 单手剑（格挡窗口 +0.1s） |
| `Price` | 0 | 初始装备，不可购买 |
| 其余战斗字段 | 0 / false | `Blue/WhiteAttackDamageMod`、`DodgeWindowBonus`、`RedPenetrationScale`、`ExtraChargeTurns` |

### 2. 角色配置

`DT_CharacterConfig` 的 `drifter` 行设置 `DefaultWeaponID = dale_sword`；
`DefaultMaskID` 保持为空。其他角色（`ace` 等）本次不动，后续各自配置。

### 3. 数据流（沿用现有链路，无新增代码）

```
Scripts/create_datatables.py（唯一数据源）
  → UE 命令let 重建 /Game/DataTable/DT_WeaponConfig、DT_CharacterConfig
  → ABaseCharacter::InitializeAttributes() 读取 DefaultWeaponID
  → UInventoryComponent::EquipWeapon(dale_sword)
  → UCombatFormulaSubsystem::GetWeaponRow(dale_sword)
  → UAttributeComponent::AddModifier 注入武器修正
```

### 4. 动画选择

`ABP_Dale`（继承 `URoleAnimInstance`）后续通过
`EquippedWeaponID → UCombatFormulaSubsystem::GetWeaponRow → Category`
做动画分支（单手剑 / 大剑 / 锤子）。本次不新增动画实例变量。

## Files

| 文件 | 改动 |
|------|------|
| `Scripts/create_datatables.py` | 新增 `dale_sword` 行；`drifter` 补 `DefaultWeaponID`；文档字符串同步为 12 张表 |
| `DataTable_Spec.md` | §4.3 / §6.4 补行；版本 v0.6→v0.7；修正“关联策划案 GDD v0.3→v0.7” |
| `GDD_Outline.md` | §6.3.1 补“主角默认武器：漂泊者短剑（单手剑系）”说明；版本 v0.6→v0.7、日期 2026-08-04 |
| `DevLog.md` | 合并一条 2026-08-04 记录（策划/程序） |
| `/Game/DataTable/DT_WeaponConfig`、`DT_CharacterConfig` | 命令let 重建（资产随脚本更新） |
| 本文件 | 设计文档 |

## Verification

1. **重建前核对**：导出当前 `DT_BattleStage` CSV 与脚本值比对（工作区有未提交改动）；不一致则停下来问用户。
2. **重建**：运行 `Scripts/create_datatables.py` 命令let，再运行 `verify_datatables.py` / `export_datatables.py`。
3. **数据校验**：`DT_WeaponConfig` 5 行；`dale_sword.MeshAsset` 路径在导出 CSV 中保留且可解析；`drifter.DefaultWeaponID = dale_sword`。
4. **PIE（可选）**：生成 BP_Dale，确认 `EquippedWeaponID == dale_sword`，属性组件中存在武器修正（`WhiteAttackDamageScale` 等）。

## Risks / Notes

- 全表重建会删除并重建全部 12 张 DataTable，属项目既有流程；`DT_BattleStage` 若有用户未落盘的微调会丢失，因此必须先核对再重建。
- `Stylized_Dark_Sword` 资源包当前为 git 未跟踪状态；本次不提交资源包本身，但 DataTable 会引用它。若资源包缺失，软引用为空，运行时 `EquipWeapon` 仅打警告、不影响属性。
- 美术设定“漂泊者不随身携带可见武器”通过描述文本“战斗时才会拔出”解释，不冲突。

## Out of Scope

- 武器网格挂到角色手部 socket / 可见挂载组件
- 武器攻击动画资源与动画蓝图分支落地
- `AWeapon` 实例化、武器槽 UI、图标
- 其他角色的默认武器
