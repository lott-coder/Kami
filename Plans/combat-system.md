# 战斗系统 v1（Dale vs Satan）实施计划书

> **For agentic workers:** REQUIRED SUB-SKILL: 使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务执行本计划。任务使用复选框（`- [ ]`）语法跟踪。

**Goal:** 实现首个可玩战斗闭环：玩家靠近 Satan → 播放 Boss 入场动画 → 动画结束/跳过 → 进入战斗（HUD + 回合逻辑 + 同色实时对抗）→ 分出胜负 → 失败回到 Boss 触发点重开、胜利退出战斗。

**Architecture:** 沿用项目三层架构（USTRUCT 纯数据 → UGameInstanceSubsystem 公式 → 组件运行时状态）。新增 `UBattleComponent`（挂在 BP_Dale 上，战斗会话状态机 + 回合结算 + 镜头/输入/HUD 编排）、`UEnemyCombatAIComponent`（挂在 BP_Satan 上，v1 全随机选行动）、`UCombatHUDWidget` C++ 基类 + WBP_CombatHUD 皮肤。伤害一律经 `UCombatFormulaSubsystem` 计算，组件不写公式。

**Tech Stack:** Unreal Engine 5.6，C++（Hole 模块）+ Blueprint（组件挂载、HUD 皮肤、输入资产），Enhanced Input，UMG，Level Sequence（复用现有 BossIntro）。

---

## 变更记录（2026-08-02，用户确认）

1. **行动选择只走 HUD 鼠标点击**：移除五个行动按键（红防=1、蓝攻=2、白攻=3、蓄力=4、技能=5）的输入绑定（提交 7892efc）。`IMC_Combat` 只保留 `IA_CombatBlock`(E) 与 `IA_CombatDodge`(Left Shift)；`UBattleComponent` 不再持有 5 个行动 UInputAction 字段。Task 10 相应只创建 2 个 InputAction。
2. **站位与镜头参数收进 DataTable（2026-08-02 终版）**：新增 12 号表 `DT_BattleStage`（`FCombatStageRow`，`/Game/DataTable/DT_BattleStage`，单行 `Default`），字段 `PlayerBattleOffset` / `bBossFacePlayer` / `bPlayerFaceBoss` / `CameraPitch` / `CameraYawOffset` / `CameraArmLength`。`UBattleComponent` 经 `UCombatFormulaSubsystem::GetBattleStageRow()` 读取，USTRUCT 默认值为回退源；资产已由 `Scripts/create_datatables.py` 生成。DataAsset 方案已废弃（提交 5337dae 被本方案替换）。
3. **DT_BattleStage 扩展 Spring/位移选项（2026-08-02）**：新增 `bPlayerOffsetInBossLocalSpace`（Boss 本地/世界空间）、`BossFacingYawOffset`、`PlayerFacingYawOffset`、`CameraFOV`、`SpringSocketOffset`、`SpringTargetOffset`、`bSpringEnableCameraLag`、`SpringCameraLagSpeed`；战斗结束恢复 SocketOffset/TargetOffset/FOV。
4. **HUD 资产路径按实际创建位置对齐**：`WBP_CombatHUD` 实际位于 `Content/HUD`（非 `Content/UI`），`UBattleComponent` 默认加载路径已改为 `/Game/HUD/WBP_CombatHUD`。

## Global Constraints

- UE 5.6 安装路径：`D:\Software\UnrealEngine\UE_5.6`
- 每次修改 C++ 后立即编译；构建命令固定为：
  ```
  D:\Software\UnrealEngine\UE_5.6\Engine\Build\BatchFiles\Build.bat HoleEditor Win64 Development "d:\UE5\UE_project\Kami\Hole\Hole.uproject"
  ```
- 重试上限 3 次：任何任务失败 3 次后停止，把完整错误发给用户决策。
- 三层架构：USTRUCT 只存数值；跨表/公式只进 `UCombatFormulaSubsystem`；运行时状态只存组件。
- 性能：优先 `ActorHasTag`（不用 `Cast` 循环找目标）、`TMap`、缓存引用、FTimer 替代 Tick；HUD 只在状态变更和碰撞倒计时时刷新，禁止每帧做全量刷新。
- 新行为入口优先组件/DataAsset，不改基类；本次只允许对 `ARole::Look` 加一个锁定门控（最小改动）。
- 战斗数值参数一律读 `DT_CombatParams`（已有）；v1 敌人 AI 权重先代码内置并标注 `[PLAYTEST]`，后续按策划需求改表。
- 用户文档（GDD/DevLog/DataTable_Spec）保持中文；AGENTS.md 保持英文；提交信息用 conventional commits（`feat:` / `fix:` / `docs:`）。
- 资产路径：输入资产放 `Content/Input`，HUD 放 `Content/HUD`；C++ 放 `Hole/Source/Hole/Public|Private` 对应子目录。

---

## 文件结构

| 文件 | 责任 | 动作 |
|---|---|---|
| `Hole/Source/Hole/Hole.Build.cs` | 增加 UMG/Slate/SlateCore 依赖 | 修改 |
| `Hole/Source/Hole/Public/Combat/BattleTypes.h` | 战斗枚举（阶段/行动/碰撞类型）+ `FTurnResolution` | 新建 |
| `Hole/Source/Hole/Public/Combat/BattleComponent.h` + `Private/Combat/BattleComponent.cpp` | 战斗会话状态机、回合结算、同色对抗、镜头/输入/HUD 编排 | 新建 |
| `Hole/Source/Hole/Public/Combat/EnemyCombatAIComponent.h` + `Private/Combat/EnemyCombatAIComponent.cpp` | 敌人行动选择（v1 全随机） | 新建 |
| `Hole/Source/Hole/Public/UI/CombatHUDWidget.h` + `Private/UI/CombatHUDWidget.cpp` | 战斗 HUD C++ 基类（BindWidget + 数据刷新 + 按钮回调） | 新建 |
| `Hole/Source/Hole/Public/Component/BossIntroComponent.h` + `Private/Component/BossIntroComponent.cpp` | 增加 `OnIntroFinished` 动态委托 | 修改 |
| `Hole/Source/Hole/Public/Character/Role.h` + `Private/Character/Role.cpp` | `Look` 受 `bCinematicLocked` 门控 | 修改 |
| `Hole/Content/Input/IA_Combat*.uasset`（7 个）+ `IMC_Combat.uasset` | 战斗输入资产 | 新建（编辑器） |
| `Hole/Content/HUD/WBP_CombatHUD.uasset` | HUD 皮肤（父类 `UCombatHUDWidget`） | 新建（编辑器） |
| `BP_Dale.uasset` | 挂载 `UBattleComponent` | 修改（编辑器） |
| `BP_Satan.uasset` | 增加 Tag `Boss`、挂载 `UEnemyCombatAIComponent` | 修改（编辑器） |

---

### Task 1: 构建依赖 + 战斗类型定义

**Files:**
- Modify: `Hole/Source/Hole/Hole.Build.cs`
- Create: `Hole/Source/Hole/Public/Combat/BattleTypes.h`

**Interfaces:**
- Produces: `EBattlePhase`、`EBattleAction`、`EClashType`、`EClashResult`、`FTurnResolution`（后续所有任务引用）。

- [ ] **Step 1: 修改 `Hole.Build.cs`**

把 `PublicDependencyModuleNames.AddRange` 改为：

```csharp
PublicDependencyModuleNames.AddRange(new string[]
{
	"Core",
	"CoreUObject",
	"Engine",
	"InputCore",
	"EnhancedInput",
	"LevelSequence",
	"MovieScene",
	"UMG",
	"Slate",
	"SlateCore"
});
```

- [ ] **Step 2: 新建 `BattleTypes.h`**

```cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BattleTypes.generated.h"

// ============================================================================
// 战斗阶段
// ============================================================================
UENUM(BlueprintType)
enum class EBattlePhase : uint8
{
	Idle			UMETA(DisplayName = "空闲"),
	Entering		UMETA(DisplayName = "入场"),
	ActionSelect	UMETA(DisplayName = "行动选择"),
	Resolving		UMETA(DisplayName = "结算中"),
	Clash			UMETA(DisplayName = "同色对抗"),
	Ended			UMETA(DisplayName = "已结束")
};

// ============================================================================
// 战斗行动（红防/蓝攻/白攻/蓄力/技能）
// ============================================================================
UENUM(BlueprintType)
enum class EBattleAction : uint8
{
	None		UMETA(DisplayName = "无"),
	RedDefense	UMETA(DisplayName = "红色防御"),
	BlueAttack	UMETA(DisplayName = "蓝色攻击"),
	WhiteAttack	UMETA(DisplayName = "白色攻击"),
	Charge		UMETA(DisplayName = "蓄力"),
	Skill		UMETA(DisplayName = "技能")
};

// ============================================================================
// 同色碰撞类型
// ============================================================================
UENUM(BlueprintType)
enum class EClashType : uint8
{
	None		UMETA(DisplayName = "无"),
	BlueClash	UMETA(DisplayName = "蓝色碰撞"),
	WhiteClash	UMETA(DisplayName = "白色碰撞")
};

// ============================================================================
// 同色碰撞结果
// ============================================================================
UENUM(BlueprintType)
enum class EClashResult : uint8
{
	None			UMETA(DisplayName = "无"),
	BlockSuccess	UMETA(DisplayName = "格挡成功"),
	BlockFail		UMETA(DisplayName = "格挡失败"),
	DodgeSuccess	UMETA(DisplayName = "闪避成功"),
	DodgeFail		UMETA(DisplayName = "闪避失败")
};

// ============================================================================
// 回合结算结果（ResolveNormalTurn / ResolveExtraTurn 的返回值）
// 注：bClash=true 时，PlayerDamageTaken 表示“防御前的原始伤害”，由对抗阶段再修正
// ============================================================================
USTRUCT(BlueprintType)
struct HOLE_API FTurnResolution
{
	GENERATED_BODY()

	/** 玩家受到的伤害（同色碰撞时=防御前原始值） */
	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	float PlayerDamageTaken = 0.0f;

	/** 敌人受到的伤害 */
	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	float EnemyDamageTaken = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	bool bPlayerChargeInterrupted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	bool bEnemyChargeInterrupted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	bool bPlayerExtraTurn = false;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	bool bEnemyExtraTurn = false;

	/** 是否进入同色实时对抗 */
	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	bool bClash = false;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	EClashType ClashType = EClashType::None;
};
```

- [ ] **Step 3: 编译**

运行构建命令。预期：构建成功（0 error）。

- [ ] **Step 4: 提交**

```bash
git add Hole/Source/Hole/Hole.Build.cs Hole/Source/Hole/Public/Combat/BattleTypes.h
git commit -m "feat(combat): add battle types and UMG module dependencies"
```

---

### Task 2: BossIntroComponent 增加 OnIntroFinished 委托

**Files:**
- Modify: `Hole/Source/Hole/Public/Component/BossIntroComponent.h`
- Modify: `Hole/Source/Hole/Private/Component/BossIntroComponent.cpp`

