# Combat Animations Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为战斗系统 v1 接入动画表驱动播放：新增 13 号表 `DT_CombatAnimConfig`，战斗组件按行动/结算/碰撞结果播放玩家与敌人 Montage，并支持玩家碰撞准备姿态与格挡成功→金色反击连播。

**Architecture:** 沿用三层架构：`FCombatAnimRow`/`FAnimRef`（USTRUCT 纯数据）→ `UCombatFormulaSubsystem::GetCombatAnimRow()`（读表）→ `UBattleComponent`（播放编排）。玩家碰撞准备姿态通过 `UBaseCharacterAnimInstance::bClashReady` 状态变量驱动 ABP 切换，镜像现有 `bWeaponDrawn` 模式。

**Tech Stack:** Unreal Engine 5.6 / C++（Hole 模块）、Python 脚本（`Scripts/create_datatables.py` 等）、UE DataTable CSV 格式。

## Global Constraints

（以下约束隐含于每个任务，执行时不重复列出）

- **构建命令（每次改 C++ 后立即编译）：**
  ```
  D:\Software\UnrealEngine\UE_5.6\Engine\Build\BatchFiles\Build.bat HoleEditor Win64 Development "d:\UE5\UE_project\Kami\Hole\Hole.uproject"
  ```
  期望输出：0 error / 0 warning（既有 warning 可忽略，不得新增）。
- **重试上限 3 次：** 编译/操作失败最多重试 3 次；仍失败立即停止并把完整错误发给用户决策。
- **三层架构：** USTRUCT 只存"表格单元格的值"，禁止跨表查找/计算/`PostLoad()`；读表走 Subsystem；运行时播放/回落逻辑走组件。
- **碰撞前摇对齐：** 敌方碰撞动画须在 `ClashTelegraphTime`（0.8s）内完成前摇，由 Montage 资产侧保证，表内不配时间。
- **回落约定：** `BlockFail`/`DodgeFail`/`ChargeInterrupted` 的 Montage 为空时播 `Hurt`；格挡成功先播 `BlockSuccess`（弹反）播完再接 `GoldCounter`。
- **动画通知约定：** 新 AnimNotify 类按类别归档 `Animation/AnimNotifies/<类别>/`，不按 Montage 命名。
- **DataTable CSV 资产引用格式：** `/Game/路径/资产名.资产名`（如 `.../MTG_DrawGreatSword.MTG_DrawGreatSword`）。
- **编辑器资产操作由用户手动完成**（C++/脚本由执行者完成）；**不运行 `create_datatables.py` 重建命令**（会覆盖用户手动编辑的其他表），脚本只作未来单一数据源同步。
- **测试方式：** 本项目无自动化测试框架；每个任务以"编译通过 + 日志/编辑器 PIE 验证清单"作为测试闭环。
- **提交规范：** 每任务独立提交；message 前缀 `feat(combat-anim):` / `docs(combat-anim):`；提交需写入 `.git`（沙箱默认禁止，需申请权限）。
- **文档语言：** 用户侧文档（DataTable_Spec/GDD/DevLog）中文；AGENTS.md 英文。

## File Structure

| 文件 | 责任 | 动作 |
|---|---|---|
| `Hole/Source/Hole/Public/DataTable/CombatAnimConfigTable.h` | `FAnimRef` + `FCombatAnimRow`（13 号表行结构） | 新建（Task 1） |
| `Hole/Source/Hole/Public/Subsystem/CombatFormulaSubsystem.h` | `GetCombatAnimRow()` 声明 + 表缓存成员 | 修改（Task 2） |
| `Hole/Source/Hole/Private/Subsystem/CombatFormulaSubsystem.cpp` | `GetCombatAnimRow()` 实现 | 修改（Task 2） |
| `Hole/Source/Hole/Public/Animation/BaseCharacterAnimInstance.h` | `bClashReady` + `SetClashReady()` | 修改（Task 3） |
| `Hole/Source/Hole/Private/Animation/BaseCharacterAnimInstance.cpp` | `SetClashReady()` 定义 | 修改（Task 3） |
| `Hole/Source/Hole/Public/Combat/BattleComponent.h` | 动画播放辅助方法/状态声明 | 修改（Task 4） |
| `Hole/Source/Hole/Private/Combat/BattleComponent.cpp` | 读表、播放、碰撞/结算/结局接入 | 修改（Task 4-7） |
| `Scripts/create_datatables.py` | 13 号表头与行数据（仅同步，不运行） | 修改（Task 8） |
| `Scripts/verify_datatables.py` | 表清单 + 抽查 | 修改（Task 8） |
| `Scripts/export_datatables.py` | 表清单 | 修改（Task 8） |
| 编辑器资产 | `DT_CombatAnimConfig`、占位动画、ABP_Dale `ClashReady` 状态 | 用户手动（Task 9） |
| `DevLog.md` / `AGENTS.md` / `GDD_Outline.md` | 记录与约定同步 | 修改（Task 10） |

---

### Task 1: FAnimRef + FCombatAnimRow USTRUCT

**Files:**
- Create: `Hole/Source/Hole/Public/DataTable/CombatAnimConfigTable.h`

**Interfaces:**
- Produces: `struct HOLE_API FAnimRef`（`Montage` / `SectionName` / `PlayRate` / `BlendOutTime` / `IsValidRef()`）、`struct HOLE_API FCombatAnimRow : FTableRowBase`（19 个 `FAnimRef` 动作列 + `DisplayName`）。

