# 开发流程记录（Development Log）

> **项目：** Hole（洞穴）
> **引擎：** Unreal Engine 5.6
> **起始日期：** 2026-07-06
> **用途：** 记录开发过程中的关键决策、遇到的问题及解决方案，便于后续纠错和复盘。

---

## 使用说明

- 按时间倒序记录（最新的在最上面）
- 每条记录包含：日期、类别、描述、解决方案、经验教训
- 类别标签：`策划` `程序` `美术` `音频` `UE引擎` `项目管理` `Bug修复`
- 重要决策使用 ⚡ 标记

---

## 记录列表

### 模板（复制使用）

```markdown
### YYYY-MM-DD | 类别标签

**问题/事项：** 简述发生了什么或需要做什么。

**处理过程：**
1. 步骤一
2. 步骤二

**结果/解决方案：** 最终如何处理。

**经验教训：** 以后遇到类似情况应该怎么做。
```

---

## 2026-07-06 ~ 至今

### 2026-07-30 | 程序 ⚡

**事项：** 创建 Enemy 体系 — AEnemy 抽象中间类 + ASatan 最终Boss + FEnemyConfigRow DataTable 结构体，完善敌人分支的 C++ 类层级。

**处理过程：**
1. `DataTable/EnemyConfigTable.h` — 新建 `FEnemyConfigRow : FTableRowBase`（15列：MaxHP / BaseDamageScale / AIDifficulty / Tier / AIPreference / DropSmokeType / 掉落货币 / 感知范围等）+ `EEnemyTier` / `EEnemyAIPreference` 枚举
2. `Character/Enemy.h/.cpp` — 新建 `AEnemy : ABaseCharacter`（Abstract, Blueprintable），添加 EnemyID / Tier / AIPreference / 掉落配置 / 感知范围等属性；重写 `InitializeAttributes()` 从 DT_EnemyConfig 加载
3. `Character/Satan.h/.cpp` — 新建 `ASatan : AEnemy`（Blueprintable），构造函数设置 EnemyID = "satan"、Tier = FinalBoss
4. `Component/AttributeComponent.h/.cpp` — `InitializeFromConfig` 重命名为 `InitializeFromCharacterConfig`；新增 `InitializeFromEnemyConfig(FName EnemyID)` 从 DT_EnemyConfig + DT_CombatParams 加载战斗属性
5. `BaseCharacter.cpp` — 调用点更新为 `InitializeFromCharacterConfig`
6. `DataTable/EnemyConfigTable.h` — `FEnemyConfigRow` 新增 `TSubclassOf<AEnemy> EnemyClass`，与 `FCharacterConfigRow.CharacterClass` 对称
7. `DataTable/MaskConfigTable.h` — 新建 `FMaskConfigRow : FTableRowBase`（15列：DisplayName / Rarity / SmokeGainScale / 三色伤害加成 / HPRegenOnKill / SkillCostScale / DropChance / Price / IconTexture / MeshAsset）+ `EMaskRarity` 枚举
8. 编译通过（0 error / 0 warning）

**设计理由：**
- AEnemy 与 ARole 平行，同为 ABaseCharacter 下的中间抽象类——敌人不需要相机和输入系统
- AIDifficulty 进入 AttributeComponent（支持精神类技能临时降低 AI），其余 Enemy 字段（身份标识/静态配置）留在 AEnemy 上
- 面具不需要 Class 列：所有面具效果 = 一行 `AddModifier()`（倍率加成/击杀回血/技能减耗），模型差异由 `MeshAsset` 驱动——零行新代码即可加新面具
- 武器/技能/消耗品需要 Class 列：不同武器有不同攻击逻辑（锤子穿透 vs 大剑蓄力+1），不同技能是不同代码路径（治愈 vs 攻击 vs 精神）——数据不足以表达

**后续待办：** ① 在 UE 编辑器中创建 `DT_EnemyConfig` DataTable 资产并填入 8 种敌人行数据；② 创建 `DT_MaskConfig` DataTable 资产；③ 创建 `AWeapon` / `ASkill` / `AConsumable` 基类（为 Class 列准备类型）

### 2026-07-30 | 策划

**事项：** DataTable_Spec.md 补充 Class 列 — 审阅全部 11 张配置表，为需要指定 C++/BP 类的表添加 Class 列。