**Interfaces:**
- Produces: `UBossIntroComponent::OnIntroFinished`（动态多播委托，`BlueprintAssignable`），在 `CompleteIntro()` / `SkipIntro()` 进入 Combat 后广播。

- [ ] **Step 1: 头文件增加委托声明**

在 `#include "BossIntroComponent.generated.h"` 之后、`UCLASS` 之前加入：

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBossIntroFinished);
```

在类内 `public:` 区域（`GetDetectedPlayer()` 声明之前）加入：

```cpp
	// ---- 事件 ----

	/** 入场动画正常结束或被跳过后广播（此时状态已为 Combat），战斗系统监听此事件启动战斗 */
	UPROPERTY(BlueprintAssignable, Category = "BossIntro|Events")
	FOnBossIntroFinished OnIntroFinished;
```

- [ ] **Step 2: cpp 中广播**

在 `UBossIntroComponent::CompleteIntro()` 的 `SetState(EBossIntroState::Combat);` 之后加入：

```cpp
	OnIntroFinished.Broadcast();
```

在 `UBossIntroComponent::SkipIntro()` 的 `SetState(EBossIntroState::Combat);` 之后加入：

```cpp
	OnIntroFinished.Broadcast();
```

- [ ] **Step 3: 编译**

运行构建命令。预期：构建成功。

- [ ] **Step 4: 提交**

```bash
git add Hole/Source/Hole/Public/Component/BossIntroComponent.h Hole/Source/Hole/Private/Component/BossIntroComponent.cpp
git commit -m "feat(combat): broadcast boss intro finished event"
```

---

### Task 3: EnemyCombatAIComponent（v1 全随机）

**Files:**
- Create: `Hole/Source/Hole/Public/Combat/EnemyCombatAIComponent.h`
- Create: `Hole/Source/Hole/Private/Combat/EnemyCombatAIComponent.cpp`

**Interfaces:**
- Produces: `UEnemyCombatAIComponent::ChooseAction(int32 RoundNumber, EBattleAction LastPlayerAction, bool bExtraTurn, int32 ChargeStacks) const -> EBattleAction`。

- [ ] **Step 1: 新建头文件**

```cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/BattleTypes.h"
#include "EnemyCombatAIComponent.generated.h"

/**
 * UEnemyCombatAIComponent - 敌人战斗 AI（挂在 BP_Satan 上）
 *
 * v1 策略：全随机四选一（红防/蓝攻/白攻/蓄力）；
 * 额外回合时按 GDD 规则只在 蓝攻/蓄力 中二选一。
 * AIDifficulty / AIPreference 暂不参与权重，保留接口供后续策划需求接入。
 */
UCLASS(ClassGroup = (Combat), Blueprintable, meta = (BlueprintSpawnableComponent))
class HOLE_API UEnemyCombatAIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyCombatAIComponent();

	/** 选择敌人本回合行动；bExtraTurn=true 时返回 BlueAttack 或 Charge */
	UFUNCTION(BlueprintCallable, Category = "EnemyAI")
	EBattleAction ChooseAction(int32 RoundNumber, EBattleAction LastPlayerAction, bool bExtraTurn, int32 ChargeStacks) const;
};
```

- [ ] **Step 2: 新建 cpp**

```cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/EnemyCombatAIComponent.h"

UEnemyCombatAIComponent::UEnemyCombatAIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

EBattleAction UEnemyCombatAIComponent::ChooseAction(int32 RoundNumber, EBattleAction LastPlayerAction, bool bExtraTurn, int32 ChargeStacks) const
{
	if (bExtraTurn)
	{
		// GDD 5.2.4：额外回合只能 出蓝刀 或 继续蓄力
		return FMath::RandBool() ? EBattleAction::BlueAttack : EBattleAction::Charge;
	}

	switch (FMath::RandRange(0, 3))
	{
	case 0:
		return EBattleAction::RedDefense;
	case 1:
		return EBattleAction::BlueAttack;
	case 2:
		return EBattleAction::WhiteAttack;
	default:
		return EBattleAction::Charge;
	}
}
```

（`RoundNumber` / `LastPlayerAction` / `ChargeStacks` 暂未参与计算，保留参数并加注释，编译器会提示未使用，属预期。）

- [ ] **Step 3: 编译**

运行构建命令。预期：构建成功。

- [ ] **Step 4: 提交**

```bash
git add Hole/Source/Hole/Public/Combat/EnemyCombatAIComponent.h Hole/Source/Hole/Private/Combat/EnemyCombatAIComponent.cpp
git commit -m "feat(combat): add random enemy combat AI component"
```

---

### Task 4: CombatHUDWidget C++ 基类

**Files:**
- Create: `Hole/Source/Hole/Public/UI/CombatHUDWidget.h`
- Create: `Hole/Source/Hole/Private/UI/CombatHUDWidget.cpp`

**Interfaces:**
- Consumes: `UBattleComponent` 的 `GetPlayerCharacter()` / `GetBossEnemy()` / `GetRoundNumber()` / `GetPlayerChargeStacks()` / `GetEnemyChargeStacks()` / `IsPlayerExtraTurn()` / `PlayerChooseAction(EBattleAction)` / `OnBattleStateChanged`（Task 5 提供）。
- Produces: `BindToBattle(UBattleComponent*)`、`SetChoiceButtonsEnabled(...)`、`ShowClashPrompt(FText, float)`、`HideClashPrompt()`、`ShowResult(FText)`、`HideResult()`。

- [ ] **Step 1: 新建头文件**

```cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Combat/BattleTypes.h"
#include "CombatHUDWidget.generated.h"

class UBattleComponent;
class UProgressBar;
class UTextBlock;
class UButton;
class UWidget;

/**
 * UCombatHUDWidget - 战斗 HUD C++ 基类
 *
 * 所有控件通过 BindWidget 绑定 WBP_CombatHUD 中同名字面量；
 * C++ 负责数据刷新与按钮回调，BP 只做布局/皮肤。
 */
UCLASS(Blueprintable)
class HOLE_API UCombatHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "CombatHUD")
	void BindToBattle(UBattleComponent* InBattle);

	UFUNCTION(BlueprintCallable, Category = "CombatHUD")
	void SetChoiceButtonsEnabled(bool bRed, bool bBlue, bool bWhite, bool bCharge, bool bSkill);

	UFUNCTION(BlueprintCallable, Category = "CombatHUD")
	void ShowClashPrompt(const FText& Text, float TotalTime);

	UFUNCTION(BlueprintCallable, Category = "CombatHUD")
	void HideClashPrompt();

	UFUNCTION(BlueprintCallable, Category = "CombatHUD")
	void ShowResult(const FText& Text);

	UFUNCTION(BlueprintCallable, Category = "CombatHUD")
	void HideResult();

protected:
	// ---- BindWidget：名称必须与 WBP_CombatHUD 中的控件名完全一致 ----

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> PlayerHealthBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> EnemyHealthBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PlayerNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EnemyNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> RoundText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PlayerChargeText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EnemyChargeText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> RedDefenseButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BlueAttackButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> WhiteAttackButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ChargeButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SkillButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> ClashPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ClashPromptText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> ClashWindowBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> ResultPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ResultText;

private:
	UFUNCTION()
	void OnRedDefenseClicked();

	UFUNCTION()
	void OnBlueAttackClicked();

	UFUNCTION()
	void OnWhiteAttackClicked();

	UFUNCTION()
	void OnChargeClicked();

	UFUNCTION()
	void OnSkillClicked();

	UFUNCTION()
	void HandleBattleStateChanged();

	void RefreshAll();

	TWeakObjectPtr<UBattleComponent> Battle;
	float ClashTotalTime = 0.0f;
	float ClashRemainingTime = 0.0f;
	bool bClashActive = false;
};
```

- [ ] **Step 2: 新建 cpp**

```cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/CombatHUDWidget.h"
#include "Combat/BattleComponent.h"
#include "Character/BaseCharacter.h"
#include "Character/Enemy.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UCombatHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (RedDefenseButton)
	{
		RedDefenseButton->OnClicked.AddDynamic(this, &UCombatHUDWidget::OnRedDefenseClicked);
	}
	if (BlueAttackButton)
	{
		BlueAttackButton->OnClicked.AddDynamic(this, &UCombatHUDWidget::OnBlueAttackClicked);
	}
	if (WhiteAttackButton)
	{
		WhiteAttackButton->OnClicked.AddDynamic(this, &UCombatHUDWidget::OnWhiteAttackClicked);
	}
	if (ChargeButton)
	{
		ChargeButton->OnClicked.AddDynamic(this, &UCombatHUDWidget::OnChargeClicked);
	}
	if (SkillButton)
	{
		SkillButton->OnClicked.AddDynamic(this, &UCombatHUDWidget::OnSkillClicked);
	}

	HideClashPrompt();
	HideResult();
}

void UCombatHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 仅在同色碰撞倒计时期间做逐帧刷新（性能：不在此处刷血条）
	if (!bClashActive)
	{
		return;
	}

	ClashRemainingTime -= InDeltaTime;
	if (ClashWindowBar)
	{
		const float Percent = ClashTotalTime > 0.0f
			? FMath::Clamp(ClashRemainingTime / ClashTotalTime, 0.0f, 1.0f)
			: 0.0f;
		ClashWindowBar->SetPercent(Percent);
	}
	if (ClashRemainingTime <= 0.0f)
	{
		bClashActive = false;
	}
}

void UCombatHUDWidget::BindToBattle(UBattleComponent* InBattle)
{
	Battle = InBattle;
	if (Battle.IsValid())
	{
		Battle->OnBattleStateChanged.AddDynamic(this, &UCombatHUDWidget::HandleBattleStateChanged);
	}
	RefreshAll();
}

void UCombatHUDWidget::HandleBattleStateChanged()
{
	RefreshAll();
}

void UCombatHUDWidget::RefreshAll()
{
	if (!Battle.IsValid())
	{
		return;
	}

	ABaseCharacter* Player = Battle->GetPlayerCharacter();
	ABaseCharacter* Enemy = Battle->GetBossEnemy();

	if (PlayerHealthBar && Player)
	{
		PlayerHealthBar->SetPercent(Player->GetHealthPercent());
	}
	if (EnemyHealthBar && Enemy)
	{
		EnemyHealthBar->SetPercent(Enemy->GetHealthPercent());
	}
	if (PlayerNameText && Player)
	{
		PlayerNameText->SetText(FText::FromName(Player->CharacterID));
	}
	if (EnemyNameText && Enemy)
	{
		EnemyNameText->SetText(FText::FromName(Enemy->EnemyID));
	}
	if (RoundText)
	{
		RoundText->SetText(FText::Format(FText::FromString(TEXT("回合 {0}")), FText::AsNumber(Battle->GetRoundNumber())));
	}
	if (PlayerChargeText)
	{
		PlayerChargeText->SetText(FText::Format(FText::FromString(TEXT("蓄力 {0}")), FText::AsNumber(Battle->GetPlayerChargeStacks())));
	}
	if (EnemyChargeText)
	{
		EnemyChargeText->SetText(FText::Format(FText::FromString(TEXT("蓄力 {0}")), FText::AsNumber(Battle->GetEnemyChargeStacks())));
	}

	// 额外回合：只允许 蓝攻/蓄力；技能 v1 始终禁用
	const bool bExtra = Battle->IsPlayerExtraTurn();
	SetChoiceButtonsEnabled(!bExtra, true, !bExtra, true, false);
}