- [ ] **Step 1: 创建头文件**

```cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Animation/AnimMontage.h"
#include "CombatAnimConfigTable.generated.h"

/** 单个战斗动画引用：Montage 软引用 + 可选 Section + 播放参数（纯数据） */
USTRUCT(BlueprintType)
struct HOLE_API FAnimRef
{
	GENERATED_BODY()

	/** Montage 软引用；空 = 不播放或按回落约定（BlockFail/DodgeFail/ChargeInterrupted 回落 Hurt） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimRef")
	TSoftObjectPtr<UAnimMontage> Montage;

	/** 起始 Section；NAME_None = 从头播放 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimRef")
	FName SectionName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimRef")
	float PlayRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimRef")
	float BlendOutTime = 0.25f;

	bool IsValidRef() const { return !Montage.IsNull(); }
};

/**
 * FCombatAnimRow - DT_CombatAnimConfig 行结构（13 号表）
 * 一行 = 一个战斗实体（v1：drifter / satan）。
 * 纯数据：不跨表查找、不含运行逻辑。
 * @see DataTable_Spec.md §15 战斗动画配置
 */
USTRUCT(BlueprintType)
struct HOLE_API FCombatAnimRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Action")
	FAnimRef Entry;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Action")
	FAnimRef Sheathe;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Action")
	FAnimRef RedDefense;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Action")
	FAnimRef GoldCounter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Action")
	FAnimRef BlueAttack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Action")
	FAnimRef WhiteAttack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Action")
	FAnimRef Charge;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Action")
	FAnimRef ChargeInterrupted;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Reaction")
	FAnimRef Hurt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Reaction")
	FAnimRef BlockSuccess;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Reaction")
	FAnimRef BlockFail;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Reaction")
	FAnimRef DodgeSuccess;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Reaction")
	FAnimRef DodgeFail;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Reaction")
	FAnimRef Death;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Reaction")
	FAnimRef Victory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Action")
	FAnimRef Skill;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Clash")
	FAnimRef ClashReady;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Clash")
	FAnimRef ClashTelegraphBlue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatAnim|Clash")
	FAnimRef ClashTelegraphWhite;
};
```

- [ ] **Step 2: 编译**

Run: 构建命令（见 Global Constraints）
Expected: 0 error / 0 warning（新增）。

- [ ] **Step 3: 提交**

```bash
git add Hole/Source/Hole/Public/DataTable/CombatAnimConfigTable.h
git commit -m "feat(combat-anim): add FAnimRef and FCombatAnimRow for 13th anim table"
```

---

### Task 2: Subsystem 读表接口 GetCombatAnimRow

**Files:**
- Modify: `Hole/Source/Hole/Public/Subsystem/CombatFormulaSubsystem.h`
- Modify: `Hole/Source/Hole/Private/Subsystem/CombatFormulaSubsystem.cpp`

**Interfaces:**
- Consumes: `FCombatAnimRow`（Task 1）。
- Produces: `const FCombatAnimRow* UCombatFormulaSubsystem::GetCombatAnimRow(FName EntityID) const`（找不到返回 nullptr；懒加载并缓存 `/Game/DataTable/DT_CombatAnimConfig`）。

- [ ] **Step 1: 头文件增加前向声明、接口与缓存**

在 `CombatFormulaSubsystem.h` 的结构体前向声明区（`struct FCombatStageRow;` 附近）加：

```cpp
struct FCombatAnimRow;
```

在 `// ---- 行查询（单表）----` 区（`GetWeaponRow` 声明后）加：

```cpp
	/** DT_CombatAnimConfig 行（找不到返回 nullptr） */
	const FCombatAnimRow* GetCombatAnimRow(FName EntityID) const;
```

在 private 缓存成员区（`mutable TObjectPtr<UDataTable> WeaponTable;` 后）加：

```cpp
	mutable TObjectPtr<UDataTable> CombatAnimTable;
```

- [ ] **Step 2: cpp 增加 include 与实现**

在 `CombatFormulaSubsystem.cpp` 的 include 区（`#include "DataTable/CombatStageTable.h"` 后）加：

```cpp
#include "DataTable/CombatAnimConfigTable.h"
```

在 `GetWeaponRow` 实现后加：

```cpp
const FCombatAnimRow* UCombatFormulaSubsystem::GetCombatAnimRow(FName EntityID) const
{
	UDataTable* Table = GetTable(CombatAnimTable, TEXT("/Game/DataTable/DT_CombatAnimConfig"));
	return Table ? Table->FindRow<FCombatAnimRow>(EntityID, TEXT("CombatFormula::CombatAnim")) : nullptr;
}
```

- [ ] **Step 3: 编译**

Run: 构建命令
Expected: 0 error / 0 warning（新增）。

- [ ] **Step 4: 提交**

```bash
git add Hole/Source/Hole/Public/Subsystem/CombatFormulaSubsystem.h Hole/Source/Hole/Private/Subsystem/CombatFormulaSubsystem.cpp
git commit -m "feat(combat-anim): expose GetCombatAnimRow on combat formula subsystem"
```

---

### Task 3: AnimInstance 暴露 bClashReady

