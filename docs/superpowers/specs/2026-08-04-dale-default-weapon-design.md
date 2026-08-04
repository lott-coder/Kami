# Dale 默认武器（漂泊者短剑）— Design Spec

> **Status:** 已确认（含 C++ 武器显示组件方案），实现中
> **Date:** 2026-08-04
> **Project:** Hole（洞穴）
> **Related:** GDD_Outline.md §6.3.1、DataTable_Spec.md §4/§6

## Overview

为 Dale（主角 `drifter`）配置一把默认单手剑，使现有数据链路
`DT_CharacterConfig.DefaultWeaponID → ABaseCharacter::InitializeAttributes → UInventoryComponent::EquipWeapon`
可以自动装备，战斗系统可直接读取武器修正，动画侧可按武器 `Category` 分支。

**主角设定变更：** 漂泊者可以随身携带武器——非战斗时背在背上，战斗时拔出（拔出/收回动作后续按动画设计接入）。当前里程碑只要求默认武器背在背上。

**执行分工：**
- 编辑器侧（DataTable 资产、BP_Dale 挂载）：由用户手动完成，本方案不运行表重建命令let。
- 仓库侧（脚本、文档）：由本方案同步，确保未来重建不会丢失数据。
- C++：新增 `UWeaponVisualComponent`（角色背上武器显示组件）+ `UInventoryComponent` 武器变更委托。

## Design Decisions

### 1. 新增武器行 `dale_sword`

| 字段 | 值 | 说明 |
|------|----|------|
| RowName | `dale_sword` | 角色专属默认武器 |
| `DisplayName` | 漂泊者短剑 | 与主角身份一致 |
| `Description` | 在洞穴边缘拾获的旧短剑，刃口磨损却保养得当；战斗时才会拔出。 | 碎片叙事，同时解释“不随身携带可见武器”的美术设定 |
| `Category` | `Sword`（单手剑） | 动画选择的关键分支字段 |
| `MeshAsset` | `/Game/Fab/Weapon/Stylized_Dark_Sword/Assets/SwordB/Meshes/SM_Sword_B.SM_Sword_B` | 用户指定 SwordB（资源已归入 Fab/Weapon） |
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
  → 用户手动编辑 /Game/DataTable/DT_WeaponConfig、DT_CharacterConfig（值与 Design Decisions 一致，脚本随后同步）
  → ABaseCharacter::InitializeAttributes() 读取 DefaultWeaponID
  → UInventoryComponent::EquipWeapon(dale_sword)
  → UCombatFormulaSubsystem::GetWeaponRow(dale_sword)
  → UAttributeComponent::AddModifier 注入武器修正