**处理过程：**
1. 逐表分析：哪些表的不同行需要不同代码行为（而非纯数据差异）
2. 判定需要 Class 列的表：EnemyConfig（补文档）、WeaponConfig、SkillConfig、ConsumableConfig
3. 判定不需要的表：CombatParams（单例）、MaskConfig（纯修正器，MeshAsset 处理视觉）、SmokeConfig（纯数据转化）、SkillTreeConfig（纯被动）、AreaConfig（已有 LevelAsset）、EconomyConfig（单例）
4. DT_EnemyConfig §5.3 列定义新增 `EnemyClass: TSubclassOf<AEnemy>`
5. DT_WeaponConfig §6.3 列定义新增 `WeaponClass: TSubclassOf<AWeapon>`
6. DT_SkillConfig §9.3 列定义新增 `SkillClass: TSubclassOf<ASkill>`
7. DT_ConsumableConfig §12.3 列定义新增 `ConsumableClass: TSubclassOf<AConsumable>`
8. §2 表索引新增"Class 列"栏位，一目了然
9. DT_MaskConfig §7.3 列定义新增 `MeshAsset: TSoftObjectPtr<USkeletalMesh>`（面具模型）

**判断标准：** 如果两行数据用同一个 C++ 类、只改 DataTable 数值就能表现所有差异 → 不需要 Class 列。如果需要不同代码才能区分 → 需要 Class 列。

### 2026-07-30 | 程序

**事项：** 动画实例重构 — DaleAnimInstance 重命名为 RoleAnimInstance（持有 ARole* 引用），新建 EnemyAnimInstance。

**处理过程：**
1. `Animation/RoleAnimInstance.h/.cpp` — `UDaleAnimInstance` 重命名为 `URoleAnimInstance`，缓存引用从 `ABaseCharacter*` 改为 `ARole*`（更精确的类型约束）
2. `Animation/EnemyAnimInstance.h/.cpp` — 新建 `UEnemyAnimInstance : UAnimInstance`，持有 `AEnemy* OwnerEnemy`，暴露 Speed / Direction / bIsMoving / bIsInAir / VerticalVelocity / GroundDistance 六项运动数据
3. 删除旧文件 `Animation/DaleAnimInstance.h/.cpp`
4. 编译通过（0 error / 0 warning）

**设计理由：**
- 命名与类层级对齐：`ARole`（玩家中间抽象类）→ `URoleAnimInstance`，`AEnemy` → `UEnemyAnimInstance`
- 两者当前逻辑相同（通用移动数据），但分离可各自扩展：Role 后续可加输入驱动的动画状态（冲刺、闪避），Enemy 后续可加 AI 驱动的战斗动画状态
- 持有引用改回具体类型（`ARole*` / `AEnemy*`）而非 `ABaseCharacter*`，避免在 AnimInstance 中做基类通用假设

**后续待办：** 在 UE 编辑器中更新 ABP_Dale 的父类为 `URoleAnimInstance`，为新敌人创建 ABP_Enemy 指向 `UEnemyAnimInstance`

### 2026-07-29 | 程序

**事项：** 实现模块化角色系统 — 支持身体/眼睛/头发/上衣/裤子/鞋子 6 个独立网格体组合。

**处理过程：**
1. `Role.h` — 为 `ARole` 添加 5 个 `USkeletalMeshComponent` 子对象（EyesMesh / HairMesh / ShirtMesh / PantsMesh / ShoesMesh），全部通过 `SetupAttachment(GetMesh())` 挂载到身体骨骼网格体
2. `Role.h` — 添加 `SetupModularMasterPose()` 蓝图可调用函数
3. `Role.cpp` — 构造函数中创建 5 个组件并附加到身体网格体
4. `Role.cpp` — `BeginPlay()` 中调用 `SetupModularMasterPose()`，使用 `SetLeaderPoseComponent()` 让所有部件共享身体骨骼动画
5. 编译通过（0 error / 0 warning）