**Files:**
- Modify: `Hole/Source/Hole/Public/Animation/BaseCharacterAnimInstance.h`
- Modify: `Hole/Source/Hole/Private/Animation/BaseCharacterAnimInstance.cpp`

**Interfaces:**
- Produces: `bool bClashReady`（BlueprintReadOnly）、`void SetClashReady(bool bReady)`（BlueprintCallable）；ABP_Dale 读取 `bClashReady` 切换碰撞准备状态（Task 9 编辑器步骤）。

- [ ] **Step 1: 头文件声明**

在 `bWeaponDrawn` 属性后加：

```cpp
	/** 同色碰撞准备阶段：由战斗组件置位，ABP 切换到 ClashReady 姿态，结束后复位 */
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	bool bClashReady = false;

	/** 设置 bClashReady（事件驱动，仅战斗组件在 Clash 阶段调用） */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void SetClashReady(bool bReady);
```

- [ ] **Step 2: cpp 定义**

在 `BaseCharacterAnimInstance.cpp` 的 `NativeInitializeAnimation` 前加：

```cpp
void UBaseCharacterAnimInstance::SetClashReady(bool bReady)
{
	bClashReady = bReady;
}
```

- [ ] **Step 3: 编译**

Run: 构建命令
Expected: 0 error / 0 warning（新增）。

- [ ] **Step 4: 提交**

```bash
git add Hole/Source/Hole/Public/Animation/BaseCharacterAnimInstance.h Hole/Source/Hole/Private/Animation/BaseCharacterAnimInstance.cpp
git commit -m "feat(combat-anim): expose clash ready state on base anim instance"
```

---

### Task 4: BattleComponent 动画播放辅助 + 普通回合动画

**Files:**
- Modify: `Hole/Source/Hole/Public/Combat/BattleComponent.h`
- Modify: `Hole/Source/Hole/Private/Combat/BattleComponent.cpp`

**Interfaces:**
- Consumes: `GetCombatAnimRow()`（Task 2）、`FCombatAnimRow`/`FAnimRef`（Task 1）、`SetClashReady()`（Task 3）。
- Produces: `const FCombatAnimRow* GetCombatAnimRow(bool bPlayer) const`、`void PlayCombatAnim(ABaseCharacter*, const FAnimRef&)`、`void PlayActionAnim(bool bPlayer, EBattleAction Action)`、`void PlayResolutionAnimations(const FTurnResolution& Resolution)`；`ApplyResolution` 非碰撞分支调用播放编排。

- [ ] **Step 1: 头文件增加声明**

在 `BattleComponent.h` 的前向声明区（`struct FCombatStageRow;` 不存在则加在 `class UAnimMontage;` 附近）加：

```cpp
struct FCombatAnimRow;
struct FAnimRef;
```

在 `// ==================== 数值辅助（只调子系统） ====================`（`BattleComponent.h` 第 198 行附近）区块前加新区块：

```cpp
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
```

在 private 状态区（`bool bClashStarted = false;` 附近）加：

```cpp
	/** 格挡成功弹反 Montage 播完后待接金色反击 */
	bool bBlockSuccessChainPending = false;
```

- [ ] **Step 2: cpp 增加 include**

在 `BattleComponent.cpp` include 区加：

```cpp
#include "Animation/BaseCharacterAnimInstance.h"
#include "DataTable/CombatAnimConfigTable.h"
```

- [ ] **Step 3: 实现辅助方法**

在 `SheathePlayerWeapon()` 定义前插入以下实现：