void UCombatHUDWidget::SetChoiceButtonsEnabled(bool bRed, bool bBlue, bool bWhite, bool bCharge, bool bSkill)
{
	if (RedDefenseButton) RedDefenseButton->SetIsEnabled(bRed);
	if (BlueAttackButton) BlueAttackButton->SetIsEnabled(bBlue);
	if (WhiteAttackButton) WhiteAttackButton->SetIsEnabled(bWhite);
	if (ChargeButton) ChargeButton->SetIsEnabled(bCharge);
	if (SkillButton) SkillButton->SetIsEnabled(bSkill);
}

void UCombatHUDWidget::ShowClashPrompt(const FText& Text, float TotalTime)
{
	ClashTotalTime = TotalTime;
	ClashRemainingTime = TotalTime;
	bClashActive = true;

	if (ClashPanel)
	{
		ClashPanel->SetVisibility(ESlateVisibility::Visible);
	}
	if (ClashPromptText)
	{
		ClashPromptText->SetText(Text);
	}
	if (ClashWindowBar)
	{
		ClashWindowBar->SetPercent(1.0f);
	}
}

void UCombatHUDWidget::HideClashPrompt()
{
	bClashActive = false;
	if (ClashPanel)
	{
		ClashPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UCombatHUDWidget::ShowResult(const FText& Text)
{
	if (ResultPanel)
	{
		ResultPanel->SetVisibility(ESlateVisibility::Visible);
	}
	if (ResultText)
	{
		ResultText->SetText(Text);
	}
}

void UCombatHUDWidget::HideResult()
{
	if (ResultPanel)
	{
		ResultPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UCombatHUDWidget::OnRedDefenseClicked()
{
	if (Battle.IsValid()) Battle->PlayerChooseAction(EBattleAction::RedDefense);
}

void UCombatHUDWidget::OnBlueAttackClicked()
{
	if (Battle.IsValid()) Battle->PlayerChooseAction(EBattleAction::BlueAttack);
}

void UCombatHUDWidget::OnWhiteAttackClicked()
{
	if (Battle.IsValid()) Battle->PlayerChooseAction(EBattleAction::WhiteAttack);
}

void UCombatHUDWidget::OnChargeClicked()
{
	if (Battle.IsValid()) Battle->PlayerChooseAction(EBattleAction::Charge);
}

void UCombatHUDWidget::OnSkillClicked()
{
	if (Battle.IsValid()) Battle->PlayerChooseAction(EBattleAction::Skill);
}
```

- [ ] **Step 3: 编译**

运行构建命令。预期：构建成功（Task 5 尚未实现 `UBattleComponent`，此处只有前向声明，cpp 仅在运行时调用其方法，编译阶段允许；若 UHT 报缺失类型，则先做 Task 5 头文件再回来编译——两种顺序都接受，但必须保证最终一起编译通过）。

> 注意：本任务与 Task 5 有头文件依赖，实际执行时可把 Task 4 + Task 5 的头文件先落地，再统一编译一次。

- [ ] **Step 4: 提交**

```bash
git add Hole/Source/Hole/Public/UI/CombatHUDWidget.h Hole/Source/Hole/Private/UI/CombatHUDWidget.cpp
git commit -m "feat(combat): add combat HUD widget base class"
```

---

### Task 5: BattleComponent 头文件 + 骨架

**Files:**
- Create: `Hole/Source/Hole/Public/Combat/BattleComponent.h`
- Create: `Hole/Source/Hole/Private/Combat/BattleComponent.cpp`（本任务先放骨架，Task 6/7/8 逐步补全）

**Interfaces:**
- Produces: `StartBattle()`、`EndBattle()`、`PlayerChooseAction(EBattleAction)`、`SetEnemyForcedAction(EBattleAction, bool)`、`DebugSetPlayerHealth(float)`、`DebugSetEnemyHealth(float)`、`GetBattlePhase()`、`GetPlayerCharacter()`、`GetBossEnemy()`、`GetRoundNumber()`、`GetPlayerChargeStacks()`、`GetEnemyChargeStacks()`、`GetPlayerLastAction()`、`IsPlayerExtraTurn()`、`OnBattleStateChanged`。

- [ ] **Step 1: 新建头文件（完整版，后续任务只改 cpp）**

```cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/BattleTypes.h"
#include "BattleComponent.generated.h"

class ARole;
class AEnemy;
class ABaseCharacter;
class UCombatHUDWidget;
class UEnemyCombatAIComponent;
class UInputAction;
class UInputMappingContext;
class UAttributeComponent;
class UCombatFormulaSubsystem;
class UBossIntroComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBattleStateChanged);

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

	// ==================== 配置 ====================

	/** 战斗 HUD 类（默认自动加载 /Game/UI/WBP_CombatHUD） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Config")
	TSubclassOf<UCombatHUDWidget> CombatHUDClass;

	/** 战斗输入映射（默认自动加载 /Game/Input/IMC_Combat） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Config")
	TObjectPtr<UInputMappingContext> CombatMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Config")
	TObjectPtr<UInputAction> RedDefenseAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Config")
	TObjectPtr<UInputAction> BlueAttackAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Config")
	TObjectPtr<UInputAction> WhiteAttackAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Config")
	TObjectPtr<UInputAction> ChargeAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Config")
	TObjectPtr<UInputAction> SkillAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Config")
	TObjectPtr<UInputAction> BlockAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Config")
	TObjectPtr<UInputAction> DodgeAction;

	/** 玩家战斗站位与 Boss 的距离（cm） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Config")
	float BattleDistance = 550.0f;

	/** 战斗固定摄像机俯仰角（负值=略俯视） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Config")
	float BattleCameraPitch = -12.0f;

	/** 战斗时 SpringArm 长度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Config")
	float BattleCameraArmLength = 400.0f;

	/** 同色碰撞：敌方攻击前摇时间（秒），期间提示格挡/闪避 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Config")
	float ClashTelegraphTime = 0.8f;

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

	/** 玩家选择行动（HUD 按钮 / 战斗输入回调统一入口） */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void PlayerChooseAction(EBattleAction Action);

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
	int32 GetEnemyChargeStacks() const { return EnemyChargeStacks; }

	UFUNCTION(BlueprintPure, Category = "Battle")
	EBattleAction GetPlayerLastAction() const { return PlayerLastAction; }

	UFUNCTION(BlueprintPure, Category = "Battle")
	bool IsPlayerExtraTurn() const { return bPlayerExtraTurnPending; }

protected:
	UFUNCTION()
	void HandleIntroFinished();

private:
	// ==================== 流程 ====================

	AEnemy* FindBossEnemy() const;
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
	void FinishBattle(bool bPlayerWon);
	void HandleVictoryCleanup();
	void HandleDefeatRestart();
	void ResetForRetry();

	// ==================== 同色碰撞 ====================

	void StartClash(EClashType ClashType);
	void OpenClashWindow();
	void OnClashImpact();
	void ResolveClash(EClashResult Result);
	void ClearClashTimers();

	// ==================== 输入 ====================

	UFUNCTION()
	void OnRedDefensePressed();

	UFUNCTION()
	void OnBlueAttackPressed();

	UFUNCTION()
	void OnWhiteAttackPressed();

	UFUNCTION()
	void OnChargePressed();

	UFUNCTION()
	void OnSkillPressed();

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

	bool bClashWindowOpen = false;
	bool bClashResolved = false;
	bool bClashStarted = false;
	EClashType ActiveClashType = EClashType::None;
	EClashResult PendingClashResult = EClashResult::None;
	float PendingIncomingDamage = 0.0f;
	float PendingOutgoingDamage = 0.0f;

	FVector PlayerStartLocation;
	FRotator PlayerStartActorRotation;
	FRotator PlayerStartControlRotation;
	float OriginalArmLength = 300.0f;
	bool bOriginalCameraLag = true;
	float OriginalLagSpeed = 10.0f;
	FVector BossStartLocation;
	FRotator BossStartRotation;

	FTimerHandle ClashOpenTimer;
	FTimerHandle ClashImpactTimer;
	FTimerHandle EndDelayTimer;

	bool bCombatInputBound = false;
	bool bCombatMappingAdded = false;
	bool bMouseCursorShown = false;
};
```

- [ ] **Step 2: 新建 cpp 骨架**

```cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/BattleComponent.h"
#include "Combat/EnemyCombatAIComponent.h"
#include "UI/CombatHUDWidget.h"
#include "Character/Role.h"
#include "Character/Enemy.h"
#include "Character/BaseCharacter.h"
#include "Component/AttributeComponent.h"
#include "Component/BossIntroComponent.h"
#include "Subsystem/CombatFormulaSubsystem.h"
#include "DataTable/CombatParamsTable.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

UBattleComponent::UBattleComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FClassFinder<UCombatHUDWidget> HUDClass(TEXT("/Game/UI/WBP_CombatHUD"));
	if (HUDClass.Succeeded())
	{
		CombatHUDClass = HUDClass.Class;
	}

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> CombatIMC(TEXT("/Game/Input/IMC_Combat"));
	if (CombatIMC.Succeeded())
	{
		CombatMappingContext = CombatIMC.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> RedDefense(TEXT("/Game/Input/IA_CombatRedDefense"));
	if (RedDefense.Succeeded()) RedDefenseAction = RedDefense.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> BlueAttack(TEXT("/Game/Input/IA_CombatBlueAttack"));
	if (BlueAttack.Succeeded()) BlueAttackAction = BlueAttack.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> WhiteAttack(TEXT("/Game/Input/IA_CombatWhiteAttack"));
	if (WhiteAttack.Succeeded()) WhiteAttackAction = WhiteAttack.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> Charge(TEXT("/Game/Input/IA_CombatCharge"));
	if (Charge.Succeeded()) ChargeAction = Charge.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> Skill(TEXT("/Game/Input/IA_CombatSkill"));
	if (Skill.Succeeded()) SkillAction = Skill.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> Block(TEXT("/Game/Input/IA_CombatBlock"));
	if (Block.Succeeded()) BlockAction = Block.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> Dodge(TEXT("/Game/Input/IA_CombatDodge"));
	if (Dodge.Succeeded()) DodgeAction = Dodge.Object;
}

void UBattleComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ARole* Role = Cast<ARole>(GetOwner()))
	{
		PlayerRole = Role;
	}

	BossEnemy = FindBossEnemy();
	if (BossEnemy.IsValid())
	{
		if (UBossIntroComponent* Intro = BossEnemy->FindComponentByClass<UBossIntroComponent>())
		{
			Intro->OnIntroFinished.AddDynamic(this, &UBattleComponent::HandleIntroFinished);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::BeginPlay - 场景中未找到 Tag=Boss 的 AEnemy"));
	}
}

void UBattleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearClashTimers();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(EndDelayTimer);
	}
	UnbindCombatInput();
	RemoveCombatMapping();

	Super::EndPlay(EndPlayReason);
}