**设计理由：**
- 身体 `GetMesh()` 作为 Leader，其余部件为 Follower，一套动画驱动全部部件
- 每个部件在蓝图中可独立替换 SkeletalMesh 资产，支持换装
- 使用 `SetLeaderPoseComponent` 而非已弃用的 `SetMasterPoseComponent`（UE 5.6 新 API）
- 部件挂载到 `GetMesh()` 而非 RootComponent，确保跟随身体的位置/旋转/缩放

**后续可在蓝图中操作：**
- 为每个部件设置 `SkeletalMesh` 资产（如 `EyesMesh->SkeletalMesh = SK_Eyes`）
- 运行时调用 `SetupModularMasterPose()` 重新绑定（如动态替换部件后）

### 2026-07-29 | 程序

**事项：** 实现落地锁定机制 — 角色跳跃/坠落后短暂禁止移动，防止落地动画播放时角色滑步。

**处理过程：**
1. `CharacterConfigTable.h` — `FCharacterConfigRow` 新增 `LandingLockTime` 字段（`Character|Movement` 分类，默认 0.3s）
2. `AttributeComponent.h` — `AttributeNames` 命名空间新增 `LandingLockTime()` 常量
3. `AttributeComponent.cpp` — `InitializeFromConfig()` 加载 `LandingLockTime` 到 `BaseAttributes`
4. `Role.h` — 新增 `Landed()` override、`bLandingLocked` 标记、`FTimerHandle LandingLockTimer`、`OnLandingLockExpired()` 回调、`GetLandingLockTime()` 辅助方法
5. `Role.cpp` — `Move()` 首行检查 `bLandingLocked`，锁定期间直接 return 忽略输入；`Landed()` 启动一次性 Timer；Timer 到期调用 `OnLandingLockExpired()` 解除锁定
6. `DataTable_Spec.md` §4.2 列定义新增 `LandingLockTime` 行
7. 编译通过（0 error / 0 warning）

**设计理由：**
- 使用 ACharacter 内置的 `Landed()` 事件检测落地，无需每帧轮询空中状态
- Timer 而非 Tick 实现延迟解锁，性能开销为零
- 数据放入 `DT_CharacterConfig` → 策划可为不同角色设置不同落地硬直（重甲角色更长、敏捷角色更短）
- 锁定期间 `CharacterMovementComponent` 仍正常工作（重力、碰撞），仅输入被拦截
- 连续落地时 `ClearTimer` + 重新 `SetTimer`，避免旧计时器导致的过早解锁

### 2026-07-29 | 策划

**事项：** 更改先制攻击加成的效果：首回合敌人不能使用红色 → 首回合敌人不能使用蓄力。

**处理过程：**
1. GDD_Outline.md §5.2.7 战斗参数表：`先制攻击加成` 行从"首回合敌人不能使用红色"改为"首回合敌人不能使用蓄力"
2. DataTable_Spec.md §3.2 列定义：`FirstStrikeDisableRedTurns` → `FirstStrikeDisableChargeTurns`，备注从"禁用红色防御"改为"禁用蓄力"

**设计理由：**
- 禁用红色防御的效果与蓄力机制没有直接互动，且对战斗策略影响有限
- 改为禁用蓄力后，先制攻击能有效阻止敌人使用高伤害的蓄力+蓝攻爆发组合
- 配合蓄力系统的设计（边境守卫等敌人偏好蓄力+蓝攻），先制攻击的价值更加明确——先手玩家可以延缓敌人的爆发节奏

### 2026-07-29 | 程序 ⚡

**事项：** 数据架构重构 — 创建 UAttributeComponent + TMap 属性修正器系统，属性从硬编码迁移至 DataTable 驱动。