```cpp
const FCombatAnimRow* UBattleComponent::GetCombatAnimRow(bool bPlayer) const
{
	UCombatFormulaSubsystem* Subsystem = GetCombatSubsystem();
	if (!Subsystem)
	{
		return nullptr;
	}
	const FName EntityID = bPlayer
		? (PlayerRole.IsValid() ? PlayerRole->CharacterID : NAME_None)
		: (BossEnemy.IsValid() ? BossEnemy->EnemyID : NAME_None);
	return EntityID.IsNone() ? nullptr : Subsystem->GetCombatAnimRow(EntityID);
}

void UBattleComponent::PlayCombatAnim(ABaseCharacter* Character, const FAnimRef& AnimRef)
{
	if (!Character || AnimRef.Montage.IsNull())
	{
		return;
	}
	UAnimMontage* Montage = AnimRef.Montage.LoadSynchronous();
	if (!Montage)
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::PlayCombatAnim - 无法加载 Montage: %s"), *AnimRef.Montage.ToString());
		return;
	}
	Character->PlayAnimMontage(Montage, AnimRef.PlayRate, AnimRef.SectionName);
	UE_LOG(LogTemp, Log, TEXT("UBattleComponent::PlayCombatAnim - %s 播放 %s"), *GetNameSafe(Character), *Montage->GetName());
}

void UBattleComponent::PlayActionAnim(bool bPlayer, EBattleAction Action)
{
	const FCombatAnimRow* Row = GetCombatAnimRow(bPlayer);
	if (!Row)
	{
		return;
	}
	const FAnimRef* Ref = nullptr;
	switch (Action)
	{
	case EBattleAction::RedDefense: Ref = &Row->RedDefense; break;
	case EBattleAction::BlueAttack: Ref = &Row->BlueAttack; break;
	case EBattleAction::WhiteAttack: Ref = &Row->WhiteAttack; break;
	case EBattleAction::Charge: Ref = &Row->Charge; break;
	default: return;
	}
	PlayCombatAnim(bPlayer ? PlayerRole.Get() : BossEnemy.Get(), *Ref);
}

void UBattleComponent::PlayResolutionAnimations(const FTurnResolution& Resolution)
{
	if (!PlayerRole.IsValid() || !BossEnemy.IsValid())
	{
		return;
	}

	const EBattleAction PlayerAction = PlayerLastAction;
	const EBattleAction EnemyAction = EnemyChosenAction;

	// 玩家侧：受击 > 金色反击 > 蓄力被打断 > 行动动画
	if (Resolution.PlayerDamageTaken > 0.0f)
	{
		if (const FCombatAnimRow* Row = GetCombatAnimRow(true))
		{
			PlayCombatAnim(PlayerRole.Get(), Row->Hurt);
		}
	}
	else if (PlayerAction == EBattleAction::RedDefense && EnemyAction == EBattleAction::BlueAttack)
	{
		if (const FCombatAnimRow* Row = GetCombatAnimRow(true))
		{
			PlayCombatAnim(PlayerRole.Get(), Row->GoldCounter);
		}
	}
	else if (Resolution.bPlayerChargeInterrupted)
	{
		if (const FCombatAnimRow* Row = GetCombatAnimRow(true))
		{
			PlayCombatAnim(PlayerRole.Get(), Row->ChargeInterrupted);
		}
	}
	else
	{
		PlayActionAnim(true, PlayerAction);
	}

	// 敌人侧：受击 > 金色反击 > 蓄力被打断 > 行动动画
	if (Resolution.EnemyDamageTaken > 0.0f)
	{
		if (const FCombatAnimRow* Row = GetCombatAnimRow(false))
		{
			PlayCombatAnim(BossEnemy.Get(), Row->Hurt);
		}
	}
	else if (EnemyAction == EBattleAction::RedDefense && PlayerAction == EBattleAction::BlueAttack)
	{
		if (const FCombatAnimRow* Row = GetCombatAnimRow(false))
		{
			PlayCombatAnim(BossEnemy.Get(), Row->GoldCounter);
		}
	}
	else if (Resolution.bEnemyChargeInterrupted)
	{
		if (const FCombatAnimRow* Row = GetCombatAnimRow(false))
		{
			PlayCombatAnim(BossEnemy.Get(), Row->ChargeInterrupted);
		}
	}
	else
	{
		PlayActionAnim(false, EnemyAction);
	}
}

void UBattleComponent::SetPlayerClashReady(bool bReady)
{
	if (!PlayerRole.IsValid())
	{
		return;
	}
	if (UAnimInstance* AnimInstance = PlayerRole->GetMesh()->GetAnimInstance())
	{
		if (UBaseCharacterAnimInstance* BaseAnim = Cast<UBaseCharacterAnimInstance>(AnimInstance))
		{
			BaseAnim->SetClashReady(bReady);
		}
	}
}
```

- [ ] **Step 4: 在 ApplyResolution 非碰撞分支末尾接入动画编排**

在 `ApplyResolution` 中，两个 `ApplyDamageTo` 之后、函数结束大括号之前插入：

```cpp
	// 战斗动画（v1 与回合推进解耦：播放后不阻塞 EndTurnAndAdvance）
	if (Phase != EBattlePhase::Ended)
	{
		PlayResolutionAnimations(Resolution);
	}
```

- [ ] **Step 5: 编译**

Run: 构建命令
Expected: 0 error / 0 warning（新增）。

- [ ] **Step 6: PIE 冒烟验证（用户可后补，代码先行）**

说明：此步依赖 Task 9 的表与占位资产；若资产未就绪，先验证"日志不报错、空引用静默跳过"。就绪后：PIE → Boss 开场 → 玩家选白攻且敌人白攻 → 双方 `PlayCombatAnim` 日志出现。

- [ ] **Step 7: 提交**

```bash
git add Hole/Source/Hole/Public/Combat/BattleComponent.h Hole/Source/Hole/Private/Combat/BattleComponent.cpp
git commit -m "feat(combat-anim): play action/reaction montages from anim table on turn resolution"
```

---

### Task 5: 入场 Montage 改读表 + 收刀清理

**Files:**
- Modify: `Hole/Source/Hole/Private/Combat/BattleComponent.cpp`

**Interfaces:**
- Consumes: `GetCombatAnimRow()`、`FCombatAnimRow::Entry`（Task 1/2）。
- Produces: `EnterBattle()` 优先读表 `Entry`（BP 字段 `PlayerEntryMontage`/`PlayerEntrySectionName` 作回退）；`SheathePlayerWeapon()` 停止所有 Montage 并复位碰撞状态。

- [ ] **Step 1: 替换 EnterBattle 的入场 Montage 逻辑**

将 `EnterBattle()` 中现有入口播放块：

```cpp
	if (PlayerEntryMontage && PlayerRole.IsValid())
	{
		const float PlayLength = PlayerEntryMontage->GetPlayLength();
		PlayerRole->PlayAnimMontage(PlayerEntryMontage, 1.0f, PlayerEntrySectionName);
```

替换为：