// ==================== 公开接口 ====================

void UBattleComponent::StartBattle()
{
	if (Phase != EBattlePhase::Idle || bBossDefeated)
	{
		return;
	}

	if (!BossEnemy.IsValid())
	{
		BossEnemy = FindBossEnemy();
	}
	if (!BossEnemy.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::StartBattle - 未找到 Tag=Boss 的 AEnemy"));
		return;
	}

	PlayerRole = Cast<ARole>(GetOwner());
	if (!PlayerRole.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::StartBattle - Owner 不是 ARole"));
		return;
	}

	EnemyAI = BossEnemy->FindComponentByClass<UEnemyCombatAIComponent>();
	EnterBattle();
}

void UBattleComponent::EndBattle()
{
	if (Phase == EBattlePhase::Idle || Phase == EBattlePhase::Ended)
	{
		return;
	}

	ClearClashTimers();
	UnbindCombatInput();
	RemoveCombatMapping();
	if (CombatHUD.IsValid())
	{
		CombatHUD->HideClashPrompt();
	}
	HideHUD();
	RestoreExplorationState();
	UnlockPlayer();
	SetPhase(EBattlePhase::Idle);
	OnBattleStateChanged.Broadcast();
}

void UBattleComponent::PlayerChooseAction(EBattleAction Action)
{
	if (Phase != EBattlePhase::ActionSelect || bPlayerChoseAction)
	{
		return;
	}
	if (Action == EBattleAction::Skill)
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::PlayerChooseAction - 技能系统未实装"));
		return;
	}
	if (bPlayerExtraTurnPending && Action != EBattleAction::BlueAttack && Action != EBattleAction::Charge)
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::PlayerChooseAction - 额外回合只能蓝攻或蓄力"));
		return;
	}

	bPlayerChoseAction = true;
	PlayerLastAction = Action;
	OnBattleStateChanged.Broadcast();

	if (bPlayerExtraTurnPending)
	{
		const FTurnResolution Resolution = ResolveExtraTurn(true, Action);
		bPlayerExtraTurnPending = false;
		ApplyResolution(Resolution);
		if (!bClashStarted)
		{
			EndTurnAndAdvance();
		}
		return;
	}

	const FTurnResolution Resolution = ResolveNormalTurn(Action, EnemyChosenAction);
	ApplyResolution(Resolution);
	if (!bClashStarted)
	{
		EndTurnAndAdvance();
	}
}

void UBattleComponent::SetEnemyForcedAction(EBattleAction Action, bool bEnabled)
{
	ForcedEnemyAction = Action;
	bForcedEnemyActionEnabled = bEnabled;
}

void UBattleComponent::DebugSetPlayerHealth(float Value)
{
	if (PlayerRole.IsValid())
	{
		PlayerRole->CurrentHealth = FMath::Clamp(Value, 0.0f, PlayerRole->GetMaxHealth());
	}
	OnBattleStateChanged.Broadcast();
}

void UBattleComponent::DebugSetEnemyHealth(float Value)
{
	if (BossEnemy.IsValid())
	{
		BossEnemy->CurrentHealth = FMath::Clamp(Value, 0.0f, BossEnemy->GetMaxHealth());
	}
	OnBattleStateChanged.Broadcast();
}

ABaseCharacter* UBattleComponent::GetPlayerCharacter() const
{
	return PlayerRole.Get();
}

AEnemy* UBattleComponent::GetBossEnemy() const
{
	return BossEnemy.Get();
}

// ==================== 内部流程（Task 6/7/8 补全） ====================

AEnemy* UBattleComponent::FindBossEnemy() const
{
	if (!GetWorld())
	{
		return nullptr;
	}
	for (TActorIterator<AEnemy> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(FName(TEXT("Boss"))))
		{
			return *It;
		}
	}
	return nullptr;
}

void UBattleComponent::HandleIntroFinished()
{
	if (Phase == EBattlePhase::Idle && BossEnemy.IsValid())
	{
		StartBattle();
	}
}

void UBattleComponent::EnterBattle()
{
	PositionBattleActors();
	LockPlayer();
	AddCombatMapping();
	SetupCombatInput();
	ShowHUD();
	SetPhase(EBattlePhase::Entering);
	OnBattleStateChanged.Broadcast();
	StartNewRound();
}

void UBattleComponent::StartNewRound()
{
	RoundNumber++;
	bPlayerChoseAction = false;
	EnemyChosenAction = EBattleAction::None;
	bPlayerExtraTurnPending = false;
	bEnemyExtraTurnPending = false;
	ChooseEnemyAction(false);
	SetPhase(EBattlePhase::ActionSelect);
	OnBattleStateChanged.Broadcast();
}

void UBattleComponent::ChooseEnemyAction(bool bExtraTurn)
{
	if (!bExtraTurn && bForcedEnemyActionEnabled)
	{
		EnemyChosenAction = ForcedEnemyAction;
		return;
	}

	if (EnemyAI.IsValid())
	{
		EnemyChosenAction = EnemyAI->ChooseAction(RoundNumber, PlayerLastAction, bExtraTurn, EnemyChargeStacks);
	}
	else if (bExtraTurn)
	{
		EnemyChosenAction = FMath::RandBool() ? EBattleAction::BlueAttack : EBattleAction::Charge;
	}
	else
	{
		switch (FMath::RandRange(0, 3))
		{
		case 0: EnemyChosenAction = EBattleAction::RedDefense; break;
		case 1: EnemyChosenAction = EBattleAction::BlueAttack; break;
		case 2: EnemyChosenAction = EBattleAction::WhiteAttack; break;
		default: EnemyChosenAction = EBattleAction::Charge; break;
		}
	}
}

void UBattleComponent::StartPlayerExtraTurn()
{
	bPlayerChoseAction = false;
	SetPhase(EBattlePhase::ActionSelect);
	OnBattleStateChanged.Broadcast();
}

void UBattleComponent::StartEnemyExtraTurn()
{
	ChooseEnemyAction(true);
	SetPhase(EBattlePhase::Resolving);
	OnBattleStateChanged.Broadcast();
}

// ---- 以下方法在 Task 6 / Task 7 / Task 8 中替换为完整实现 ----

FTurnResolution UBattleComponent::ResolveNormalTurn(EBattleAction PlayerAction, EBattleAction EnemyAction)
{
	// TODO-TASK6
	return FTurnResolution();
}

FTurnResolution UBattleComponent::ResolveExtraTurn(bool bPlayerTurn, EBattleAction Action)
{
	// TODO-TASK6
	return FTurnResolution();
}

void UBattleComponent::ApplyResolution(const FTurnResolution& Resolution)
{
	// TODO-TASK6
}

void UBattleComponent::ApplyDamageTo(ABaseCharacter* Target, float Amount, AActor* Causer)
{
	// TODO-TASK6
}

void UBattleComponent::EndTurnAndAdvance()
{
	// TODO-TASK6
}

void UBattleComponent::StartClash(EClashType ClashType)
{
	// TODO-TASK7
}

void UBattleComponent::OpenClashWindow()
{
	// TODO-TASK7
}

void UBattleComponent::OnClashImpact()
{
	// TODO-TASK7
}

void UBattleComponent::ResolveClash(EClashResult Result)
{
	// TODO-TASK7
}

void UBattleComponent::ClearClashTimers()
{
	// TODO-TASK7
}

void UBattleComponent::OnRedDefensePressed()
{
	PlayerChooseAction(EBattleAction::RedDefense);
}

void UBattleComponent::OnBlueAttackPressed()
{
	PlayerChooseAction(EBattleAction::BlueAttack);
}

void UBattleComponent::OnWhiteAttackPressed()
{
	PlayerChooseAction(EBattleAction::WhiteAttack);
}

void UBattleComponent::OnChargePressed()
{
	PlayerChooseAction(EBattleAction::Charge);
}

void UBattleComponent::OnSkillPressed()
{
	PlayerChooseAction(EBattleAction::Skill);
}

void UBattleComponent::OnBlockPressed()
{
	// TODO-TASK7
}

void UBattleComponent::OnDodgePressed()
{
	// TODO-TASK7
}

void UBattleComponent::SetupCombatInput()
{
	// TODO-TASK8
}

void UBattleComponent::UnbindCombatInput()
{
	// TODO-TASK8
}

void UBattleComponent::AddCombatMapping()
{
	// TODO-TASK8
}

void UBattleComponent::RemoveCombatMapping()
{
	// TODO-TASK8
}

void UBattleComponent::PositionBattleActors()
{
	// TODO-TASK8
}

void UBattleComponent::RestoreExplorationState()
{
	// TODO-TASK8
}

void UBattleComponent::LockPlayer()
{
	// TODO-TASK8
}

void UBattleComponent::UnlockPlayer()
{
	// TODO-TASK8
}

void UBattleComponent::ShowHUD()
{
	// TODO-TASK8
}

void UBattleComponent::HideHUD()
{
	// TODO-TASK8
}

void UBattleComponent::FinishBattle(bool bPlayerWon)
{
	// TODO-TASK8
}

void UBattleComponent::HandleVictoryCleanup()
{
	// TODO-TASK8
}

void UBattleComponent::HandleDefeatRestart()
{
	// TODO-TASK8
}

void UBattleComponent::ResetForRetry()
{
	// TODO-TASK8
}

float UBattleComponent::GetPlayerWhiteDamage() const
{
	// TODO-TASK6
	return 0.0f;
}

float UBattleComponent::GetPlayerBlueDamage(int32 Stacks) const
{
	// TODO-TASK6
	return 0.0f;
}

float UBattleComponent::GetEnemyWhiteDamage() const
{
	// TODO-TASK6
	return 0.0f;
}

float UBattleComponent::GetEnemyBlueDamage(int32 Stacks) const
{
	// TODO-TASK6
	return 0.0f;
}

float UBattleComponent::GetPlayerGoldDamage() const
{
	// TODO-TASK6
	return 0.0f;
}

float UBattleComponent::GetEnemyGoldDamage() const
{
	// TODO-TASK6
	return 0.0f;
}

float UBattleComponent::GetChargeResistScale() const
{
	// TODO-TASK6
	return 0.3f;
}

int32 UBattleComponent::GetMaxChargeStacks(bool bPlayer) const
{
	// TODO-TASK6
	return 2;
}

float UBattleComponent::GetBlockWindow() const
{
	// TODO-TASK7
	return 0.25f;
}

float UBattleComponent::GetDodgeWindow() const
{
	// TODO-TASK7
	return 0.35f;
}

UCombatFormulaSubsystem* UBattleComponent::GetCombatSubsystem() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	UGameInstance* GameInstance = World->GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UCombatFormulaSubsystem>() : nullptr;
}

UAttributeComponent* UBattleComponent::GetPlayerAttr() const
{
	return PlayerRole.IsValid() ? PlayerRole->AttributeComponent : nullptr;
}

UAttributeComponent* UBattleComponent::GetEnemyAttr() const
{
	return BossEnemy.IsValid() ? BossEnemy->AttributeComponent : nullptr;
}