**处理过程：**
1. 新建 `DataTable/CharacterConfigTable.h` — `FCharacterConfigRow : FTableRowBase`，定义 DT_CharacterConfig 全部 14 列（MaxHP / BaseDamageScale / BlueAttackBonus / WhiteAttackBonus / MaxSmokeReserve / bHasSmokeGland / DefaultWeaponID 等）
2. 新建 `Component/AttributeComponent.h/.cpp` — `UAttributeComponent : UActorComponent`，实现 TMap + FAttributeModifier 栈的属性修正器模型。包含 `AttributeNames` 命名空间（防 FName 拼写错误）、`EModifierOp` 枚举、`InitializeFromConfig()` / `GetFinal()` / `AddModifier()` / `TickTurn()` 等完整接口
3. 更新 `BaseCharacter.h/.cpp` — 移除 `MaxHealth` 字段；新增 `CharacterID`（对应 DT RowName）+ `UAttributeComponent*` 组件 + `GetMaxHealth()` 辅助方法；`GetHealthPercent()` 和 `Heal()` 改为从 AttributeComponent 读取上限
4. 更新 `Role.h/.cpp` — 移除 `WalkSpeed` / `SprintSpeed` 字段；改为 `GetWalkSpeed()` / `GetSprintSpeed()` 从 AttributeComponent 读取
5. 更新 `Dale.cpp` — 移除 `MaxHealth = 120.0f` 硬编码；设置 `CharacterID = "drifter"`；属性完全从 DT_CharacterConfig 加载

**设计理由：**
- 所有可被 buff/debuff/装备/被动 修改的属性统一进入 UAttributeComponent 的 TMap，一种 buff = 一行 AddModifier()
- DataTable 行结构体（USTRUCT）只存数据 + 编辑器默认值，不包含跨表查询和计算逻辑
- 仅 `CurrentHealth`（高频变化的运行时状态）保留在 Character 上
- 玩家和敌人共用同一套 UAttributeComponent，区别仅在于初始化时读取不同的 DT
- GDD 中已定义的闪避 buff、面具效果、技能增益、边境之烟道具等全部可通过 FAttributeModifier 统一表达

**后续待办：** ① 在 UE 编辑器中创建 `DT_CharacterConfig` DataTable 资产并填入漂泊者/艾斯等行数据；② 已创建 `DataTable/CombatParamsTable.h` — `FCombatParamsRow`（16列全局战斗参数，单例模式）；WalkSpeed/SprintSpeed 迁移至 FCharacterConfigRow；编译通过。

### 2026-07-09 | 程序

**事项：** Enhanced Input 绑定从 BaseCharacter 迁移至 Role，新增跑动（Shift）和跳跃（Space）输入。

**处理过程：**
1. 从 `ABaseCharacter` 移除全部 Enhanced Input 相关代码（`SetupPlayerInputComponent`、`Move`、`Look`、输入资源属性）
2. 在 `ARole` 中重新实现完整输入系统：Move / Look / Sprint / Jump 四个 Action 绑定
3. 新增跑动机制：`WalkSpeed = 300`（默认走路），`SprintSpeed = 600`（按住 Shift），通过 `CharacterMovementComponent::MaxWalkSpeed` 实时切换
4. 新增跳跃：`JumpAction` 点按触发 `ACharacter::Jump()` / `StopJumping()`
5. 编译通过

**设计理由：**
- 只有可操控角色（ARole 子类）才需要输入，NPC 等非操控角色不应携带输入系统
- 走路/跑动分离为后续体力系统预留接口

**后续待办：** 在编辑器中为 ADale 蓝图创建 `IA_Sprint` 和 `IA_Jump` 资产，并映射到 `IMC_Default`。

### 2026-07-07 | 美术 ⚡

**事项：** 主角形象重设计 v2.0 — 从「狩猎者」重构为「漂泊者」。移除所有护甲、武器、工具、头带，改为纯废土衣装 + 不对称长外套的干净流浪者形象，便于潜入魔法师世界。

**处理过程：**
1. 收到反馈：不要盔甲和小刀、纯废土风、只有人物和衣服
2. 二次反馈：头部不要装饰（为面具槽留空）、衣服整洁无破洞（便于潜入魔法师世界）
3. 新视觉三角：不对称长外套（右长左短）+ 疤痕面容（左颊斜疤）+ 褪色暗红围巾（颈部唯一亮色）
4. 重新设计全部服装层：去掉所有护甲（肩甲/胸甲/护喉/护腕/护膝），去掉所有武器道具（匕首/烟瓶/工具袋/绳索），去掉头带
5. 新增不对称长外套（蜡帆布，木制扣合）作为核心视觉元素——替代旧版的肩甲剪影
6. 头部完全裸露：无头带、无帽、无发饰——面部成为唯一焦点，同时为面具装备槽留出干净空间
7. 配色保持暖灰褐废土系 + 暗红围巾，所有材质为布料或软皮——无金属（仅皮带扣例外）
8. 更新全部四方向视图（正/右侧/背/45°），新增「潜入时放松表情」
9. 新增 §13 新旧设计对比表，更新参考板（去掉战锤/黑暗之魂盔甲向，加入死亡搁浅/银翼杀手向）