```cpp
	// 入场动画优先读 DT_CombatAnimConfig.Entry，BP 字段回退
	UAnimMontage* EntryMontage = PlayerEntryMontage;
	FName EntrySection = PlayerEntrySectionName;
	float EntryPlayRate = 1.0f;
	if (const FCombatAnimRow* AnimRow = GetCombatAnimRow(true))
	{
		if (!AnimRow->Entry.Montage.IsNull())
		{
			EntryMontage = AnimRow->Entry.Montage.LoadSynchronous();
			EntrySection = AnimRow->Entry.SectionName;
			EntryPlayRate = AnimRow->Entry.PlayRate;
		}
	}
	if (EntryMontage && PlayerRole.IsValid())
	{
		const float PlayLength = EntryMontage->GetPlayLength();
		PlayerRole->PlayAnimMontage(EntryMontage, EntryPlayRate, EntrySection);
```

（`if (PlayLength > 0.0f) { ... StartNewRound ... }` 及后续不变。）

- [ ] **Step 2: 收刀改为停止全部 Montage 并复位碰撞状态**

将 `SheathePlayerWeapon()` 中停止入口 Montage 的代码：

```cpp
		// 停止入场拔刀 Montage，避免其通知回调再次把武器拿回手上
		if (PlayerEntryMontage)
		{
			PlayerRole->StopAnimMontage(PlayerEntryMontage);
		}
```

替换为：

```cpp
		// 停止全部 Montage（入场/动作/碰撞残留），避免通知回调再次拔刀或连播反击
		if (UAnimInstance* AnimInstance = PlayerRole->GetMesh()->GetAnimInstance())
		{
			AnimInstance->StopAllMontages(0.0f);
			AnimInstance->OnMontageEnded.RemoveDynamic(this, &UBattleComponent::OnBlockSuccessMontageEnded);
		}
		SetPlayerClashReady(false);
		bBlockSuccessChainPending = false;
```

（`RemoveDynamic` 指向 Task 6 才会声明的回调，本任务先编译会报错——因此本任务与 Task 6 必须连续执行：见下方提示。）

> **执行提示：** `OnBlockSuccessMontageEnded` 与 `bBlockSuccessChainPending` 在 Task 6 定义。若按本计划顺序执行，请将 Task 5 与 Task 6 的代码改动合并到同一编译批次，最后统一编译一次再提交（提交可仍分开：先提交 Task 5 前的文件快照不现实，改为 Task 5+6 合并为一次提交，message 覆盖两个变更）。

- [ ] **Step 3: 编译（与 Task 6 合并后统一编译）**

- [ ] **Step 4: 提交（与 Task 6 合并提交，message 见 Task 6 Step 5）**

---

### Task 6: 同色碰撞动画（前摇/准备姿态/格挡闪避/反击连播）

**Files:**
- Modify: `Hole/Source/Hole/Public/Combat/BattleComponent.h`
- Modify: `Hole/Source/Hole/Private/Combat/BattleComponent.cpp`

**Interfaces:**
- Consumes: Task 3 的 `SetClashReady()`、Task 4 的播放辅助。
- Produces: `void PlayBlockSuccessChain()`、`void PlayClashFailReaction(EClashResult Result)`、`UFUNCTION() void OnBlockSuccessMontageEnded(UAnimMontage*, bool)`；`StartClash` 播敌方前摇 + 玩家准备；`ResolveClash` 复位准备并按结果播放。

- [ ] **Step 1: 头文件增加声明**

在 Task 4 新增的"战斗动画"区块末尾追加：

```cpp
	/** 格挡成功：先播 BlockSuccess（弹反），播完回调接 GoldCounter */
	void PlayBlockSuccessChain();

	/** 格挡/闪避失败：优先对应 Fail 动画，空则回落 Hurt */
	void PlayClashFailReaction(EClashResult Result);

	/** BlockSuccess 播完回调：接金色反击 */
	UFUNCTION()
	void OnBlockSuccessMontageEnded(UAnimMontage* Montage, bool bInterrupted);
```

- [ ] **Step 2: cpp 实现连播与失败回落**

在 `SetPlayerClashReady` 实现后追加：

```cpp
void UBattleComponent::PlayBlockSuccessChain()
{
	const FCombatAnimRow* Row = GetCombatAnimRow(true);
	if (!Row)
	{
		return;
	}
	if (Row->BlockSuccess.Montage.IsNull())
	{
		// 无弹反动作时直接尝试金色反击
		PlayCombatAnim(PlayerRole.Get(), Row->GoldCounter);
		return;
	}
	if (!PlayerRole.IsValid())
	{
		return;
	}
	bBlockSuccessChainPending = true;
	if (UAnimInstance* AnimInstance = PlayerRole->GetMesh()->GetAnimInstance())
	{
		AnimInstance->OnMontageEnded.AddDynamic(this, &UBattleComponent::OnBlockSuccessMontageEnded);
	}
	PlayCombatAnim(PlayerRole.Get(), Row->BlockSuccess);
}

void UBattleComponent::PlayClashFailReaction(EClashResult Result)
{
	const FCombatAnimRow* Row = GetCombatAnimRow(true);
	if (!Row)
	{
		return;
	}
	const FAnimRef& Ref = (Result == EClashResult::DodgeFail) ? Row->DodgeFail : Row->BlockFail;
	PlayCombatAnim(PlayerRole.Get(), Ref.Montage.IsNull() ? Row->Hurt : Ref);
}

void UBattleComponent::OnBlockSuccessMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!bBlockSuccessChainPending)
	{
		return;
	}
	bBlockSuccessChainPending = false;
	if (PlayerRole.IsValid())
	{
		if (UAnimInstance* AnimInstance = PlayerRole->GetMesh()->GetAnimInstance())
		{
			AnimInstance->OnMontageEnded.RemoveDynamic(this, &UBattleComponent::OnBlockSuccessMontageEnded);
		}
	}
	if (bInterrupted)
	{
		return;
	}
	if (const FCombatAnimRow* Row = GetCombatAnimRow(true))
	{
		PlayCombatAnim(PlayerRole.Get(), Row->GoldCounter);
	}
}
```