void UBattleComponent::SetPhase(EBattlePhase NewPhase)
{
	Phase = NewPhase;
}
```

- [ ] **Step 3: 编译**

运行构建命令。预期：构建成功（TODO 方法均有定义，链接无缺失）。

- [ ] **Step 4: 提交**

```bash
git add Hole/Source/Hole/Public/Combat/BattleComponent.h Hole/Source/Hole/Private/Combat/BattleComponent.cpp
git commit -m "feat(combat): add battle component skeleton and state machine"
```

---

### Task 6: 回合结算矩阵 + 伤害应用 + 额外回合

**Files:**
- Modify: `Hole/Source/Hole/Private/Combat/BattleComponent.cpp`

**Interfaces:**
- Consumes: `UCombatFormulaSubsystem::CalculateWhiteDamage(Attr, FlatBonus)`、`CalculateBlueDamage(Attr, FlatBonus, Stacks)`、`CalculateGoldDamage(Attr)`、`FCombatParamsRow::WhiteInterruptChargeDamageScale`、`AttributeNames::MaxChargeStacks()`。
- Produces: 完整 `ResolveNormalTurn` / `ResolveExtraTurn` / `ApplyResolution` / `ApplyDamageTo` / `EndTurnAndAdvance` 及全部伤害辅助函数。

- [ ] **Step 1: 替换 `ResolveNormalTurn` 的 TODO 实现**

把骨架中的 `ResolveNormalTurn` 函数体整体替换为：

```cpp
FTurnResolution UBattleComponent::ResolveNormalTurn(EBattleAction PlayerAction, EBattleAction EnemyAction)
{
	FTurnResolution R;
	const int32 MaxStacks = GetMaxChargeStacks(true);

	switch (PlayerAction)
	{
	case EBattleAction::RedDefense:
	{
		switch (EnemyAction)
		{
		case EBattleAction::RedDefense:
			// 红 vs 红：本回合跳过
			break;
		case EBattleAction::BlueAttack:
			// 红防克蓝攻 → 金色反击；敌方 2 层蓄力出蓝刀时红防正面承受强化蓝攻
			if (EnemyChargeStacks >= 2)
			{
				R.PlayerDamageTaken = GetEnemyBlueDamage(EnemyChargeStacks);
			}
			else
			{
				R.EnemyDamageTaken = GetPlayerGoldDamage();
			}
			EnemyChargeStacks = 0;
			break;
		case EBattleAction::WhiteAttack:
			// 红防被白克 → 全额受伤
			R.PlayerDamageTaken = GetEnemyWhiteDamage();
			break;
		case EBattleAction::Charge:
			// 敌方蓄力：1 层无事；蓄满 2 层视为直接发动强化蓝攻
		{
			const int32 NewStacks = FMath::Min(EnemyChargeStacks + 1, MaxStacks);
			if (NewStacks >= MaxStacks && MaxStacks >= 2)
			{
				R.PlayerDamageTaken = GetEnemyBlueDamage(NewStacks);
				EnemyChargeStacks = 0;
			}
			else
			{
				EnemyChargeStacks = NewStacks;
			}
			break;
		}
		default:
			break;
		}
		break;
	}
	case EBattleAction::BlueAttack:
	{
		switch (EnemyAction)
		{
		case EBattleAction::RedDefense:
			// 蓝攻被红防克制 → 玩家吃金色反击伤害
			R.PlayerDamageTaken = GetEnemyGoldDamage();
			PlayerChargeStacks = 0;
			break;
		case EBattleAction::BlueAttack:
			// 同色碰撞
			R.bClash = true;
			R.ClashType = EClashType::BlueClash;
			R.PlayerDamageTaken = GetEnemyBlueDamage(EnemyChargeStacks);
			R.EnemyDamageTaken = GetPlayerBlueDamage(PlayerChargeStacks);
			PlayerChargeStacks = 0;
			EnemyChargeStacks = 0;
			break;
		case EBattleAction::WhiteAttack:
			// 蓝克白 → 玩家优势
			R.EnemyDamageTaken = GetPlayerBlueDamage(PlayerChargeStacks);
			PlayerChargeStacks = 0;
			break;
		case EBattleAction::Charge:
			// 蓝攻打断蓄力，全额伤害
			R.EnemyDamageTaken = GetPlayerBlueDamage(PlayerChargeStacks);
			R.bEnemyChargeInterrupted = true;
			PlayerChargeStacks = 0;
			EnemyChargeStacks = 0;
			break;
		default:
			break;
		}
		break;
	}
	case EBattleAction::WhiteAttack:
	{
		switch (EnemyAction)
		{
		case EBattleAction::RedDefense:
			// 白克红 → 玩家优势
			R.EnemyDamageTaken = GetPlayerWhiteDamage();
			break;
		case EBattleAction::BlueAttack:
			// 白被蓝克 → 玩家吃全额蓝攻
			R.PlayerDamageTaken = GetEnemyBlueDamage(EnemyChargeStacks);
			EnemyChargeStacks = 0;
			break;
		case EBattleAction::WhiteAttack:
			// 同色碰撞
			R.bClash = true;
			R.ClashType = EClashType::WhiteClash;
			R.PlayerDamageTaken = GetEnemyWhiteDamage();
			R.EnemyDamageTaken = GetPlayerWhiteDamage();
			break;
		case EBattleAction::Charge:
			// 蓄力抵抗白攻：微量伤害 + 额外回合，不打断蓄力
			R.EnemyDamageTaken = GetPlayerWhiteDamage() * GetChargeResistScale();
			EnemyChargeStacks = FMath::Min(EnemyChargeStacks + 1, MaxStacks);
			R.bEnemyExtraTurn = true;
			break;
		default:
			break;
		}
		break;
	}
	case EBattleAction::Charge:
	{
		switch (EnemyAction)
		{
		case EBattleAction::RedDefense:
			// 玩家蓄力对红防：1 层无事；蓄满 2 层直接发动强化蓝攻
		{
			const int32 NewStacks = FMath::Min(PlayerChargeStacks + 1, MaxStacks);
			if (NewStacks >= MaxStacks && MaxStacks >= 2)
			{
				R.EnemyDamageTaken = GetPlayerBlueDamage(NewStacks);
				PlayerChargeStacks = 0;
			}
			else
			{
				PlayerChargeStacks = NewStacks;
			}
			break;
		}
		case EBattleAction::BlueAttack:
			// 蓄力被蓝攻打断，玩家吃全额蓝攻
			R.PlayerDamageTaken = GetEnemyBlueDamage(EnemyChargeStacks);
			R.bPlayerChargeInterrupted = true;
			PlayerChargeStacks = 0;
			EnemyChargeStacks = 0;
			break;
		case EBattleAction::WhiteAttack:
			// 蓄力抵抗白攻：微量伤害 + 额外回合
			R.PlayerDamageTaken = GetEnemyWhiteDamage() * GetChargeResistScale();
			PlayerChargeStacks = FMath::Min(PlayerChargeStacks + 1, MaxStacks);
			R.bPlayerExtraTurn = true;
			break;
		case EBattleAction::Charge:
			// 双方蓄力：都 +1 层，无事发生
			PlayerChargeStacks = FMath::Min(PlayerChargeStacks + 1, MaxStacks);
			EnemyChargeStacks = FMath::Min(EnemyChargeStacks + 1, MaxStacks);
			break;
		default:
			break;
		}
		break;
	}
	default:
		break;
	}

	return R;
}
```

- [ ] **Step 2: 替换 `ResolveExtraTurn` 的 TODO 实现**

```cpp
FTurnResolution UBattleComponent::ResolveExtraTurn(bool bPlayerTurn, EBattleAction Action)
{
	FTurnResolution R;

	if (Action == EBattleAction::BlueAttack)
	{
		// 额外回合出蓝刀：必定命中，出刀后解除蓄力
		if (bPlayerTurn)
		{
			R.EnemyDamageTaken = GetPlayerBlueDamage(PlayerChargeStacks);
			PlayerChargeStacks = 0;
		}
		else
		{
			R.PlayerDamageTaken = GetEnemyBlueDamage(EnemyChargeStacks);
			EnemyChargeStacks = 0;
		}
	}
	else if (Action == EBattleAction::Charge)
	{
		const int32 MaxStacks = GetMaxChargeStacks(bPlayerTurn);
		if (bPlayerTurn)
		{
			PlayerChargeStacks = FMath::Min(PlayerChargeStacks + 1, MaxStacks);
		}
		else
		{
			EnemyChargeStacks = FMath::Min(EnemyChargeStacks + 1, MaxStacks);
		}
	}

	return R;
}
```

- [ ] **Step 3: 替换 `ApplyResolution` / `ApplyDamageTo` / `EndTurnAndAdvance` 的 TODO 实现**

```cpp
void UBattleComponent::ApplyResolution(const FTurnResolution& Resolution)
{
	bClashStarted = false;
	bPlayerExtraTurnPending = Resolution.bPlayerExtraTurn;
	bEnemyExtraTurnPending = Resolution.bEnemyExtraTurn;

	if (Resolution.bClash)
	{
		PendingIncomingDamage = Resolution.PlayerDamageTaken;
		PendingOutgoingDamage = Resolution.EnemyDamageTaken;

		// 同色碰撞：玩家这刀先命中敌人，敌人这刀由实时防御决定
		if (Resolution.EnemyDamageTaken > 0.0f)
		{
			ApplyDamageTo(BossEnemy.Get(), Resolution.EnemyDamageTaken, PlayerRole.Get());
		}
		if (Phase != EBattlePhase::Ended)
		{
			StartClash(Resolution.ClashType);
			bClashStarted = true;
		}
		return;
	}

	if (Resolution.PlayerDamageTaken > 0.0f)
	{
		ApplyDamageTo(PlayerRole.Get(), Resolution.PlayerDamageTaken, BossEnemy.Get());
	}
	if (Resolution.EnemyDamageTaken > 0.0f)
	{
		ApplyDamageTo(BossEnemy.Get(), Resolution.EnemyDamageTaken, PlayerRole.Get());
	}
}

void UBattleComponent::ApplyDamageTo(ABaseCharacter* Target, float Amount, AActor* Causer)
{
	if (!Target || Amount <= 0.0f || Target->IsDead())
	{
		return;
	}

	Target->ReceiveDamage(Amount, Causer);
	OnBattleStateChanged.Broadcast();

	if (Target->IsDead())
	{
		FinishBattle(Target == BossEnemy.Get());
	}
}