**结果：** `Protagonist_Character_Design.md` 更新至 v2.0（14章节）。

**设计理由：**
- 无护甲/无武器 = 主角在魔法师城市中不引人注目，符合「潜入」叙事
- 衣服整洁 = 可伪装成平民或低阶仆从，破洞和补丁会引起不必要的注意
- 不对称长外套 = 替代旧版不对称肩甲成为 #1 剪影识别点，保持视觉锚点但不读作「战斗人员」
- 光头饰 = 面具装备槽的刚需，任何头带/兜帽都会与面具模型冲突
- 废土材质 + 整洁外观 = 在「洞穴出身」和「融入魔法世界」之间找到平衡

### 2026-07-07 | 美术

**事项：** 主角形象概念设计 v1.1 — 完成主角「狩猎者」的写实风格角色设计，包含多方向视图。

**处理过程：**
1. 阅读 GDD v0.3 §10.4（角色风格），确定主角设计锚点：洞穴人类、魔法师狩猎者、废土生存主义
2. 确定核心视觉三角：不对称肩甲 + 疤痕面容 + 褪色暗红围巾
3. 完成角色身份卡、剪影设计、完整面部特征（含口部/下颌/牙齿/胡茬）、体格数据、服装分层（内/中/外三层 + 护甲/披风/工具挂载）
4. 面具从默认外观移除，改为可选装备（§5.6 Mask Equipment Slot），支持五种稀有度层级的视觉变化
5. 确定配色方案（暖灰褐废土系，低饱和度，围巾为唯一高亮色）
6. 设计多方向视图（正面、右侧面、背面、45°），含 ASCII 结构图（均以露脸为基础）
7. 设计表情参考表（7种表情），更新原则——默认无面具时全脸传情，佩戴面具后眼眉为主要表达通道
8. 规划角色外观阶段变化（序章 → 第4次轮回），面具作为独立装备线
9. 预留女性主角变体设计方案

**结果：** 设计文档保存为 `DesignDocs/Protagonist_Character_Design.md`（v1.1，13章节），可作为3D建模参考。

**设计理由：**
- 不对称肩甲（右大左小）：右肩朝向敌人，需更多防护；左手需灵活取用腰间工具
- 疤痕面容：左颊斜向刀疤为核心面部识别特征——替代面具成为正面视觉锚点
- 面具改为可选装备：玩家在游戏中获取不同稀有度的面具并自由更换，每张面具携带特定属性加成（GDD §6.3.2），视觉随稀有度变化
- 褪色暗红围巾：唯一高饱和度元素，引导视觉焦点到面部；暗红色暗示"曾经鲜红（身份象征）→ 褪色（不可辨识的过去）"
- 服装分层设计：方便3D建模和后续装备更换系统

**新增文件：**
- `DesignDocs/Protagonist_Character_Design.md`

### 2026-07-06 | 程序 ⚡

**事项：** C++ 模块初始化 — 创建 `ABaseCharacter` 角色基类，项目从纯蓝图转为 C++ + 蓝图混合。

**处理过程：**
1. 创建 `Source/Hole/` 目录结构（Public/Private）
2. 创建 `Hole.Build.cs` 模块构建文件，添加 Core/CoreUObject/Engine/InputCore/EnhancedInput 依赖
3. 创建 `Hole.Target.cs` 和 `HoleEditor.Target.cs` 目标规则文件
4. 创建 `ABaseCharacter` 类（继承 `ACharacter`），包含生命值/颜色属性/伤害/死亡/治疗基础功能
5. 新增 `EElementalColor` 枚举（None/Red/Blue/White），对应三色克制系统
6. 修改 `Hole.uproject` 添加 Modules 配置
7. 编译成功（UE 5.6 + VS 2022，72s，8 actions 全部通过）
8. 为 `ABaseCharacter` 添加增强输入移动系统：`SetupPlayerInputComponent` 绑定 IMC + IA，`Move()`/`Look()` 虚函数处理移动和视角
9. 二次编译通过（16s，5 actions）