- [ ] **Step 3: StartClash 接入前摇与准备姿态**

在 `StartClash()` 的 `SetPhase(EBattlePhase::Clash);` 之后插入：

```cpp
	// 动画：敌方碰撞前摇 + 玩家进入准备姿态（Idle 切换）
	if (const FCombatAnimRow* Row = GetCombatAnimRow(false))
	{
		const FAnimRef& Telegraph = (ClashType == EClashType::BlueClash)
			? Row->ClashTelegraphBlue
			: Row->ClashTelegraphWhite;
		PlayCombatAnim(BossEnemy.Get(), Telegraph);
	}
	SetPlayerClashReady(true);
```

- [ ] **Step 4: ResolveClash 复位并播放结果动画**

在 `ResolveClash()` 的 `ClearClashTimers();` 之后插入：

```cpp
	SetPlayerClashReady(false);
```

在 `switch (Result)` 的 `case EClashResult::BlockSuccess:` 块末尾（`ApplyDamageTo(...GetPlayerGoldDamage()...)` 之后）插入：

```cpp
		PlayBlockSuccessChain();
		break;
```

（把原 `break;` 一并替换。）

在 `case EClashResult::DodgeSuccess:` 块末尾（闪避 Buff 施加后）插入：

```cpp
		if (const FCombatAnimRow* Row = GetCombatAnimRow(true))
		{
			PlayCombatAnim(PlayerRole.Get(), Row->DodgeSuccess);
		}
		break;
```

在 `case EClashResult::DodgeFail:` 的 `Incoming *= P.DodgeFailDamageScale;` 后插入：

```cpp
		PlayClashFailReaction(EClashResult::DodgeFail);
		break;
```

将 `case EClashResult::BlockFail:` 与 `default:` 分支替换为：

```cpp
	case EClashResult::BlockFail:
	default:
		// 全额伤害
		PlayClashFailReaction(EClashResult::BlockFail);
		break;
```

- [ ] **Step 5: 编译 + 提交（含 Task 5 改动）**

Run: 构建命令
Expected: 0 error / 0 warning（新增）。

```bash
git add Hole/Source/Hole/Public/Combat/BattleComponent.h Hole/Source/Hole/Private/Combat/BattleComponent.cpp
git commit -m "feat(combat-anim): table-driven entry montage, clash telegraph/ready, block-gold counter chain"
```

---

### Task 7: 结算死亡/胜利动画

**Files:**
- Modify: `Hole/Source/Hole/Public/Combat/BattleComponent.h`
- Modify: `Hole/Source/Hole/Private/Combat/BattleComponent.cpp`

**Interfaces:**
- Consumes: Task 4 播放辅助。
- Produces: `void PlayDeathAnimations(bool bPlayerWon)`；`FinishBattle` 在设置 `Phase=Ended` 后播放败方 `Death` 与胜方 `Victory`。

- [ ] **Step 1: 头文件声明**

在"战斗动画"区块（`SetPlayerClashReady` 后）加：

```cpp
	/** 战斗结束：败方播 Death，胜方播 Victory（空引用跳过） */
	void PlayDeathAnimations(bool bPlayerWon);
```

- [ ] **Step 2: cpp 实现并接入 FinishBattle**

在 `PlayClashFailReaction` 实现后加：

```cpp
void UBattleComponent::PlayDeathAnimations(bool bPlayerWon)
{
	if (const FCombatAnimRow* LoserRow = GetCombatAnimRow(!bPlayerWon))
	{
		PlayCombatAnim(bPlayerWon ? BossEnemy.Get() : PlayerRole.Get(), LoserRow->Death);
	}
	if (const FCombatAnimRow* WinnerRow = GetCombatAnimRow(bPlayerWon))
	{
		PlayCombatAnim(bPlayerWon ? PlayerRole.Get() : BossEnemy.Get(), WinnerRow->Victory);
	}
}
```

在 `FinishBattle()` 的 `SetPhase(EBattlePhase::Ended);` 之后插入：

```cpp
	PlayDeathAnimations(bPlayerWon);
```

- [ ] **Step 3: 编译**

Run: 构建命令
Expected: 0 error / 0 warning（新增）。

- [ ] **Step 4: 提交**

```bash
git add Hole/Source/Hole/Public/Combat/BattleComponent.h Hole/Source/Hole/Private/Combat/BattleComponent.cpp
git commit -m "feat(combat-anim): play death and victory montages when battle ends"
```

---

### Task 8: 脚本同步（13 号表）

**Files:**
- Modify: `Scripts/create_datatables.py`
- Modify: `Scripts/verify_datatables.py`
- Modify: `Scripts/export_datatables.py`