void UBattleComponent::EndTurnAndAdvance()
{
	if (Phase == EBattlePhase::Ended)
	{
		return;
	}

	// 回合结束：buff 倒计时（含闪避Buff）
	if (UAttributeComponent* PA = GetPlayerAttr())
	{
		PA->TickTurn();
	}
	if (UAttributeComponent* EA = GetEnemyAttr())
	{
		EA->TickTurn();
	}

	if (bPlayerExtraTurnPending)
	{
		StartPlayerExtraTurn();
		return;
	}
	if (bEnemyExtraTurnPending)
	{
		bEnemyExtraTurnPending = false;
		StartEnemyExtraTurn();
		SetPhase(EBattlePhase::Resolving);
		const FTurnResolution Resolution = ResolveExtraTurn(false, EnemyChosenAction);
		ApplyResolution(Resolution);
		if (!bClashStarted)
		{
			EndTurnAndAdvance();
		}
		return;
	}

	StartNewRound();
}
```

- [ ] **Step 4: 替换 6 个伤害辅助函数 + `GetChargeResistScale` + `GetMaxChargeStacks` 的 TODO 实现**

```cpp
float UBattleComponent::GetPlayerWhiteDamage() const
{
	UCombatFormulaSubsystem* Subsystem = GetCombatSubsystem();
	return Subsystem ? Subsystem->CalculateWhiteDamage(GetPlayerAttr(), 0.0f) : 0.0f;
}

float UBattleComponent::GetPlayerBlueDamage(int32 Stacks) const
{
	UCombatFormulaSubsystem* Subsystem = GetCombatSubsystem();
	return Subsystem ? Subsystem->CalculateBlueDamage(GetPlayerAttr(), 0.0f, Stacks) : 0.0f;
}

float UBattleComponent::GetEnemyWhiteDamage() const
{
	UCombatFormulaSubsystem* Subsystem = GetCombatSubsystem();
	return Subsystem ? Subsystem->CalculateWhiteDamage(GetEnemyAttr(), 0.0f) : 0.0f;
}

float UBattleComponent::GetEnemyBlueDamage(int32 Stacks) const
{
	UCombatFormulaSubsystem* Subsystem = GetCombatSubsystem();
	return Subsystem ? Subsystem->CalculateBlueDamage(GetEnemyAttr(), 0.0f, Stacks) : 0.0f;
}

float UBattleComponent::GetPlayerGoldDamage() const
{
	UCombatFormulaSubsystem* Subsystem = GetCombatSubsystem();
	return Subsystem ? Subsystem->CalculateGoldDamage(GetPlayerAttr()) : 0.0f;
}

float UBattleComponent::GetEnemyGoldDamage() const
{
	UCombatFormulaSubsystem* Subsystem = GetCombatSubsystem();
	return Subsystem ? Subsystem->CalculateGoldDamage(GetEnemyAttr()) : 0.0f;
}

float UBattleComponent::GetChargeResistScale() const
{
	UCombatFormulaSubsystem* Subsystem = GetCombatSubsystem();
	if (!Subsystem)
	{
		return 0.3f;
	}
	const FCombatParamsRow Defaults;
	const FCombatParamsRow* Params = Subsystem->GetCombatParams();
	return Params ? Params->WhiteInterruptChargeDamageScale : Defaults.WhiteInterruptChargeDamageScale;
}

int32 UBattleComponent::GetMaxChargeStacks(bool bPlayer) const
{
	UAttributeComponent* Attr = bPlayer ? GetPlayerAttr() : GetEnemyAttr();
	if (Attr)
	{
		return FMath::Max(1, FMath::RoundToInt(Attr->GetFinal(AttributeNames::MaxChargeStacks())));
	}
	return 2;
}
```

- [ ] **Step 5: 编译**

运行构建命令。预期：构建成功，0 error。

- [ ] **Step 6: 提交**

```bash
git add Hole/Source/Hole/Private/Combat/BattleComponent.cpp
git commit -m "feat(combat): implement turn resolution matrix, damage and extra turns"
```

---

### Task 7: 同色碰撞实时对抗（格挡/闪避）

**Files:**
- Modify: `Hole/Source/Hole/Private/Combat/BattleComponent.cpp`

**Interfaces:**
- Consumes: `AttributeNames::BlockWindow()` / `DodgeWindow()` / `DodgeFailDamageScale`（经 `DT_CombatParams`）、`DodgeBuffDamageScale` / `DodgeBuffTurns`。
- Produces: `StartClash` / `OpenClashWindow` / `OnClashImpact` / `ResolveClash` / `ClearClashTimers` / `OnBlockPressed` / `OnDodgePressed` / `GetBlockWindow` / `GetDodgeWindow`。

- [ ] **Step 1: 替换 `StartClash` / `OpenClashWindow` / `OnClashImpact` / `ResolveClash` / `ClearClashTimers` 的 TODO 实现**

```cpp
void UBattleComponent::StartClash(EClashType ClashType)
{
	ActiveClashType = ClashType;
	bClashResolved = false;
	bClashWindowOpen = false;
	PendingClashResult = EClashResult::None;

	SetPhase(EBattlePhase::Clash);

	const float BlockWindow = GetBlockWindow();
	const float DodgeWindow = GetDodgeWindow();
	const float OpenDelay = FMath::Max(0.0f, ClashTelegraphTime - FMath::Max(BlockWindow, DodgeWindow));

	GetWorld()->GetTimerManager().SetTimer(ClashOpenTimer, this, &UBattleComponent::OpenClashWindow, OpenDelay, false);
	GetWorld()->GetTimerManager().SetTimer(ClashImpactTimer, this, &UBattleComponent::OnClashImpact, ClashTelegraphTime, false);

	if (CombatHUD.IsValid())
	{
		const FText Prompt = (ClashType == EClashType::BlueClash)
			? FText::FromString(TEXT("蓝色碰撞！格挡(E) / 闪避(Shift)"))
			: FText::FromString(TEXT("白色碰撞！格挡(E) / 闪避(Shift)"));
		CombatHUD->ShowClashPrompt(Prompt, ClashTelegraphTime);
	}

	OnBattleStateChanged.Broadcast();
}

void UBattleComponent::OpenClashWindow()
{
	bClashWindowOpen = true;
}

void UBattleComponent::OnClashImpact()
{
	if (Phase != EBattlePhase::Clash || bClashResolved)
	{
		return;
	}

	EClashResult Result = PendingClashResult;
	if (Result == EClashResult::None)
	{
		// 未按任何键 → 按格挡失败处理（全额伤害）
		Result = EClashResult::BlockFail;
	}
	ResolveClash(Result);
}

void UBattleComponent::ResolveClash(EClashResult Result)
{
	if (bClashResolved)
	{
		return;
	}
	bClashResolved = true;
	ClearClashTimers();

	float Incoming = PendingIncomingDamage;

	switch (Result)
	{
	case EClashResult::BlockSuccess:
		Incoming = 0.0f;
		if (BossEnemy.IsValid())
		{
			ApplyDamageTo(BossEnemy.Get(), GetPlayerGoldDamage(), PlayerRole.Get());
		}
		break;
	case EClashResult::DodgeSuccess:
		Incoming = 0.0f;
		if (UAttributeComponent* PA = GetPlayerAttr())
		{
			UCombatFormulaSubsystem* Subsystem = GetCombatSubsystem();
			const FCombatParamsRow Defaults;
			const FCombatParamsRow* Params = Subsystem ? Subsystem->GetCombatParams() : nullptr;
			const FCombatParamsRow& P = Params ? *Params : Defaults;
			PA->AddModifier(
				AttributeNames::NextAttackDamageScale(),
				EModifierOp::Multiply,
				P.DodgeBuffDamageScale,
				P.DodgeBuffTurns,
				FName(TEXT("DodgeBuff")));
		}
		break;
	case EClashResult::DodgeFail:
	{
		UCombatFormulaSubsystem* Subsystem = GetCombatSubsystem();
		const FCombatParamsRow Defaults;
		const FCombatParamsRow* Params = Subsystem ? Subsystem->GetCombatParams() : nullptr;
		const FCombatParamsRow& P = Params ? *Params : Defaults;
		Incoming *= P.DodgeFailDamageScale;
		break;
	}
	case EClashResult::BlockFail:
	default:
		// 全额伤害
		break;
	}

	if (CombatHUD.IsValid())
	{
		CombatHUD->HideClashPrompt();
	}

	if (Incoming > 0.0f)
	{
		ApplyDamageTo(PlayerRole.Get(), Incoming, BossEnemy.Get());
	}

	if (Phase != EBattlePhase::Ended)
	{
		EndTurnAndAdvance();
	}
}

void UBattleComponent::ClearClashTimers()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ClashOpenTimer);
		GetWorld()->GetTimerManager().ClearTimer(ClashImpactTimer);
	}
	bClashWindowOpen = false;
}
```

- [ ] **Step 2: 替换 `OnBlockPressed` / `OnDodgePressed` 的 TODO 实现**

```cpp
void UBattleComponent::OnBlockPressed()
{
	if (Phase != EBattlePhase::Clash || bClashResolved)
	{
		return;
	}
	if (bClashWindowOpen)
	{
		ResolveClash(EClashResult::BlockSuccess);
		return;
	}
	// 窗口外按下：先记录失败结果，窗口内再按可覆盖
	PendingClashResult = EClashResult::BlockFail;
}

void UBattleComponent::OnDodgePressed()
{
	if (Phase != EBattlePhase::Clash || bClashResolved)
	{
		return;
	}
	if (bClashWindowOpen)
	{
		ResolveClash(EClashResult::DodgeSuccess);
		return;
	}
	PendingClashResult = EClashResult::DodgeFail;
}
```

- [ ] **Step 3: 替换 `GetBlockWindow` / `GetDodgeWindow` 的 TODO 实现**

```cpp
float UBattleComponent::GetBlockWindow() const
{
	UAttributeComponent* Attr = GetPlayerAttr();
	if (Attr)
	{
		return Attr->GetFinal(AttributeNames::BlockWindow());
	}
	return 0.25f;
}