**注意：** `BuildSettingsVersion.V6` 在 UE 5.6 中不存在，修正为 `V5`；`IncludeOrderVersion.Unreal5_6` 可用。

**设计理由：** `ABaseCharacter` 作为所有角色（主角、魔法师、NPC）的统一基类，后续角色只需继承并扩展即可，避免重复实现属性管理和生命周期逻辑。

**移动输入设计：**
- 输入资产（`UInputMappingContext` / `UInputAction`）暴露为 `EditAnywhere` 属性，在蓝图中配置，C++ 只负责运行时绑定
- `Move()` 和 `Look()` 标记为 `virtual`，子类可重写以实现自定义移动逻辑（如固定视角、限制移动方向等）
- IMC 优先级设为 0（基础层），子类可通过叠加更高优先级的 Context 实现输入覆盖

**新增文件：**
- `Source/Hole/Hole.Build.cs`
- `Source/Hole.Target.cs`
- `Source/HoleEditor.Target.cs`
- `Source/Hole/Public/BaseCharacter.h`
- `Source/Hole/Private/BaseCharacter.cpp`

### 2026-07-06 | 策划 ⚡

**事项：** 烟系统重构（v0.2 → v0.3）— 根据用户反馈重新设计烟与货币的关系。

**核心变更：**
1. **烟 ≠ 货币：** 烟是力量载体，不同类型的烟自动转化为对应的道具/技能/被动增益
2. **烟的回复机制：** 主角（洞穴人类）无烟囊，需通过博士提炼或艾斯随时间回复
3. **货币独立：** 新增独立货币系统，通过治愈之烟兑换、支线任务、出售装备获取
4. **烟以来源分类：** 猎手之烟、学徒之烟、术者之烟、统领之烟、边境之烟、恶魔之烟、撒旦之烟、治愈之烟——每种对应特定获取物

**修改章节：** §5.3（资源系统）、§5.4（制作系统）、§6.1-6.2（成长系统）、§7.2（敌人掉落）、§7.4（技能）、§14（经济系统）、§19.1（术语表）

**设计理由：** 烟作为"力量印记"而非货币，强化了"狩猎敌人获取力量"的世界观一致性。货币独立后，玩家有更清晰的资源分配决策空间。

### 2026-07-06 | 策划

**事项：** 完成 GDD v0.2 扩充 — 将用户草案扩充为 19 章节的专业级策划案。

**处理过程：**
1. 用户填写了核心概念草案（游戏名称 Hole、Roguelike 类型、红蓝白战斗、时间轮回等）
2. Claude 按行业 GDD 标准逐章扩充，添加了参数表、流程图、设计理由、对标分析
3. 对不确定内容标记 `[待定]` 或 `[PLAYTEST]`，避免假性确定

**关键设计决策记录：**
- ⚡ 战斗核心：红蓝白三色克制（回合制博弈层）+ 同色碰撞实时格挡/闪避（操作层）双层设计
- ⚡ 成长系统：不设传统等级，改用"烟收集 → 局外技能树"的 Roguelike 模式
- ⚡ 叙事方式：碎片化叙事（魂系风格），不做大段过场动画
- ⚡ 进度结构：4次轮回逐步解锁全部6个区域，第4次轮回后方可挑战最终Boss

**经验教训：** GDD 不要求一次完美，标记 `[待定]` 是正常且必要的——设计会在原型验证中自然演变。

### 2026-07-06 | 项目管理 ⚡

**事项：** Git 版本控制接入 — 初始化仓库、创建 .gitignore、配置 GitHub MCP 服务器。

**处理过程：**
1. 创建 `.gitignore`（排除 Binaries/, Intermediate/, DerivedDataCache/, Saved/, .vs/ 等生成文件）
2. `git init` 初始化本地仓库
3. `git add . && git commit` 首次提交（166 files, 1728 insertions）
4. 创建 `.mcp.json` 配置 GitHub MCP 服务器（远程 HTTP 端点 `api.githubcopilot.com/mcp`）
5. 搜索并确认最佳 MCP 方案：GitHub 官方 `github-mcp-server`（90+ 工具）