**Interfaces:**
- Consumes: Task 1 的行结构列名（`Entry.Montage` 等扁平 CSV 列）。
- Produces: `create_datatables.py` 新增 `DT_CombatAnimConfig` job（**不运行**）；verify/export 清单含该表。

- [ ] **Step 1: create_datatables.py 增加表头与行数据**

在 `# 12 - DT_BattleStage` 区块之前插入：

```python
# ---------------------------------------------------------------------------
# 13 - DT_CombatAnimConfig
# ---------------------------------------------------------------------------
ANIM_COLUMNS = [
    "Entry", "Sheathe", "RedDefense", "GoldCounter", "BlueAttack", "WhiteAttack",
    "Charge", "ChargeInterrupted", "Hurt", "BlockSuccess", "BlockFail",
    "DodgeSuccess", "DodgeFail", "Death", "Victory", "Skill",
    "ClashReady", "ClashTelegraphBlue", "ClashTelegraphWhite",
]


def _anim_headers():
    headers = ["DisplayName"]
    for column in ANIM_COLUMNS:
        headers.append("{}.Montage".format(column))
        headers.append("{}.SectionName".format(column))
        headers.append("{}.PlayRate".format(column))
        headers.append("{}.BlendOutTime".format(column))
    return headers


def _anim_ref(montage="", section="", play_rate=1.0, blend_out=0.25):
    return {
        "Montage": montage,
        "SectionName": section,
        "PlayRate": play_rate,
        "BlendOutTime": blend_out,
    }


def _anim_row(display_name, refs):
    row = {"DisplayName": display_name}
    for column in ANIM_COLUMNS:
        ref = refs.get(column, {})
        row["{}.Montage".format(column)] = ref.get("Montage", "")
        row["{}.SectionName".format(column)] = ref.get("SectionName", "")
        row["{}.PlayRate".format(column)] = ref.get("PlayRate", 1.0)
        row["{}.BlendOutTime".format(column)] = ref.get("BlendOutTime", 0.25)
    return row


COMBATANIM_HEADERS = _anim_headers()
combat_anims = [
    ("drifter", _anim_row("漂泊者", {
        "Entry": _anim_ref(
            "/Game/Blueprint/Character/Roles/Dale/Animations/Entrance/MTG_DrawGreatSword.MTG_DrawGreatSword",
            "Draw", 1.0, 0.25),
    })),
    ("satan", _anim_row("撒旦", {})),
]
```

在 `main()` 的 `jobs` 列表末尾（`DT_BattleStage` 之后）加：

```python
        ("DT_CombatAnimConfig", "CombatAnimRow", COMBATANIM_HEADERS, combat_anims),
```

把文件头 docstring 中 `currently 12 tables` 改为 `currently 13 tables`。

- [ ] **Step 2: verify_datatables.py 增加清单与抽查**

把 `TABLES` 列表末尾（`"DT_BattleStage",` 后）加：

```python
    "DT_CombatAnimConfig",
```

在 `main()` 的 `stage` 抽查块后加：

```python
    anim = unreal.load_asset("/Game/DataTable/DT_CombatAnimConfig")
    if anim is not None:
        for line in anim.export_to_csv_string().splitlines()[:3]:
            print("ANIM_CSV: " + line)
```

- [ ] **Step 3: export_datatables.py 增加清单**

在 `TABLES` 列表末尾（`"DT_BattleStage",` 后）加：

```python
    "DT_CombatAnimConfig",
```

- [ ] **Step 4: Python 语法检查**

Run: `python -m py_compile Scripts/create_datatables.py Scripts/verify_datatables.py Scripts/export_datatables.py`
Expected: 退出码 0，无输出。

- [ ] **Step 5: 提交**

```bash
git add Scripts/create_datatables.py Scripts/verify_datatables.py Scripts/export_datatables.py
git commit -m "feat(combat-anim): sync 13th anim config table into datatable scripts"
```

> **注意：** 不要运行 `create_datatables.py` 命令（会重建全部 13 张表、覆盖用户手动编辑）。资产创建在 Task 9 由用户手动完成。

---

### Task 9: 编辑器资产与 PIE 全流程验证（用户手动）

**Files（编辑器内，不入 git）：**
- Create: `/Game/DataTable/DT_CombatAnimConfig`（Row Structure = `CombatAnimRow`）
- Modify: `ABP_Dale`（新增 `ClashReady` 状态）
- Optional: 占位动画导入（玩家/敌人动作）

- [ ] **Step 1: 重启编辑器** 加载新 DLL。
- [ ] **Step 2: 新建 DataTable** `DT_CombatAnimConfig`（`/Game/DataTable`），Row Structure 选 `CombatAnimRow`；添加行 `drifter`（显示名"漂泊者"）与 `satan`（显示名"撒旦"）。
- [ ] **Step 3: 填行数据**：`drifter.Entry.Montage = /Game/Blueprint/Character/Roles/Dale/Animations/Entrance/MTG_DrawGreatSword`，`Entry.SectionName = Draw`；其余列留空。`satan` 全部留空（占位）。
- [ ] **Step 4: 占位动画（可选）**：为 `drifter` 的 `RedDefense/BlueAttack/WhiteAttack/Charge/Hurt/BlockSuccess/GoldCounter/DodgeSuccess/Death/Victory/ClashReady` 与 `satan` 的对应列填入现有/临时动画（可先用 `Great_Sword_Idle`、`Roar`、移动动画占位），跑通播放链路。
- [ ] **Step 5: ABP_Dale 增加 ClashReady 状态**：读取 `bClashReady`，true 时从拔刀 Idle 切换到 `ClashReady` 姿态（占位可用 `Great_Sword_Idle`），false 回退；保存并编译蓝图。
- [ ] **Step 6: PIE 验证清单**