float UBattleComponent::GetDodgeWindow() const
{
	UAttributeComponent* Attr = GetPlayerAttr();
	if (Attr)
	{
		return Attr->GetFinal(AttributeNames::DodgeWindow());
	}
	return 0.35f;
}
```

- [ ] **Step 4: 编译**

运行构建命令。预期：构建成功，0 error。

- [ ] **Step 5: 提交**

```bash
git add Hole/Source/Hole/Private/Combat/BattleComponent.cpp
git commit -m "feat(combat): implement same-color clash with block and dodge windows"
```

---

### Task 8: 站位/镜头/HUD/输入 + 战斗结束与失败重开

**Files:**
- Modify: `Hole/Source/Hole/Private/Combat/BattleComponent.cpp`

**Interfaces:**
- Consumes: `ARole::SpringArm` / `SetCinematicLocked`、`UBossIntroComponent::TriggerSphere::UpdateOverlaps()`、`CreateWidget<UCombatHUDWidget>`。
- Produces: `PositionBattleActors` / `RestoreExplorationState` / `LockPlayer` / `UnlockPlayer` / `ShowHUD` / `HideHUD` / `SetupCombatInput` / `UnbindCombatInput` / `AddCombatMapping` / `RemoveCombatMapping` / `FinishBattle` / `HandleVictoryCleanup` / `HandleDefeatRestart` / `ResetForRetry`。

- [ ] **Step 1: 替换站位/镜头相关 TODO**

```cpp
void UBattleComponent::PositionBattleActors()
{
	ARole* Role = PlayerRole.Get();
	AEnemy* Boss = BossEnemy.Get();
	if (!Role || !Boss)
	{
		return;
	}

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;

	// 保存探索状态（战斗结束后恢复）
	PlayerStartLocation = Role->GetActorLocation();
	PlayerStartActorRotation = Role->GetActorRotation();
	BossStartLocation = Boss->GetActorLocation();
	BossStartRotation = Boss->GetActorRotation();
	if (PC)
	{
		PlayerStartControlRotation = PC->GetControlRotation();
	}
	if (Role->SpringArm)
	{
		OriginalArmLength = Role->SpringArm->TargetArmLength;
		bOriginalCameraLag = Role->SpringArm->bEnableCameraRotationLag;
		OriginalLagSpeed = Role->SpringArm->CameraRotationLagSpeed;
	}

	// Boss 面向玩家
	FVector DirToPlayer = PlayerStartLocation - BossStartLocation;
	DirToPlayer.Z = 0.0f;
	if (!DirToPlayer.IsNearlyZero())
	{
		DirToPlayer.Normalize();
	}
	Boss->SetActorRotation(FRotator(0.0f, DirToPlayer.Rotation().Yaw, 0.0f));

	// 玩家站到 Boss 正前方固定距离（固定位移）
	const FVector PlayerSpot = BossStartLocation - DirToPlayer * BattleDistance;
	Role->SetActorLocation(
		FVector(PlayerSpot.X, PlayerSpot.Y, BossStartLocation.Z),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	// 玩家面向 Boss（固定角度）
	const FRotator PlayerFaceRot = (BossStartLocation - Role->GetActorLocation()).Rotation();
	Role->SetActorRotation(FRotator(0.0f, PlayerFaceRot.Yaw, 0.0f));

	// 固定玩家摄像机：锁定控制旋转 + 固定 SpringArm 长度/关闭滞后
	if (PC)
	{
		PC->SetControlRotation(FRotator(BattleCameraPitch, PlayerFaceRot.Yaw, 0.0f));
	}
	if (Role->SpringArm)
	{
		Role->SpringArm->TargetArmLength = BattleCameraArmLength;
		Role->SpringArm->bEnableCameraRotationLag = false;
	}
}

void UBattleComponent::RestoreExplorationState()
{
	ARole* Role = PlayerRole.Get();
	AEnemy* Boss = BossEnemy.Get();

	if (Role)
	{
		Role->SetActorLocation(PlayerStartLocation, false, nullptr, ETeleportType::TeleportPhysics);
		Role->SetActorRotation(PlayerStartActorRotation);
		if (Role->SpringArm)
		{
			Role->SpringArm->TargetArmLength = OriginalArmLength;
			Role->SpringArm->bEnableCameraRotationLag = bOriginalCameraLag;
			Role->SpringArm->CameraRotationLagSpeed = OriginalLagSpeed;
		}
	}
	if (Boss)
	{
		Boss->SetActorLocation(BossStartLocation, false, nullptr, ETeleportType::TeleportPhysics);
		Boss->SetActorRotation(BossStartRotation);
	}
	if (GetWorld())
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			PC->SetControlRotation(PlayerStartControlRotation);
		}
	}
}

void UBattleComponent::LockPlayer()
{
	if (PlayerRole.IsValid())
	{
		PlayerRole->SetCinematicLocked(true);
	}
}

void UBattleComponent::UnlockPlayer()
{
	if (PlayerRole.IsValid())
	{
		PlayerRole->SetCinematicLocked(false);
	}
}
```

- [ ] **Step 2: 替换 HUD 相关 TODO**

```cpp
void UBattleComponent::ShowHUD()
{
	if (CombatHUD.IsValid() || !CombatHUDClass)
	{
		return;
	}

	CombatHUD = CreateWidget<UCombatHUDWidget>(GetWorld(), CombatHUDClass);
	if (!CombatHUD.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UBattleComponent::ShowHUD - 创建 WBP_CombatHUD 失败"));
		return;
	}

	CombatHUD->AddToViewport(10);
	CombatHUD->BindToBattle(this);

	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
		bMouseCursorShown = true;
	}
}

void UBattleComponent::HideHUD()
{
	if (CombatHUD.IsValid())
	{
		CombatHUD->RemoveFromParent();
		CombatHUD.Reset();
	}

	if (bMouseCursorShown)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			PC->bShowMouseCursor = false;
			PC->SetInputMode(FInputModeGameOnly());
		}
		bMouseCursorShown = false;
	}
}
```

- [ ] **Step 3: 替换输入相关 TODO**

```cpp
void UBattleComponent::SetupCombatInput()
{
	if (bCombatInputBound || !PlayerRole.IsValid())
	{
		return;
	}

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerRole->InputComponent);
	if (!EIC)
	{
		return;
	}

	if (RedDefenseAction) EIC->BindAction(RedDefenseAction, ETriggerEvent::Started, this, &UBattleComponent::OnRedDefensePressed);
	if (BlueAttackAction) EIC->BindAction(BlueAttackAction, ETriggerEvent::Started, this, &UBattleComponent::OnBlueAttackPressed);
	if (WhiteAttackAction) EIC->BindAction(WhiteAttackAction, ETriggerEvent::Started, this, &UBattleComponent::OnWhiteAttackPressed);
	if (ChargeAction) EIC->BindAction(ChargeAction, ETriggerEvent::Started, this, &UBattleComponent::OnChargePressed);
	if (SkillAction) EIC->BindAction(SkillAction, ETriggerEvent::Started, this, &UBattleComponent::OnSkillPressed);
	if (BlockAction) EIC->BindAction(BlockAction, ETriggerEvent::Started, this, &UBattleComponent::OnBlockPressed);
	if (DodgeAction) EIC->BindAction(DodgeAction, ETriggerEvent::Started, this, &UBattleComponent::OnDodgePressed);

	bCombatInputBound = true;
}

void UBattleComponent::UnbindCombatInput()
{
	if (!bCombatInputBound || !PlayerRole.IsValid())
	{
		return;
	}

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerRole->InputComponent))
	{
		EIC->ClearBindingsForObject(this);
	}
	bCombatInputBound = false;
}

void UBattleComponent::AddCombatMapping()
{
	if (bCombatMappingAdded || !CombatMappingContext)
	{
		return;
	}

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->GetLocalPlayer())
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		// 优先级 10 > 探索 IMC 的 0，战斗期间 Shift 等键被战斗映射覆盖
		Subsystem->AddMappingContext(CombatMappingContext, 10);
		bCombatMappingAdded = true;
	}
}

void UBattleComponent::RemoveCombatMapping()
{
	if (!bCombatMappingAdded || !CombatMappingContext)
	{
		return;
	}

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (PC && PC->GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->RemoveMappingContext(CombatMappingContext);
		}
	}
	bCombatMappingAdded = false;
}
```

- [ ] **Step 4: 替换结束流程 TODO（胜利/失败/重开）**

```cpp
void UBattleComponent::FinishBattle(bool bPlayerWon)
{
	if (Phase == EBattlePhase::Ended)
	{
		return;
	}

	SetPhase(EBattlePhase::Ended);
	ClearClashTimers();
	UnbindCombatInput();
	RemoveCombatMapping();
	if (CombatHUD.IsValid())
	{
		CombatHUD->HideClashPrompt();
		CombatHUD->ShowResult(bPlayerWon ? FText::FromString(TEXT("战斗胜利")) : FText::FromString(TEXT("战斗失败")));
	}
	OnBattleStateChanged.Broadcast();

	const float Delay = bPlayerWon ? 2.0f : DefeatRestartDelay;
	if (bPlayerWon)
	{
		GetWorld()->GetTimerManager().SetTimer(EndDelayTimer, this, &UBattleComponent::HandleVictoryCleanup, Delay, false);
	}
	else
	{
		GetWorld()->GetTimerManager().SetTimer(EndDelayTimer, this, &UBattleComponent::HandleDefeatRestart, Delay, false);
	}
}

void UBattleComponent::HandleVictoryCleanup()
{
	bBossDefeated = true;
	HideHUD();
	RestoreExplorationState();
	UnlockPlayer();
	SetPhase(EBattlePhase::Idle);
	OnBattleStateChanged.Broadcast();
}

void UBattleComponent::HandleDefeatRestart()
{
	HideHUD();
	RestoreExplorationState();
	UnlockPlayer();
	ResetForRetry();

	// 回到 Boss 触发点：玩家已恢复为战斗前位置（触发球内），重置触发后强制刷新重叠 → 直接重播 Boss 动画
	if (BossEnemy.IsValid())
	{
		if (UBossIntroComponent* Intro = BossEnemy->FindComponentByClass<UBossIntroComponent>())
		{
			Intro->ResetIntro();
			Intro->TriggerSphere->UpdateOverlaps();
		}
	}

	SetPhase(EBattlePhase::Idle);
	OnBattleStateChanged.Broadcast();
}