```

### 4. 动画选择

`ABP_Dale`（继承 `URoleAnimInstance`）后续通过
`EquippedWeaponID → UCombatFormulaSubsystem::GetWeaponRow → Category`
做动画分支（单手剑 / 大剑 / 锤子）。本次不新增动画实例变量。

### 5. 武器可见挂载（当前里程碑）

- 非战斗：`SM_Sword_B` 静态网格挂载在 Dale 背上（骨骼 socket），随骨骼动画运动。
- 战斗拔出 / 收回：后续按动画设计接入，当前只做“背在背上”这一状态。
- 挂载组件命名建议 `WeaponMesh`（便于后续战斗系统/动画引用做拔出/收回切换）。

### 6. C++ 组件方案（已确认）

新增 `UWeaponVisualComponent`（`USceneComponent` 子类，挂所有角色通用）：

- 内部持有 `UStaticMeshComponent WeaponMesh`（No Collision），作为 `ABaseCharacter` 的默认子对象创建，所有 Role 自动获得，无需在 BP 手动加组件。
- `BackSocketName` 可配置（默认 `weapon_back`）；socket 不存在时回退挂到网格根节点 + `BackAttachOffset`，并打警告。
- `BeginPlay` 时绑定 `UInventoryComponent::OnWeaponChanged`，读取 `EquippedWeaponID → GetWeaponRow() → MeshAsset`，软加载后设置网格；无武器则隐藏。
- 提供 `SetWeaponVisible(bool)`，供后续战斗系统做拔出/收回；当前阶段始终显示背上状态。
- 实现要点（Bug 修复 2026-08-04）：`WeaponMesh` 必须在 `ABaseCharacter` 构造器中以 Actor 级默认子对象创建并 `SetupAttachment(WeaponVisualComponent)`；不能在组件构造器中用嵌套默认子对象创建，否则实例化时报 `Template Mismatch` 且网格停留在世界原点。

`UInventoryComponent` 增加 `OnWeaponChanged(FName WeaponID)` 动态多播委托，在 `EquipWeapon/UnequipWeapon` 后广播（卸下时 `WeaponID = NAME_None`）。

与 `AWeapon`（`Weapon.h`）的关系：`AWeapon` 是武器 Actor 占位（对应 `WeaponClass`），本组件直接使用 `MeshAsset` 做角色可见表现，两者共用同一行数据但职责分离，不冲突。

### 7. 入场拔刀通知（已确认）

- 新增 `UAnimNotify_Hold`，按 Montage 类别归档于 `Animation/AnimNotifies/Entrance/`（不按具体 Montage 命名；后续其他 Montage 类别各自建目录）。
- 触发时调用 `UWeaponVisualComponent::AttachWeaponToSocket(HandSocketName)`（默认 `weapon_hand_r`），把武器从背上转移到手部。
- `UWeaponVisualComponent` 新增 `HandSocketName`、`AttachWeaponToSocket(FName)`，缓存网格引用；`BeginPlay` 背部挂载复用同一方法。
- `UWeaponVisualComponent` 新增 `bWeaponDrawn` / `IsWeaponDrawn()`：挂到 `weapon_hand_r` 时置 true、挂回背上时置 false，供 ABP 做拔剑后 Idle/移动变体切换。
- `UBattleComponent` 新增 `PlayerEntrySectionName`（默认 `Draw`），`PlayAnimMontage` 从该 section 开始，支持 `Draw_A_Great_Sword_1 → _2` 完整播放；Montage 内两段序列需在同一 section 内或通过 Next Section 串联。
- 流程定名：Boss 开场动画为**剧情动画**；玩家入场动画在战斗开始（Boss 剧情结束、战斗 HUD 已出现）后播放，`UBattleComponent` 现有 `PlayerEntryMontage` 时序不变。

### 8. 编辑器操作（用户执行）

1. 重启编辑器加载新 DLL。
2. 打开 `MTG_DrawGreatSword`，把现有 `Hold` 通知右键 **Change to → AnimNotify_Hold**（保留名称与位置）。
3. 通知放在剑到达手部的帧；如手型不对，调骨骼 `weapon_hand_r` socket 朝向。
4. PIE：Boss 剧情动画结束 → HUD 出现 → 入场 Montage → `Hold` 触发时武器从背转移到手。

## Files

| 文件 | 改动 |
|------|------|
| `Scripts/create_datatables.py` | 新增 `dale_sword` 行；`drifter` 补 `DefaultWeaponID`；文档字符串同步为 12 张表（用户确认编辑器操作后同步） |
| `DataTable_Spec.md` | §4.3 / §6.4 补行；版本 v0.6→v0.7；修正“关联策划案 GDD v0.3→v0.7” |
| `GDD_Outline.md` | §6.3.1 补“主角默认武器：漂泊者短剑（单手剑系）”与“随身携带、非战斗背在背上”说明；版本 v0.6→v0.7、日期 2026-08-04 |
| `docs/DesignDocs/Protagonist_Character_Design.md` | v2.0“不携带可见武器”改为“武器背在背上（战斗时拔出）”，补武器外观段落 |
| `DevLog.md` | 合并一条 2026-08-04 记录（策划/程序） |
| `/Game/DataTable/DT_WeaponConfig`、`DT_CharacterConfig` | 用户手动编辑（不运行命令let） |
| `Hole/Source/Hole/Public/Component/WeaponVisualComponent.h` + `Private/Component/WeaponVisualComponent.cpp` | 新增：背上武器显示组件 |
| `Hole/Source/Hole/Public/Animation/AnimNotifies/Entrance/AnimNotify_Hold.h` + `Private/Animation/AnimNotifies/Entrance/AnimNotify_Hold.cpp` | 新增：入场 Montage 拔刀通知 |
| `Hole/Source/Hole/Public/Component/InventoryComponent.h` + `Private/Component/InventoryComponent.cpp` | 新增 `OnWeaponChanged` 委托并广播 |
| `Hole/Source/Hole/Public/Character/BaseCharacter.h` + `Private/Character/BaseCharacter.cpp` | 新增默认子对象 `WeaponVisualComponent` |
| 本文件 | 设计文档 |

## 编辑器操作流程（用户执行）

1. **DT_WeaponConfig**：新增行 `dale_sword`，按“Design Decisions”填值，`MeshAsset` 选 `SM_Sword_B`，保存。
2. **DT_CharacterConfig**：`drifter` 行 `DefaultWeaponID = dale_sword`，保存。
3. **骨骼 socket**：在 CoreC 骨骼中新建背部 socket `weapon_back`（`spine_03` 附近，方向朝后/斜背），保存。
4. **BP_Dale**：编译 C++ 后 `WeaponVisualComponent` 自动存在；在其子对象 `WeaponMesh` 上微调相对 Transform 使其贴背（若 socket 姿态已调好可跳过）。
5. **PIE 验证**：角色背上可见武器且跟随骨骼；`InventoryComponent->GetEquippedWeaponID() == dale_sword`；武器修正已注入（格挡窗口 +0.1s 等）。

## Risks / Notes

- 本方案不运行表重建命令let，避免覆盖用户手动编辑与 `DT_BattleStage` 未提交改动。
- 手动填表后必须同步 `Scripts/create_datatables.py`，否则未来重建会丢行。
- `Stylized_Dark_Sword` 资源包当前为 git 未跟踪状态；本次不提交资源包本身，但 DataTable 会引用它。若资源包缺失，软引用为空，运行时 `EquipWeapon` 仅打警告、不影响属性。
- 主角美术设定已从“不携带可见武器”改为“背在背上、战斗时拔出”，需同步 `Protagonist_Character_Design.md`。

## Out of Scope

- 战斗时拔出/收回的动作动画与逻辑（后续按动画设计接入）
- 武器攻击动画资源与动画蓝图分支落地
- `AWeapon` 实例化、武器槽 UI、图标
- 其他角色的默认武器