| 验证点 | 操作 | 期望 |
|---|---|---|
| 入场读表 | 靠近 Boss 触发战斗 | 入场 Montage 从表播放（日志 `PlayCombatAnim`），HUD 出现 |
| 普通回合动画 | 玩家选白攻/敌人白攻 | 双方 `PlayCombatAnim` 日志出现对应动作 |
| 受击动画 | 玩家选红防/敌人白攻 | 玩家播 `Hurt` |
| 红防反击 | 玩家红防/敌人蓝攻（1层蓄力） | 玩家播 `GoldCounter` |
| 敌人反击 | 玩家蓝攻/敌人红防 | 敌人播 `GoldCounter`，玩家播 `Hurt` |
| 蓄力打断 | 玩家蓝攻/敌人蓄力 | 敌人播 `ChargeInterrupted` |
| 碰撞准备 | 玩家蓝攻/敌人蓝攻 | 敌人播 `ClashTelegraphBlue`，玩家 `bClashReady=true`（ABP 切姿态），结算后回退 |
| 格挡连播 | 碰撞中窗口内按 E | 先 `BlockSuccess` 播完，再 `GoldCounter`（日志顺序） |
| 闪避成功 | 碰撞中窗口内按 Shift | 播 `DodgeSuccess` |
| 失败回落 | 碰撞不按键 | 播 `Hurt`（或 `BlockFail` 若已填） |
| 结局 | 把敌人/玩家血量调 0 | 败方播 `Death`，胜方播 `Victory`（若已填） |

- [ ] **Step 7: 验证结果回填执行者**：PIE 中发现的问题记录到 DevLog（Task 10）或作为新 bug 任务反馈。

---

### Task 10: 文档同步

**Files:**
- Modify: `DevLog.md`
- Modify: `AGENTS.md`
- Modify: `GDD_Outline.md`（§5.2.5 一行同步 + 版本号）

- [ ] **Step 1: DevLog.md 新增一条合并记录（2026-08-05 | 策划 ⚡ + 程序）**

内容要点：战斗动画表定案（13 号表 `DT_CombatAnimConfig` + `FAnimRef`）；决策（收刀留空、敌人占位、蓝攻共用、玩家碰撞准备姿态、敌人金色反击、格挡成功=弹反+金色反击连播）；实现（USTRUCT → Subsystem 读表 → BattleComponent 播放）；回落约定；待办（编辑器资产、ABP_Dale ClashReady、占位动画）。

- [ ] **Step 2: AGENTS.md 更新**

在"### 战斗系统 v1"区块末尾追加：

```markdown
### 战斗动画（2026-08-05）
- `DT_CombatAnimConfig`（13 号表，`FCombatAnimRow` + `FAnimRef`）一行一个实体；动作列 = Montage 软引用 + Section + PlayRate + BlendOutTime；`UCombatFormulaSubsystem::GetCombatAnimRow(EntityID)` 读表，`UBattleComponent` 播放编排。
- 回落约定：BlockFail/DodgeFail/ChargeInterrupted 空 = 播 Hurt；格挡成功先播 BlockSuccess（弹反），Montage 播完回调再接 GoldCounter。
- 玩家同色碰撞准备姿态：`UBaseCharacterAnimInstance::bClashReady` + `SetClashReady(bool)`，ABP 状态切换，镜像 `bWeaponDrawn` 模式；碰撞阶段置位、结算/收刀/重试时复位。
- 入场 Montage 优先读表 `Entry`，`UBattleComponent::PlayerEntryMontage` 字段仅作回退；收刀时 `StopAllMontages` 并解绑连播回调。
```

- [ ] **Step 3: GDD_Outline.md 同步（可选，需用户确认）**

在 §5.2.5 的 v1 实现说明中，把"后续使用玩家/敌人专属碰撞动画提示"改为"已定案：`DT_CombatAnimConfig` 驱动敌方前摇与玩家准备姿态（2026-08-05）"，并将版本 v0.8 → v0.9。若用户不需要 GDD 变更则跳过此步。

- [ ] **Step 4: 提交**

```bash
git add DevLog.md AGENTS.md GDD_Outline.md
git commit -m "docs(combat-anim): record anim table design, conventions and GDD sync"
```

---

## 计划自检记录

- 覆盖：设计文档中的动画清单（玩家/敌人）、时序、回落约定、接入架构、资产占位、文档同步均有对应任务。
- 无占位符：所有代码步骤给出完整代码或精确插入点。
- 类型一致性：`GetCombatAnimRow(bool)` / `PlayCombatAnim(ABaseCharacter*, const FAnimRef&)` / `SetClashReady(bool)` / `OnBlockSuccessMontageEnded(UAnimMontage*, bool)` 在任务间签名一致。
- 风险：Task 5/6 因 `RemoveDynamic` 回调依赖需合并编译与提交，已在任务内明确提示。