**待完成：**
- 创建 GitHub Personal Access Token（https://github.com/settings/tokens，需 `repo` scope）
- 设置 `GITHUB_TOKEN` 环境变量激活 MCP
- 在 GitHub 创建远程仓库并推送

**设计理由：**
- 选择远程 HTTP MCP 而非 Docker（`ghcr.io/github/github-mcp-server`），因为本地未安装 Docker
- `.uasset` 和 `.umap` 文件**不**加入 .gitignore——它们是核心游戏资产，必须版本控制
- `.mcp.json` 使用项目级配置，便于团队共享

### 2026-07-06 | 项目管理
**事项：** 项目初始化 — 创建 UE5.6 项目 `Hole`，建立策划案大纲和开发日志。
**处理过程：** 完成。
**备注：** 策划案已扩充至 v0.3，见 `GDD_Outline.md`。

---

## 关键决策记录

> 此处记录项目中所有重要的 ⚡ 决策（如引擎版本选择、核心机制变更、技术方案选型等），便于后续回顾"为什么当时这么做"。

| 日期 | 决策 | 理由 | 影响 |
|------|------|------|------|
| 2026-07-07 | ⚡ 主角视觉三角 v2.0：不对称长外套 + 疤痕面容 + 暗红围巾 | 无护甲/无武器/无头饰的纯废土衣装流浪者；外套不对称剪影为 #1 识别点；光头为面具装备槽留空；整洁外观便于潜入魔法师世界 | 所有主角3D建模、动画、UI角色展示的视觉基准 |
| 2026-07-07 | ⚡ 主角视觉三角 v1.1：不对称肩甲 + 半面具 + 暗红围巾（已废弃） | 原方案：武装猎手形象 | 被 v2.0 替代 |
| 2026-07-06 | ⚡ C++ 角色基类 `ABaseCharacter`（继承 `ACharacter`）| 统一生命值、颜色属性、伤害/死亡接口，后续角色全部继承 | 角色系统技术基准，影响所有角色类的继承结构 |

| 2026-07-06 | 使用 UE5.6 | 最新稳定版本，完整功能支持 | 技术选型基准 |
| 2026-07-06 | 战斗系统：三色克制 + 实时格挡闪避 | 融合《永劫无间》《33号远征队》的核心乐趣 | 核心差异化，整个游戏围绕此系统构建 |
| 2026-07-06 | 成长系统：无等级 + 烟驱动成长 | Roguelike 局外成长模式，避免数值碾压 | 影响全部数值设计和关卡难度曲线 |
| 2026-07-06 | 叙事方式：碎片化叙事 | 参考魂系，降低叙事开发成本，匹配 Roguelike 结构 | 不做大段过场动画，影响内容管线规划 |
| 2026-07-06 | ⚡ 烟系统重构：烟≠货币 | 烟是力量载体（以来源分类，自动转化道具/技能）；货币独立（治愈烟兑换）；主角无烟囊需博士/艾斯回复 | 影响资源系统、经济系统、成长系统、敌人设计——是世界观一致性的关键修正 |

---

## 常见问题速查

> 把反复遇到的问题和解决方案记录在这里，形成项目专属知识库。

| 问题 | 解决方案 | 相关日期 |
|------|----------|----------|
| -    | -        | -        |

---

## 版本里程碑

| 版本 | 日期 | 主要内容 | 备注 |
|------|------|----------|------|
| v0.0 | 2026-07-06 | 项目初始化 | - |
| v0.1 | 2026-07-06 | GDD 草案（用户填充核心概念） | 19章节框架建立 |
| v0.2 | 2026-07-06 | GDD 扩充为专业级策划案 | 包含完整参数表、流程图、竞品分析、风险评估 |
| v0.3 | 2026-07-06 | 烟系统重构 | 烟≠货币；新增独立货币；回复机制（博士/艾斯）；烟以来源分类 |
| v0.5 | 2026-07-07 | 主角形象重设计 v2.0 | 视觉三角（不对称长外套+疤痕面容+暗红围巾）；纯布衣无护甲无武器；光头为面具槽留空；整洁废土风 |
| v0.4 | 2026-07-07 | 主角形象概念设计 v1.1 | 视觉三角（不对称肩甲+半面具+暗红围巾）；多方向视图；配色方案；表情参考（已被 v2.0 替代） |