void UBattleComponent::ResetForRetry()
{
	if (PlayerRole.IsValid())
	{
		PlayerRole->CurrentHealth = PlayerRole->GetMaxHealth();
		if (UAttributeComponent* PA = GetPlayerAttr())
		{
			PA->RemoveAllTemporaryModifiers();
		}
	}
	if (BossEnemy.IsValid())
	{
		BossEnemy->CurrentHealth = BossEnemy->GetMaxHealth();
		if (UAttributeComponent* EA = GetEnemyAttr())
		{
			EA->RemoveAllTemporaryModifiers();
		}
	}

	PlayerChargeStacks = 0;
	EnemyChargeStacks = 0;
	RoundNumber = 0;
	bPlayerChoseAction = false;
	bPlayerExtraTurnPending = false;
	bEnemyExtraTurnPending = false;
	bClashResolved = false;
	bClashWindowOpen = false;
	PendingClashResult = EClashResult::None;
}
```

- [ ] **Step 5: 编译**

运行构建命令。预期：构建成功，0 error；确认无 `TODO-TASK` 残留（`rg "TODO-TASK" Hole/Source` 应无输出）。

- [ ] **Step 6: 提交**

```bash
git add Hole/Source/Hole/Private/Combat/BattleComponent.cpp
git commit -m "feat(combat): wire battle positioning, camera, HUD, input and end states"
```

---

### Task 9: ARole::Look 战斗锁定门控

**Files:**
- Modify: `Hole/Source/Hole/Private/Character/Role.cpp`

**Interfaces:**
- Consumes: 现有 `bCinematicLocked` 成员。

- [ ] **Step 1: 修改 `ARole::Look`**

把：

```cpp
void ARole::Look(const FInputActionValue& Value)
{
	const FVector2D LookVector = Value.Get<FVector2D>();
```

改为：

```cpp
void ARole::Look(const FInputActionValue& Value)
{
	// 战斗/过场锁定期间禁止转动视角，避免破坏固定战斗机位
	if (bCinematicLocked)
	{
		return;
	}

	const FVector2D LookVector = Value.Get<FVector2D>();
```

- [ ] **Step 2: 编译**

运行构建命令。预期：构建成功。

- [ ] **Step 3: 提交**

```bash
git add Hole/Source/Hole/Private/Character/Role.cpp
git commit -m "feat(combat): gate camera look while cinematic/battle locked"
```

---

### Task 10: 编辑器资产与蓝图接线

**Files（全部在 UE 编辑器中操作，保存后提交 .uasset）：**
- Create: `Hole/Content/Input/IA_CombatRedDefense.uasset`、`IA_CombatBlueAttack.uasset`、`IA_CombatWhiteAttack.uasset`、`IA_CombatCharge.uasset`、`IA_CombatSkill.uasset`、`IA_CombatBlock.uasset`、`IA_CombatDodge.uasset`、`IMC_Combat.uasset`
- Create: `Hole/Content/UI/WBP_CombatHUD.uasset`
- Modify: `Hole/Content/Blueprint/Character/Roles/Dale/BP_Dale.uasset`（挂 `UBattleComponent`）
- Modify: `Hole/Content/Blueprint/Character/Enemies/Satan/BP_Satan.uasset`（Tag `Boss` + 挂 `UEnemyCombatAIComponent`）

**Interfaces:**
- 输入动作键位：红防=1、蓝攻=2、白攻=3、蓄力=4、技能=5、格挡=E、闪避=Left Shift。

- [ ] **Step 1: 创建 2 个 InputAction**

1. 打开 Content Browser，进入 `Content/Input`。
2. 右键 → Input → Input Action，命名 `IA_CombatBlock`；详情面板 Value Type = Digital (bool)。
3. 重复创建：`IA_CombatDodge`（Digital bool）。
（行动选择红/蓝/白/蓄力/技能只通过 HUD 按钮点击，不创建输入资产。）

- [ ] **Step 2: 创建 IMC_Combat 并配置映射**

1. `Content/Input` 内右键 → Input → Input Mapping Context，命名 `IMC_Combat`。
2. 打开 IMC_Combat → Mappings 点击 `+`，依次添加：
   - `IA_CombatBlock` → 键盘 E
   - `IA_CombatDodge` → 键盘 Left Shift
3. 保存。

- [ ] **Step 3: 创建 WBP_CombatHUD**

1. `Content` 下新建文件夹 `UI`。
2. 在 `Content/HUD` 右键 → User Interface → Widget Blueprint；Parent Class 搜索 `CombatHUDWidget` 选择 `UCombatHUDWidget`；命名 `WBP_CombatHUD`（已创建，需补全控件树）。
3. 打开编辑器，按以下控件树搭建（**控件名必须与 C++ BindWidget 完全一致**，否则编译报错）：

```
Root [Canvas Panel]
├─ PlayerPanel [Vertical Box]（左上角锚点）
│  ├─ PlayerNameText [Text Block]
│  ├─ PlayerHealthBar [Progress Bar]
│  └─ PlayerChargeText [Text Block]
├─ EnemyPanel [Vertical Box]（右上角锚点）
│  ├─ EnemyNameText [Text Block]
│  ├─ EnemyHealthBar [Progress Bar]
│  └─ EnemyChargeText [Text Block]
├─ RoundText [Text Block]（顶部居中）
├─ ActionPanel [Horizontal Box]（底部居中）
│  ├─ RedDefenseButton [Button] → 子文本 "红色防御"
│  ├─ BlueAttackButton [Button] → 子文本 "蓝色攻击"
│  ├─ WhiteAttackButton [Button] → 子文本 "白色攻击"
│  ├─ ChargeButton [Button] → 子文本 "蓄力"
│  └─ SkillButton [Button] → 子文本 "技能(未实装)"
├─ ClashPanel [Vertical Box]（居中，默认 Collapsed）
│  ├─ ClashPromptText [Text Block]
│  └─ ClashWindowBar [Progress Bar]
└─ ResultPanel [Vertical Box]（居中，默认 Collapsed）
   └─ ResultText [Text Block]
```

4. 点击 Compile：预期无 BindWidget 缺失错误（C++ 已编译的前提下）。
5. 保存。

- [ ] **Step 3.5: 核对 DT_BattleStage（已由 create_datatables.py 生成）**

1. 打开 `Content/DataTable/DT_BattleStage`，确认结构体为 `CombatStageRow`、存在 `Default` 行。
2. 保持默认值（PlayerBattleOffset=(0,-550,0)、bBossFacePlayer=true、bPlayerFaceBoss=true、CameraPitch=-12、CameraYawOffset=0、CameraArmLength=400），策划后续直接在此表调参。
3. 保存。

- [ ] **Step 4: BP_Dale 挂载 UBattleComponent**

1. 打开 `BP_Dale`（Content/Blueprint/Character/Roles/Dale）。
2. Components 面板 → `+ Add` → 搜索 `BattleComponent`（ClassGroup=Combat）→ 添加。
3. 选中新组件，详情面板确认（构造函数已自动填充，只需核对）：
   - `CombatHUDClass` = WBP_CombatHUD
   - `CombatMappingContext` = IMC_Combat
   - `BlockAction` = IA_CombatBlock、`DodgeAction` = IA_CombatDodge
4. 保存。

- [ ] **Step 5: BP_Satan 加 Tag 与 AI 组件**

1. 打开 `BP_Satan`（Content/Blueprint/Character/Enemies/Satan）。
2. Class Defaults → Tags → `+` → 输入 `Boss`（与 `UBattleComponent::FindBossEnemy` 的查找一致）。
3. Components 面板 → `+ Add` → 搜索 `EnemyCombatAIComponent` → 添加。
4. 确认原有 `BossIntroComponent` 及其 `LS_SatanIntro` / `IA_Skip` 配置保持不变。
5. 保存。

- [ ] **Step 6: PIE 冒烟测试**

1. 打开 `Content/Map/Untitled`，PIE 运行。
2. 走到 Satan 触发球内 → Boss 动画播放（可跳过）→ 战斗 HUD 出现。
3. 预期：玩家被传送到 Boss 正前方固定距离、镜头固定、鼠标可见、移动被锁定。
4. 预期：`Output Log` 无 `未找到 Tag=Boss` 等警告。
5. 退出 PIE。

- [ ] **Step 7: 提交**

```bash
git add Hole/Content/Input Hole/Content/HUD Hole/Content/Blueprint/Character/Roles/Dale/BP_Dale.uasset Hole/Content/Blueprint/Character/Enemies/Satan/BP_Satan.uasset
git commit -m "feat(combat): add combat input, HUD widget, and blueprint wiring"
```

---

### Task 11: 全流程验证 + 文档同步

**Files:**
- Modify: `DevLog.md`
- Modify: `GDD_Outline.md`（如需记录实现口径）
- Modify: `AGENTS.md`

- [ ] **Step 1: PIE 验证清单（逐项打勾）**

战斗进入：
- [ ] 靠近 Satan 触发 `LS_SatanIntro`，动画播放、跳过键可用
- [ ] 动画结束/跳过 → 自动进入战斗，HUD 出现
- [ ] 玩家/Boss 固定站位与角度，摄像机固定，视角不可转动，移动/跳跃/疾跑被锁
- [ ] 鼠标光标显示，按钮可点击，键盘 1/2/3/4 可触发行动

回合结算（用 `SetEnemyForcedAction` 调试接口固定敌人行动，逐项验证 16 种组合）：
- [ ] 红vs红：无事件，进入下一回合
- [ ] 红vs蓝：金色反击伤害（Log 校验数值区间 25~35×(1+CounterDmgBonus)）
- [ ] 红vs白：玩家全额吃白攻
- [ ] 红vs蓄力：1 层无事；2 层吃强化蓝攻
- [ ] 蓝vs红：玩家吃金色反击；蓝攻蓄力层数清零
- [ ] 蓝vs蓝：进入蓝色碰撞
- [ ] 蓝vs白：敌人吃全额蓝攻
- [ ] 蓝vs蓄力：敌人吃全额蓝攻且蓄力清零
- [ ] 白vs红：敌人吃全额白攻
- [ ] 白vs蓝：玩家吃全额蓝攻
- [ ] 白vs白：进入白色碰撞
- [ ] 白vs蓄力：敌人吃 ×0.3 微量白攻、蓄力+1、获得额外回合
- [ ] 蓄力vs红：1 层无事；2 层敌人吃强化蓝攻
- [ ] 蓄力vs蓝：玩家吃全额蓝攻、蓄力清零
- [ ] 蓄力vs白：玩家吃 ×0.3 微量白攻、蓄力+1、获得额外回合
- [ ] 蓄力vs蓄力：双方蓄力 +1

额外回合：
- [ ] 额外回合中红/白/技能按钮禁用，仅蓝攻/蓄力可用
- [ ] 额外回合蓝攻必定命中且不触发反击/碰撞

同色碰撞：
- [ ] 提示条出现并倒计时
- [ ] 窗口内按 E → 格挡成功：不受伤害 + 金色反击
- [ ] 窗口内按 Shift → 闪避成功：不受伤害 + 下回合伤害 ×1.2（Log 可查）
- [ ] 不按键 → 全额伤害；窗口外按 Shift → ×1.2 惩罚伤害
- [ ] 碰撞结束后 HUD 提示条消失

结束与重开：
- [ ] 敌人 HP=0（可用 `DebugSetEnemyHealth(1)` 后攻击验证）→ 胜利横幅 → 相机/站位/输入恢复 → 不再触发 Boss 动画
- [ ] 玩家 HP=0（`DebugSetPlayerHealth(1)`）→ 失败横幅 → 约 2 秒后回到 Boss 触发点 → Boss 动画直接重播 → 双方满血进入新战斗
- [ ] 全流程无崩溃，Output Log 无异常

- [ ] **Step 2: 更新 DevLog.md**

追加一条 `### 2026-08-02 | 程序 ⚡`（如含策划决策再加 `策划` 条目），记录：战斗系统 v1 落地（BattleComponent/EnemyCombatAIComponent/CombatHUDWidget）、16 项结算矩阵、同色碰撞窗口、失败回到 Boss 触发重开、v1 敌人 AI 全随机 [PLAYTEST]。

- [ ] **Step 3: 更新 GDD_Outline.md（如需）**

若验证中发现规则口径需要补充（例如“同色碰撞中双方攻击同时命中、防御只修正玩家承伤”），在 §5.2.5 补一句并 bump 版本号；无变更则跳过。

- [ ] **Step 4: 更新 AGENTS.md**

在“Data Architecture Principles”后新增一节“Battle System Architecture”，用英文记录：`UBattleComponent`（会话状态机/不写公式）、`UEnemyCombatAIComponent`（v1 random）、`UCombatHUDWidget`（C++ 基类 + BP 皮肤）、BossIntro → `OnIntroFinished` → `StartBattle` 链路、失败重开口径。

- [ ] **Step 5: 最终提交**

```bash
git add DevLog.md GDD_Outline.md AGENTS.md
git commit -m "docs(combat): record combat v1 implementation and decisions"
```

---

## 执行方式选择

计划书已完成并保存到 `Plans/combat-system.md`。两种执行方式：

1. **Subagent-Driven（推荐）**：每个 Task 派一个全新 subagent 执行，任务间做两阶段审查，迭代快、上下文干净。
2. **Inline Execution**：在当前会话按 executing-plans 批量执行，设检查点供用户审查。
