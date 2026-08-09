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

### 2026-08-09 | 策划 ⚡

**问题/事项：** 设计序章教学敌人（新手教程单场战斗），替代原"教学1+教学2"双教学结构。

**处理过程：**
1. 与用户定案 5 项决策：新增 `apprentice_cave` 行；洞穴仅此一场教学战斗；教学 1 一次性教完全部机制（三色克制/金色反击/蓄力/同色碰撞/格挡闪避）；教学战无掉落（烟仅在击败敌人时掉落）；取消教学 2。
2. 附加定案：序章剧情改为主角 Dale 发现魔法师并突袭 → 玩家先制（与敌方先制效果一致：开局玩家 1 层、敌方 0 层，无其它效果）。
3. 产出 `Plans/tutorial-enemy.md` 计划书；同步 GDD v0.17（§3.3/§5.2.1/§7.2/§9.3/§19.4）与 DataTable_Spec v0.13（`apprentice_cave` 行、`inept` 洞穴移除、行数 8→9）。
4. 规则补充定案：先制效果统一——玩家/敌方先制一致，仅先制方开局 1 层蓄力；撤销先制伤害（`FirstStrikeDamageScale`）与首回合禁蓄力（`FirstStrikeDisableChargeTurns`）；同步 GDD v0.18 / DataTable_Spec v0.14。
5. C++ 落地（GDD v0.19 / DataTable_Spec v0.15）：`FCombatParamsRow` 移除两个废弃参数；`StartBattle` 教学战自动玩家先制（`bEnemyFirstStrike=false`）；`UEnemyCombatAIComponent` 新增教学脚本模式（`apprentice_cave` 自动启用，7 回合固定行动 + 非法回落 + 每步引导提示）；`UBattleComponent` 新增教学钩子（锁血 1、脚本播完/血量阈值必逃、`FinishBattle(true, true)` 逃跑路径、重开重置脚本）；HUD 新增可选绑定 `TutorialHintText`。编译通过（-NoUba）。
6. 补建敌人 C++ 类：`AApprentice`（EnemyID=apprentice，普通低级魔法师基类）与 `AApprenticeCave`（EnemyID=apprentice_cave，序章教学变体，EnemyID 自动识别教学模式）；同步 DataTable_Spec v0.16；编译通过（-NoUba）。
7. Bug修复：Untitled 测试关卡同时放置 BP_Satan 与 BP_ApprenticeCave（均 Tag=Boss），`UBattleComponent` 只绑定第一个 Boss 的入场委托，导致教学怪 `LS_ApprenticeCaveIntro` 播完后 `OnIntroFinished` 无人监听、不进战斗。修复：`FOnBossIntroFinished` 改为携带敌人参数（`AActor* EnemyActor`），战斗组件绑定所有 Tag=Boss 敌人的入场动画，`HandleIntroFinished` 按实际播完者设置 `BossEnemy` 并开战（同时重置 `bBossDefeated` 支持同一关卡多个 Boss 各自触发）；编译通过（-NoUba）。
8. 新需求落地（GDD v0.20 / DataTable_Spec v0.17）：①教学路线锁定——`TutorialPlayerRoute`（T1蓝攻/T2白攻/T3红防/T4红防/T5蓝攻/T6蓄力/T7蓝攻），HUD 每回合只开放指定按钮，`PlayerChooseAction` 逻辑层拒绝非法选择，额外回合不锁；②同色碰撞可格挡慢放——新增 `ClashTimeDilation`（0.05，0=关闭），可格挡窗口开启时世界时间流速降至该值，玩家点击格挡/闪避后恢复，碰撞结算/清理/失败重开兜底恢复；编译通过（-NoUba）。
9. 慢放范围限定：`ClashTimeDilation` 仅序章教学战生效（`ApplyClashTimeDilation` 增加 `IsTutorialBattle()` 门控），正式战斗（如 Satan）不修改时间流速；同步 GDD v0.21 / DataTable_Spec v0.18；编译通过（-NoUba）。
10. Bug修复：慢放出现时立刻点击格挡被判失败——慢放原从 `ClashHitTime − max(格挡窗,闪避窗)` 开始，格挡窗比闪避窗晚 0.1s（游戏时间），慢放后游戏时间流速 0.05，该 0.1s 相当于约 2s 真实时间，"慢放一出现就点击"按游戏时间仍在窗口外 → 提前按失败。修复：慢放起点改为 `min(格挡窗,闪避窗)`（慢放出现时两个窗口均已开放），保留原 Elapsed 窗口判定，不修改输入逻辑；同步 GDD v0.22；编译通过（-NoUba）。
11. Bug修复：引导提示/玩家路线锁定比敌方当前行动快一回合——`TutorialScriptIndex` 在敌方选招时（回合开始）已 +1 指向下一回合，但提示与路线直接用它取值。修复：`GetCurrentTutorialHint` / `GetLockedPlayerAction` 取 `索引 − 1`（当前回合），敌方行动本身与表格一致；编译通过（-NoUba）。
12. 教学扩展为 9 回合完整战斗逻辑（GDD v0.23 / DataTable_Spec v0.19）：新增 T4"蓄力 vs 白攻（0 层抵抗 0.3 伤 + 1 层 + 额外回合）"与 T7"红防 vs 蓄力到 2 层（自动强化蓝攻破防，2.25 倍蓝伤）"；玩家路线同步 9 回合（T4 蓄力→额外回合蓝攻/蓄力）；HUD 额外回合显示"只能选择蓝攻或蓄力"引导；必逃改为脚本播完（T9 碰撞结算后）触发，`RunAwayHPThreshold` 仅脚本未启用时兜底，避免加长脚本被血量提前打断；编译通过（-NoUba）。
13. Bug修复：白攻/红防会清空自身蓄力层数，原 9 回合脚本 T4 敌方白攻后层数归 0，T5 蓝攻非法（被回落替换，教学点丢失）。修复：插入 T5 红红跳过（双方红防清空蓄力，顺带教学红红规则），把玩家层数清回 0 再接 T6 蓄力 vs 白攻抵抗；脚本扩展为 10 回合（T1 蓝克白 / T2 白克红 / T3 反例 / T4 红克蓝+金反击 / T5 红红跳过 / T6 蓄力抵抗+额外回合 / T7 蓝断蓄 / T8 满蓄破红防 / T9 双方蓄力 / T10 蓝蓝碰撞）；同步 GDD v0.24；编译通过（-NoUba）。
14. 教学引导重构（GDD v0.25 / DataTable_Spec v0.20）：按用户要求取消逐回合按键锁定与固定敌方脚本（`UEnemyCombatAIComponent` 恢复随机 AI，脚本/路线系统移除）；改为——①进入战斗后给出一次总提示（先制/蓄力上限/三色克制）；②两个条件教学点：玩家蓄力 1 层（首回合除外）→ 敌方强制红防 + 锁定蓄力（满蓄自动强化蓝攻破红防）；玩家蓄力 0 层 → 敌方强制白攻 + 锁定蓄力（0→1 层抵抗 + 额外回合）；③同色碰撞触发时给出格挡/闪避操作提示；④必逃改为两个教学点均演示后按 `RunAwayHPThreshold` 触发，15 回合上限兜底防软锁；编译通过（-NoUba）。
15. Bug修复：教学战入场动画播完编辑器崩溃（EXCEPTION_ACCESS_VIOLATION 读取 0x160）——调用栈 `NativeConstruct → UpdateTutorialHint → GetTutorialHintText → IsTutorialBattle`。根因：重构提示逻辑时把 `Battle->GetTutorialHintText()` 移出了 `Battle.IsValid()` 保护，而 Widget 的 `NativeConstruct` 先于 `BindToBattle` 执行，此时 `Battle` 为空弱引用，解引用空指针。修复：调用前判空（`Battle.IsValid() ? Battle->GetTutorialHintText() : FText::GetEmpty()`）；编译通过（-NoUba）。
16. 新需求（GDD v0.26）：教学战玩家第一次使用白攻时必定触发白白碰撞——`PlayerChooseAction` 检测首次白攻（`bTutorialFirstWhiteAttackUsed`）并覆盖敌方已选行动为白攻；教学战内仅一次，失败重开重置；编译通过（-NoUba）。

**结果/解决方案：** 教学引导重构为"开场总提示一次 + 两个条件教学点（蓄力 1 层/敌方红防 → 满蓄破防；蓄力 0 层/敌方白攻 → 抵抗+额外回合）+ 首次白攻必定白白碰撞 + 碰撞提示"，玩家自由出招、敌方随机 AI；`apprentice_cave`（Tutorial/180HP/BaseDamageScale 0.4/无掉落/SpawnAreas=hole）；教学锁血 1；必逃在两个教学点完成后按血量阈值触发，15 回合兜底。C++ 侧全部落地并通过编译；编辑器侧资产（DT 行、BP_Apprentice/BP_Apprentice_Cave/ABP、蒙太奇、WBP 绑定、Level Sequence）由用户手动完成。

**经验教训：** 教学敌人设计应先把"教学点 → 回合脚本 → 现有规则约束（蓄力门槛/层数流转/先制）"三者对齐再定数值；逃跑型敌人不配置掉落，烟掉落语义统一为"击败敌人"；玩家先制规则在序章教学战首次落地，后续探索攻击先制可直接复用。

---

### 2026-08-08 | 程序 ⚡

**事项：** 双蒙太奇时间对齐统一采用"双向错峰"预排——两侧各自取 `max(0, 对方真实时间 − 自身真实时间)` 作为开始延时，而不是固定一侧先播、只延后另一侧；同时约定 `FTimerManager::SetTimer` 不接受 `Rate <= 0`（直接失效），非正延时必须立即执行。

**处理过程：** 红防 vs 蓝攻对齐过程中，单向预排在"举剑帧晚于命中帧"的数据下无法重合，且 `Delay=0` 导致定时器永不触发、金色反击待命中挂起、回合卡死；改为双向错峰后所有数据组合均可对齐（红防先起手、蓝攻延后出刀）。

**结果/解决方案：** 已写入 AGENTS.md 开发原则第 3 条；后续所有类似需求（蒙太奇/时间点对齐）统一按双向错峰实现，延后定时器在战斗结束/重试时清理。

**经验教训：** 时间对齐问题先判断"谁先起手"，不要默认攻击方先播；0 延时定时器是隐藏陷阱（UE 会直接不调度）。

### 2026-08-08 | Bug修复

**事项：** 同色碰撞的格挡/闪避窗口失效——伤害出现前任意时刻点击格挡几乎都判定成功，窗口远超设定值（BlockWindowSeconds=0.25s）。

**处理过程：**
1. 定位①：`OnBlockPressed`/`OnDodgePressed` 用世界绝对时间 `Now` 与相对时间 `ClashHitTime` 比较（`Now >= ClashHitTime − 窗口`），条件几乎恒真——整个碰撞阶段都算窗口内。
2. 定位②：`ClashHitTime` 直接取 `ClashAttackHit` 通知的绝对时间，未按随机 Section（1|2|3）起点/PlayRate 折算真实秒数，伤害与窗口锚点错位（与红防同类问题）。
3. 修复：新增 `ClashStartTime`，输入判定改为 `Now − ClashStartTime >= ClashHitTime − 窗口`；`ClashHitTime` 改用 `GetNotifyRealTime`（扣除随机 Section 起点、按 PlayRate 折算，缺失回落 `ClashAttackTime`）；新增 `StartClash`/`OnBlockPressed`/`OnDodgePressed` 的 Elapsed/HitTime 日志。
4. 需求补充（2026-08-08，定案：格挡动画只有一种）：不使用 `BlockFail` 动画——窗口外点击格挡 = 立即判定失败并播放格挡动画（`PlayBlockAnimNow`：红防姿态），不再等命中帧；失败后 `BlockFailLockoutSeconds`（默认 1s）内不可再次格挡（`LastBlockFailTime` 锁定优先于窗口判定）；命中帧立刻混入受击动画 `Hurt`；`FCombatParamsRow` 新增 `BlockFailLockoutSeconds`。
5. 结构清理（2026-08-08）：`FCombatAnimRow` 删除 `BlockFail` 列、`BlockSuccess` 改名 `Block`（格挡动画只有一种）；`PlayBlockSuccessChain` 改用 `Row->Block`，`PlayClashFailReaction` 非闪避分支回落 `Hurt`；`DT_CombatAnimConfig` 资产已用编辑器脚本迁移（drifter.Block = 红防姿态 PlayRate 2.0，satan.Block 空），`DataTable_Spec` 13 号表同步。
6. PIE 复现无输入也提前受击后定位：日志显示 `Section=1/2` 时 `ClashHitTime` 回落为全局 0.800（这两个 Section 没有 `ClashAttackHit` 通知），而 `Section=3` 为 2.058——0.8s 比攻击动画打击帧早，Hurt 提前播放。修复：所选 Section 缺少通知时改用**该 Section 的结束时间（真实秒数）**作为命中锚点，`ClashAttackTime` 仅作最后兜底。
7. PIE 复现"接近窗口未命中 → 只有格挡动画、无受击/伤害/成功链"后定位（用户日志）：①`GetNotifyTime` 只取全 Montage 第一个 `ClashAttackHit`（属于 Section 0），其它 Section 的打击通知被误判缺失；②碰撞通知在影响定时器前触发并清掉待命中（Amount=0），随后格挡动画播完触发回合闸门，Phase 在命中前已从 Clash 变回 ActionSelect，`OnClashImpact` 直接 return → 结算被跳过。修复：`GetNotifyRealTime` 改为**在当前 Section 时间范围内匹配通知**（多个同名通知按段取）；`TryAdvanceTurnIfGateDone` 在 `Phase==Clash` 时禁止推进（由 `OnClashImpact` 结算后再推进）；`ClashHitTime<=0` 时强制用 `ClashAttackTime` 兜底（0 延时定时器永不触发）。
8. 闸门拦截过粗导致碰撞后回合卡死：`ResolveClash` 结算后 Phase 仍是 Clash，`TryAdvanceTurnIfGateDone` 被 `Phase==Clash` 直接拦截，永远进不了下一回合。修复：拦截条件改为 `Phase==Clash && !bClashResolved`——未结算时禁止提前推进，`ResolveClash` 置 `bClashResolved=true` 后正常推进。

**结果/解决方案：** 编译通过（-NoUba，12.8s）。PIE 待验证：接近窗口但未在窗口内点击 → 只播格挡动画，命中帧正常结算（受击动画+伤害）；无输入时 Hurt 与当前 Section 打击帧对齐；碰撞结算后正常进入下一回合（不再卡死）。

**经验教训：** 相对时间比较必须统一基准（都用经过时间），绝对世界时间不能与相对时间混用；碰撞时间锚点同样遵循真实秒数换算原则。

### 2026-08-08 | Bug修复

**事项：** Player 红防面对敌方蓝攻时红防延迟触发，举剑帧（GuardReady）与蓝攻伤害点（BlueAttackHit）不重合。

**处理过程：**
1. 定位：`ScheduleDefenderReaction` 用两个通知的"绝对时间"直接相减作为预排延时，没有扣除 Montage 从 Section 起点播放的偏移，也没有按 `FAnimRef::PlayRate` 折算真实秒数；任一 Montage 的 PlayRate ≠ 1 或配置了起始 Section 时，红防就会提前/延后。
2. 修复：新增 `GetNotifyRealTime(Montage, EventName, AnimRef)`——通知时间先减 Section 起点（`GetSectionStartAndEndTime`），再除以 PlayRate；`ScheduleDefenderReaction` 改为按真实提前量 `HitRealTime − GuardReadyRealTime` 预排红防（无 GuardReady 标记时用真实命中时间 − `RedDefenseLeadTime` 回落），并传入攻击方蓝攻 `FAnimRef` 以获取其 PlayRate/Section。
3. 构建受阻一次：编辑器 Live Coding 激活导致 UBT 拒绝编译，关闭编辑器后重编通过。
4. PIE 复现回合卡死（红防不播放）后加固：异常延时（非有限值或超出蓝攻动作时长）会导致红防永不播放 → 金色反击待命中挂起 → 回合闸门无法推进；`ScheduleDefenderReaction` 增加有限性检查与"钳制到蓝攻真实播放时长"的安全上限，并输出原始通知时间/Section 起点/PlayRate 的详细日志用于校准。
5. 读用户日志定位最终根因：日志显示 `Delay=0.000` 且红防从未播放——`FTimerManager::SetTimer` 对 `Rate<=0` 直接 `Invalidate()`，定时器根本不会触发；此前"延时为正"时能播（只是晚），修成 0 后反而永不播放，金色反击待命中一直挂起 → 回合卡死。修复：`Delay<=0` 时改为立即调用红防链（不走定时器）。
6. 数据校准（供策划调整）：蓝攻命中帧真实时间 `HitRealTime=0.242s`，红防举剑标记 `GuardReadyRealTime=0.383s`——即使立即启动红防，举剑帧仍比命中点晚约 0.14s；要完全重合需把 `MTG_Dale_RedDefence` 的 GuardReady 标记移到 ≤0.24s 处，或把玩家红防 PlayRate 调到约 1.6。另发现玩家红防 Montage 缺少名为 `RedDefence` 的 Section（日志 `JumpToSectionName RedDefence failed`），当前从开头起播。
7. 需求澄清后改为**双向错峰预排**（2026-08-08）：红防开始 = `max(0, HitReal − GuardReal)`，蓝攻开始 = `max(0, GuardReal − HitReal)`——当 GuardReady 晚于命中帧时，红防立即起手、蓝攻延后出刀（新增 `BlueAttackDelayTimer`，正延时定时器可正常触发），举剑帧与命中帧在所有数据组合下都能重合；战斗结束/重试时清理该定时器。

**结果/解决方案：** 编译通过（-NoUba，27.6s）。PIE 待验证：蓝 vs 红不再卡死；红防先起手、蓝攻延后 0.141s 出刀，举剑帧与命中帧重合；回合推进正常。

**经验教训：** Montage 通知时间是"资产时间线"的绝对时间，实际播放起点受起始 Section 与 PlayRate 影响；任何跨 Montage 的对齐计算都必须统一换算成真实播放秒数，否则在不同动画配置下必然漂移。

### 2026-08-07 | 程序 ⚡

**事项：** 战斗规则 v1.1 定案——①红防 vs 蓝攻：直接选出的蓝攻（含 2 层）一律被金色反击、不破防，仅"蓄力对红防蓄满自动发动强化蓝攻"保留破防例外；②先制攻击效果改为开场拥有一层蓄力（Satan 等 Boss 默认敌方先制，开局 0:1，避免 0:0 无三方博弈）；③白攻 vs 蓄力仅蓄力前 0 层时触发额外回合（0:1 时白攻不再过于劣势）；④打断蓄力/金色反击/格挡闪避成功方获得 1 层蓄力，格挡闪避失败敌方获得 1 层蓄力。

**处理过程：**
1. 原 `ResolveNormalTurn` 红防×蓝攻分支对敌方 2 层蓝攻有 `EnemyChargeStacks >= 2` 破防检查，与"2 层蓝攻不破红防"规则冲突。
2. 删除该检查：红防 vs 蓝攻统一 `R.EnemyDamageTaken = GetPlayerGoldDamage()`（玩家不受伤、敌方吃金色反击）；`PlayResolutionAnimations` 蓝 vs 红专用路径无需改动（`bCounterSucceeds` 恒为 true）。
3. 蓄力对红防分支保持不变：1 层蓄力撞红防 → 蓄满自动发动强化蓝攻，红防正面承受。
4. 先制攻击：`EnterBattle` 按 `bEnemyFirstStrike`（默认 true）设置开局层数——敌方先制=敌方 1 层/我方 0 层，玩家先制则反之；失败重开会重新应用。
5. 白攻 vs 蓄力（2026-08-07 多次修订定案）：`FTurnResolution` 新增 `bEnemyChargeResisted`/`bPlayerChargeResisted` 与伤害/额外回合解耦——白攻从不打断蓄力，蓄力方无论 0/1 层都保持蓄力姿态、不播受击动画；蓄力前 0 层：吃 0.3 白伤 + 1 层、触发额外回合；蓄力前 1 层：吃**全额白伤** + 1 层（到 2 层）、无额外回合；`RegisterSideHit` 的抵抗反应按新标记，`PlayChargeResistPose` 增加 `bBlockGate` 参数（无额外回合时不阻塞闸门）。
6. 蓄力奖励（2026-08-07 晚修订为统一规则）：任何正常对敌方造成伤害 → 自身 +1 层（上限 2），白攻 vs 蓄力除外（0.3 抵抗伤害不给予奖励）；蓝攻打断蓄力、金色反击、白克红、蓝克白、蓄力对红防自动强化蓝攻、额外回合蓝刀均走同一规则，在结算末尾统一 `FMath::Min(层数 + 1, Max)`；同色对抗中格挡/闪避成功玩家 +1 层、格挡/闪避失败敌方 +1 层（`ResolveClash` 保留）。
7. EnemyAI 规则（2026-08-07）：`ChooseAction` 新增 `PlayerChargeStacks` 参数，玩家 0 层蓄力时敌方不选红防（红防只克制蓝攻）；`UBattleComponent` 兜底随机分支同步该规则。
8. Bug 确认与修复（2026-08-07）：统一奖励误把同色碰撞的待判定伤害（`R.PlayerDamageTaken`）当作"已造成伤害"，敌方在结算矩阵先 +1 层，格挡/闪避失败时 `ResolveClash` 又 +1 层（叠加成 2 层），格挡/闪避成功时也错误 +1 层。修复：统一奖励块跳过 `R.bClash`，碰撞的蓄力奖励完全由格挡/闪避结果决定；非碰撞的"造成伤害 +1 层"均发生在招式清空之后，结算为 1 层。

**结果/解决方案：** 编译通过（-NoUba）。PIE 待验证：开局敌方 1 层（0:1）；白攻 vs 0 层蓄力 → 0.3 抵抗 + 额外回合；白攻 vs 1 层蓄力 → 全额白伤、无额外回合，但始终保持蓄力姿态、不播受击动画；蓝攻打断蓄力后自身 1 层；白克红/蓝克白/金色反击/强化蓝攻/额外回合蓝刀命中后攻击方 1 层；白攻 vs 蓄力不给攻击方层数（按定案）；同色碰撞：格挡/闪避成功玩家 1 层（敌 0）、失败敌方 1 层（玩家 0），不再叠加；玩家 0 层蓄力时敌方 AI 不再选红防。

**经验教训：** 克制矩阵的"层数特例"要明确限定来源（直接选招 vs 蓄力自动发动）与触发条件（蓄力前层数）；"抵抗"与"额外回合"是两个独立语义，应拆成独立标记，避免一个布尔值承担两种含义。

### 2026-08-07 | Bug修复

**事项：** 两个问题——①敌方蓄力抵抗我方白攻（白攻 vs 蓄力）时，敌方立刻进入额外回合动画，额外回合动画打断了仍在播放的蓄力姿态；应等蓄力动画完整播完再进入额外回合。②出现额外回合时另一方也"行动"一次：玩家额外回合敌方也会播一次行动动画，敌方额外回合玩家会自动再放一次白刀。

**处理过程：**
1. 定位：`ApplyResolution` 开启回合闸门后，白攻 Montage（非循环）计入 `GatedMontages`，而蓄力姿态是循环动画、按设计不阻塞闸门；白攻命中帧 `ApplyPendingHitNow` 重播蓄力抵抗姿态（`TargetRow->Charge`），白攻 Montage 播完即推进 `EndTurnAndAdvance` → 敌方额外回合，此时蓄力姿态仍在播放，被额外回合动画打断。
2. 修复：新增 `PlayChargeResistPose`——蓄力抵抗姿态（循环）也计入回合闸门，并在一整个循环时长（`GetPlayLength()/PlayRate`）后用 `Montage_Stop` 主动结束，`OnActionMontageEnded` 再正常推进到额外回合；`ApplyPendingHitNow` 检测"目标方有额外回合 + 命中反应为目标蓄力姿态"时走该路径（玩家/敌方镜像对称）。
3. 边界：战斗结束/重试/调试结束时清除 `ChargePoseTimer`；非循环蓄力姿态走原有自然结束路径，不设定时器。
4. 定位②：`PlayResolutionAnimations` 始终用 `PlayerLastAction`/`EnemyChosenAction` 播双方动画，而额外回合结算时另一方保留的仍是上一回合的旧行动（如我方白刀），于是敌方额外回合时玩家会再播一次旧白刀；玩家额外回合时敌方旧行动也会播，且旧行动为红防时还会误入蓝 vs 红反击专用路径。
5. 修复②：`FTurnResolution` 新增 `bPlayerOnlyAction`/`bEnemyOnlyAction`，`ResolveExtraTurn` 标记本回合只有一方行动；`PlayResolutionAnimations` 只注册行动方的命中/动画，另一方不播；`RegisterSideHit` 在单方结算时把 `OtherAction` 视为 `None`，避免旧行动误触发克制/打断判定（玩家/敌方镜像对称）。
6. PIE 复现①仍被打断的补充定位：敌方蓄力动作与抵抗反应是同一 Montage 资产，命中帧"停旧实例 + 重播新实例"后，旧实例的 `OnMontageEnded` 回调带同一资产指针，把新实例刚计入的 `GatedMontages` 条目误删，闸门提前放行 → 额外回合再次打断蓄力。
7. 修复①补：`OnActionMontageEnded` 移出闸门前先经 `IsMontageActiveOnCombatants` 确认该蒙太奇在玩家/敌人动画实例上已无活动实例，旧实例结束回调不再误删新实例的闸门条目。
8. PIE 复现①白刀命中瞬间蓄力重播的补充定位：命中反应与当前蓄力姿态是同一 Montage，`ApplyPendingHitNow` 一律先停当前动作再播反应，等于把蓄力原地重播了一次。修复：当前活动 Montage 与反应 Montage 相同时不打断；`PlayChargeResistPose` 检测到姿态已在播放时直接延续，并只等当前循环的剩余时长，不再重播。

**结果/解决方案：** 编译通过（-NoUba）。PIE 待验证：①白攻 vs 敌方蓄力 → 敌方蓄力姿态连续不重播，完整播完当前循环后（含 BlendOut）才进入额外回合动画；②玩家额外回合敌方不再播行动动画，敌方额外回合玩家不再自动放白刀，只有获得额外回合的一方出招。

**经验教训：** 循环姿态（蓄力）默认不阻塞回合闸门，但"抵抗 + 额外回合"这类需要姿态播完才能接下一段动画的时序，必须显式把姿态计入闸门并用定时器主动收尾；额外回合是单方结算，动画编排必须只播行动方，另一方保留的 `LastAction` 是上一回合旧值，绝不能被当作本回合行动；只有明确说明可混入的动画才允许打断其它动画。

### 2026-08-07 | 程序

**事项：** 战斗 HUD 增加"敌方出招"提示——玩家点击行动按钮（回合开始）后显示敌方本回合所选行动，3 秒后自动隐藏。

**处理过程：**
1. `UBattleComponent::PlayerChooseAction` 改为返回 `bool`（非法选择返回 false），新增只读 `GetEnemyChosenAction()` 暴露敌方已选行动。
2. `UCombatHUDWidget` 新增可选绑定 `EnemyActionHintText`（`BindWidgetOptional`）与 `EnemyHintDuration`（默认 3 秒）：5 个行动按钮回调在选招成功时显示"敌方出招：{行动名}"（行动名复用 `EBattleAction` 的 DisplayName），Widget 定时器 3 秒后隐藏；新回合/战斗结束/额外回合开始时提前隐藏，避免过期信息。
3. 编译通过（-NoUba）。WBP_CombatHUD 的 `EnemyActionHintText` TextBlock 由用户手动补齐。
4. PIE 复现额外回合仍显示旧提示的修复：点击回调原先在 `PlayerChooseAction` 返回后才检查 `IsPlayerExtraTurn()`，而额外回合分支内部已把标记清除，检查永远为 false → 把上一回合的敌方旧行动显示出来。修复：新增 `ChooseAction` 统一入口，在调用 `PlayerChooseAction` **之前**记录 `bWasExtraTurn`，额外回合选招成功时直接隐藏提示。
5. PIE 复现触发额外回合的当回合提示消失的修复：`ShowEnemyActionHint` 与刷新逻辑在结算后看到 `bPlayerExtraTurnPending` 已置位就隐藏，把"当回合"误判为"额外回合选择阶段"。修复：`ShowEnemyActionHint` 不再检查额外回合标记（由 `ChooseAction` 调用前记录处理）；刷新逻辑改为 `IsPlayerExtraTurn() && !HasPlayerChosenAction()` 才清提示——触发回合结算中玩家已选招（保留提示），额外回合选择开始（未选招）才清除；`UBattleComponent` 新增只读 `HasPlayerChosenAction()`。
6. 需求调整（2026-08-07，定案：只显示敌方出招）：敌方额外回合开始时（阶段进入 `Resolving`）显示"敌方出招：{敌方额外回合行动}"；玩家额外回合敌方不出招，不显示任何提示；统一复用 `EnemyActionHintText` 与 3 秒定时器（`SetHintText`）。

**结果/解决方案：** C++ 已落地并编译通过；PIE 待验证：普通回合选招后提示出现、3 秒后消失；触发额外回合的当回合仍显示敌方提示（结算期间保留）；玩家额外回合选招时不显示任何提示；敌方额外回合开始时显示"敌方出招：X"。

**经验教训：** 显示"敌方行动"应在玩家选招成功后读取已选定的 `EnemyChosenAction`，用选招返回值确认成功而非阶段判断，避免非法点击误显示；额外回合/新回合必须主动清除旧提示。注意 `PlayerChooseAction` 会在结算时清除额外回合标记，任何需要"调用前状态"的判断（如是否额外回合）都必须在调用前记录。

### 2026-08-06 | Bug修复

**事项：** 两个问题——①"满蓄力 vs 红防"自动强化蓝攻时，蓝攻动画直接播放打断/替代了蓄力动画，玩家观感为"红防被蓝攻打中且无反击"（实际敌方选择的是蓄力，1 层→2 层自动发动强化蓝攻）；②格挡成功链路中 `PlayAnimThenReaction` 重复绑定 `OnMontageEnded` 触发 ensure，且停帧用 Timer 恢复存在恢复失败/时长不固定的隐患。

**处理过程：**
1. 定位①：日志 `PlayResolutionAnimations - PlayerAction=1 EnemyAction=4 PlayerDmg=48.2` 证明敌方实际选的是蓄力（Charge=4）而非蓝攻；`RegisterSideHit` 把满蓄力按蓝攻处理时直接 `PlayCombatAnim(BlueAttack)`，未播蓄力动画。
2. 修复①：`RegisterSideHit` 中满蓄力自动强化蓝攻改为 `PlayAnimThenReaction(Charge, BlueAttack)`——先完整播放蓄力动画，播完立即接蓝攻动画（蓝攻不打断蓄力）；伤害仍由蓝攻命中通知/播完结算，回合闸门自然覆盖蓄力→蓝攻链。
3. 定位②：ensure 是 `PlayAnimThenReaction` 在 `PlayCombatAnim` 已绑定 `OnMontageEnded` 后再次 `AddDynamic` 造成重复绑定（`PlayBlockSuccessChain` 每次格挡成功必触发）。
4. 修复②：删除 `PlayAnimThenReaction` 里的重复绑定，统一由 `PlayCombatAnim` 先移除再添加；停帧 `StartHitStop` 改为组件 Tick 用 `DeltaTime` 累积固定时长倒计时（重叠触发保留更长剩余时间），结束时仅恢复仍处于播放状态的 Montage（`Montage_IsActive` 校验），战斗结束/重试统一 `EndHitStop` 清理；触发时机保持"格挡/闪避生效（造成伤害判定）"时。

**结果/解决方案：** 编译通过（-NoUba）。PIE 待验证：敌方 1 层蓄力 + 玩家红防 → 敌方完整播放蓄力动画后再播蓝攻，玩家正面承受，无金色反击；格挡成功链路不再弹 ensure，停帧 0.12s 固定。

**经验教训：** 动画播放顺序必须与结算语义一致——"蓄满自动发动"要先呈现蓄力过程再接攻击；统一由 `PlayCombatAnim` 负责 `OnMontageEnded` 绑定，任何"先播 A 再接 B"的链都走 `PlayAnimThenReaction` 且不得重复绑定。

### 2026-08-06 | 程序 ⚡

**事项：** 回合推进升级为"通用动画闸门"——每次结算的完整播出链（行动 + 命中反应）播完且无待命中/待接反应后才推进；红防 vs 蓝攻的"红防→金色反击"链从专用等待变为通用规则。

**处理过程：**
1. 先落专用方案（提交 `f47efc7`）：`bAwaitingDefenderChain` / `AwaitingChainFinalMontage` 挂起红防链的回合推进，验证"等动画链播完"方向可行。
2. 泛化为通用闸门（当前工作区）：`ApplyResolution` / `StartClash` 开启 `bTurnGateOpen`，`PlayCombatAnim` 将本结算的非循环 Montage 计入 `GatedMontages`，`OnActionMontageEnded` 移出并在"无播放中蒙太奇 + 无待命中事件 + 无待接反应"时经 `TryAdvanceTurnIfGateDone` 推进。
3. 防死锁：待命中事件无承载 Montage（`FallbackMontage == nullptr` 且伤害 > 0）立即结算；循环姿态（如蓄力，`Montage->bLoop`）不计入播出链；战斗结束/重试清理时关闭闸门并清空播出链。

**结果/解决方案：** 编译通过（`-NoUba`，修复 `bIsLooping` → `bLoop` 编译错误）。PIE 待验证：任意结算 → 行动/命中反应完整播完才进入下一回合；红防 vs 蓝攻 → 红防完整播放 → 金色反击 → 结算 → 才推进；快速点下一回合不会清掉红防预排。

**经验教训：** "结算驱动动画"的回合需要以动画链终点作为推进闸门；等待条件要统一收敛到"无播放中蒙太奇 + 无待命中 + 无待接反应"，避免每类结算各写一套挂起逻辑。

### 2026-08-06 | 项目管理

**事项：** 构建卡住处理规则——UBA 因内存压力限流导致构建长时间无产出时，直接取消旧构建重新构建。

**处理过程：** 构建长时间停在"Delaying ... due to memory pressure"（机器可用物理内存仅 1.7GB），取消遗留的 Build.bat/UnrealBuildTool 进程后加 `-NoUba` 重新构建，6.6 秒编译通过。

**经验教训：** UBA 内存限流可能让构建无限期等待；机器内存吃紧时直接用 `-NoUba` 本地编译，比等待加速器调度更快可预期。

### 2026-08-06 | Bug修复

**事项：** 红防 vs 蓝攻时红防动画不播放——金色反击注册把刚预排好的红防定时器清掉了。

**处理过程：**
1. 定位：`RegisterBlueVsRedHit` 顺序为"注册蓝攻事件 → 预排红防 → 注册金色反击"，而 `RegisterPendingHit` 会先 `ClearPendingHit` 同侧槽并连带清理防御定时器，金色反击注册恰好清掉了红防预排（防御方侧）。
2. 修复：调整顺序为"先注册金色反击 → 再注册蓝攻事件 → 最后预排红防"，保证定时器存活。
3. `ScheduleDefenderReaction` 增加日志（Delay/HitTime/GuardReadyTime），PIE 可直接核对严格判定时间。

**结果/解决方案：** 编译通过，提交 `62ccc98`。PIE 待验证：红防 vs 蓝攻 → 日志出现"红防预排 Delay=..."且红防动画播放，`GuardReady` 帧与 `BlueAttackHit` 帧对齐。

**经验教训：** 同一结算里有多个 `RegisterPendingHit` 时，凡带防御预排的注册必须放到最后，否则后续注册的清理会把预排定时器一起清掉。

### 2026-08-06 | Bug修复

**事项：** 格挡成功后金色反击命中时，敌方先播完被防动画再受击——受击应打断被防动画。

**处理过程：**
1. 修复金色反击待命中注册的受击反应取错行：目标（敌方）的 `HitReaction` 应取敌方行 `Hurt`，此前误取玩家行 `Hurt`。
2. `ApplyPendingHitNow` 播放命中反应前显式 `Montage_Stop` 当前活动 Montage（如 `BlockedReaction`），保证受击立即打断被防动画，不依赖混合时机。

**结果/解决方案：** 编译通过，提交 `c7ca018`。PIE 待验证：格挡成功 → 敌方 `BlockedReaction` 中途被金色反击受击打断。

**经验教训：** 待命中事件的 `HitReaction` 属于**目标方**的行（谁挨打用谁的动画），不能误用攻击方行；命中反应要显式打断当前动作。

### 2026-08-06 | Bug修复

**事项：** 白攻被蓝攻克制时先播白攻动画再受击——Task 3 重构后 `RegisterSideHit` 默认"双方都播行动动画"，丢失了被克制方不出招的结算层判定。

**处理过程：**
1. 新增 `IsActionSuppressed`：白攻 vs 蓝攻、红防 vs 白攻、蓄力 vs 蓝攻 时该侧不播行动动画（播放权由结算结果推导）。
2. `RegisterSideHit`：被抑制侧直接返回；无伤害行动（蓄力抵抗/无事发生）只播姿态、不注册命中事件；满蓄力 vs 红防自动强化蓝攻按蓝攻动画/事件处理。
3. 文档：设计文档新增"动画优先级与打断原则"，GDD/AGENTS 同步。

**结果/解决方案：** 编译通过，提交 `91d9056`。PIE 待验证：白攻 vs 蓝攻 → 只播敌方蓝攻 + 我方命中帧受击，不再先播白攻。

**经验教训：** 动画播放权必须由结算结果推导，不能默认"双方都播"；把"谁有权出招"和"谁在命中帧受击"分成两层判断。

### 2026-08-06 | 程序 ⚡

**事项：** 伤害结算绑定动画命中通知落地——新增 `UAnimNotify_CombatDamage`/`UAnimNotify_CombatMarker`，`UBattleComponent` 以待命中事件槽（`FPendingHitEvent`）延迟结算；碰撞窗口以命中通知为锚并加入输入冷却；停帧与被格挡动画接入。

**处理过程：**
1. 参数/表列：`FCombatParamsRow` 新增 `ClashInputCooldown`（0.15）、`RedDefenseLeadTime`（0.3）、`HitStopDuration`（0.12）；`FCombatAnimRow` 新增 `BlockedReaction`；脚本同步。
2. 通知类：`AnimNotify_CombatDamage`（命中帧回调 `OnHitNotify`）、`AnimNotify_CombatMarker`（`GuardReady` 等无伤害标记）。
3. 待命中事件：结算只注册不扣血；命中通知/蒙太奇播完回落二选一消费（先到先得），`ApplyPendingHitNow` 统一扣血+受击+被格挡反馈；`PlayCombatAnim` 统一绑定 `OnActionMontageEnded` 保证回落可达。
4. 蓝 vs 红：`RegisterBlueVsRedHit` 注册蓝攻命中事件与金色反击事件；红防按 `BlueAttackHitTime - GuardReadyTime` 预排启动。
5. 碰撞：`StartClash` 以 `ClashAttackHit` 通知时间为锚（缺失回落 `ClashAttackTime`）；格挡/闪避各自窗口判定；`ClashInputCooldown` 防连按；`ResolveClash` 只决定结果与金额，伤害由通知/影响计时器触发。
6. 反馈：格挡/闪避/红防反击成功触发 `StartHitStop`（暂停双方 Montage 后恢复）；被格挡方立即混入 `BlockedReaction`。
7. 文档：GDD v0.11、DataTable_Spec v0.10、AGENTS.md 同步。
8. 命名修订：`ClashTelegraphBlue/White` → `ClashAttackBlue/White`（碰撞攻击）；命中事件 → `ClashAttackHit`；计时参数 → `ClashAttackTime`；DataTable_Spec v0.11。
9. 随机 Section：`ClashAttack*Sections`（竖线分隔）配置碰撞攻击可选 Section，`StartClash` 随机选一段播放，空则回落到 `ClashAttack*.SectionName`；GDD v0.12 / DataTable_Spec v0.12。

**结果/解决方案：** 编译通过（提交 `f069d0a`、`647fba0`、`296f39f`、`9e73e4a`、`d9c5ad1`、`44125f7`）。待编辑器：给攻击/碰撞攻击 Montage 挂命中通知、红防挂 `GuardReady` 标记、填 `BlockedReaction` 资产与 `ClashAttack*Sections`；PIE 验证 12 项清单（见实现计划 Task 8）。

**经验教训：** "先注册、后由动画事件消费"让伤害帧与表现天然同步；双槽先到先得保证不重复结算；停帧用 Montage_Pause/Resume 而非全局时间膨胀，避免计时器被冻结。

### 2026-08-06 | 策划 ⚡ + 程序

**事项：** 金色反击动画顺序定案——红防克蓝攻时，红防与蓝攻动画同时播放，玩家不受伤；红防播完接金色反击，敌方蓝攻播完接受击；并澄清"0 蓄力打出蓝攻"观感问题。

**处理过程：**
1. 排查"敌人 0 蓄力打出蓝攻"：`UEnemyCombatAIComponent::ChooseAction` 已保证 0 层不选蓝攻（含额外回合）；实际观感来自红防反击场景——敌方蓝攻动画照常播放且蓄力被清 0，属预期表现。
2. 动画编排：`PlayResolutionAnimations` 新增红防反击分支（玩家红防 vs 敌方蓝攻）与镜像分支（玩家蓝攻 vs 敌方红防）：双方行动动画同时播放，各自播完后按槽接反应（金色反击/受击）。
3. 播放机制泛化：原单人"弹反→金色反击"回调升级为玩家/敌人双槽待接反应（`PlayAnimThenReaction` / `ClearPendingReactionSide` / `OnActionMontageEnded`），格挡连播改走同一机制；收刀时统一清理两侧待接。
4. 文档同步：GDD §5.2.3 金色反击动画顺序、AGENTS.md 战斗动画约定。
5. 蓄力抵抗白攻修复：抵抗方不播受击/打断动画、保持蓄力姿态；`StartPlayerExtraTurn`/`StartEnemyExtraTurn` 增加日志确认额外回合触发（结算链路本身无逻辑 bug）。

**结果/解决方案：** 编译通过，提交 `04e7501` / `3db02ae`。PIE 待验证：红防 vs 蓝攻 → 红防+蓝攻同播 → 我方不掉血 → 金色反击 + 敌方受击；白攻 vs 蓄力 → 抵抗方保持蓄力姿态、不播受击，日志出现"额外回合触发"。

**经验教训：** 伤害结算目前仍在结算时立即生效，动画只负责表现顺序；后续如需"命中帧才掉血"，应把伤害触发改到 Montage AnimNotify（现有 `AnimNotify_Hold` 同类机制）。

### 2026-08-06 | 策划 ⚡

**事项：** 同色碰撞规则定案——蓝 vs 蓝、白 vs 白不再直接结算双方伤害，仅进入抵挡环节（敌方伤害由格挡/闪避判定）；红 vs 红维持跳过。

**处理过程：**
1. 结算矩阵修改：`ResolveNormalTurn` 中 BlueClash / WhiteClash 分支移除 `R.EnemyDamageTaken`（玩家对敌的自动命中伤害），保留 `R.PlayerDamageTaken` 作为抵挡环节的待判定伤害；蓝攻仍清空双方蓄力层数。
2. `ApplyResolution` 碰撞分支注释同步：同色碰撞不再"玩家先命中敌人"，直接进入抵挡环节。
3. 文档同步：GDD v0.10（§5.2.3 克制表、§5.2.5 规则行、修订记录）、AGENTS.md 战斗规则、DataTable_Spec 关联版本。

**结果/解决方案：** 编译通过，提交 `7ad5620`。PIE 待验证：蓝蓝/白白碰撞时双方不掉血、直接进入格挡/闪避判定；红红跳过。

**经验教训：** 同色碰撞的定位从"互砍后实时防御"收敛为"纯抵挡环节"，避免玩家在实时操作前就白吃/白打一刀；结算矩阵里"待判定伤害"与"已生效伤害"要区分清楚。

### 2026-08-05 | 策划 ⚡ + 程序

**事项：** 战斗动画表定案并接入 v1——新增 13 号表 `DT_CombatAnimConfig`（`FAnimRef` + `FCombatAnimRow`），`UBattleComponent` 按行动/结算/碰撞播放玩家与敌人 Montage，支持玩家碰撞准备姿态与"格挡成功=弹反+金色反击"连播。

**处理过程：**
1. 动画表定案（用户确认）：收刀动画留空、敌人动画先占位、蓝攻不分蓄力层级共用一条；玩家增加同色碰撞准备姿态（`ClashReady`，碰撞期间 Idle 切换，结束后回退）；敌人增加金色反击（红防成功触发）；格挡成功后先播 `BlockSuccess` 弹反、播完再接 `GoldCounter`。
2. DataTable_Spec v0.9 新增 13 号表 `DT_CombatAnimConfig`：一行一个实体（v1：`drifter`/`satan`），19 个 `FAnimRef` 动作列（Montage 软引用 + Section + PlayRate + BlendOutTime）；回落约定：`BlockFail`/`DodgeFail`/`ChargeInterrupted` 空 = 播 `Hurt`。设计说明落 `docs/superpowers/specs/2026-08-05-combat-anim-design.md`，实现计划落 `docs/superpowers/plans/2026-08-05-combat-animations.md`。
3. C++：新增 `FAnimRef`/`FCombatAnimRow`；`UCombatFormulaSubsystem::GetCombatAnimRow()` 读表；`UBaseCharacterAnimInstance` 暴露 `bClashReady`/`SetClashReady`；`UBattleComponent` 新增播放辅助（`GetCombatAnimRow/PlayCombatAnim/PlayActionAnim/PlayResolutionAnimations`），普通回合按"受击 > 金色反击 > 蓄力被打断 > 行动动画"编排，碰撞阶段播敌方碰撞攻击 + 玩家准备姿态，结算后复位；格挡成功连播用 `OnMontageEnded` 回调；入场 Montage 优先读表 `Entry`（BP 字段回退）；收刀时 `StopAllMontages` 并复位连播/准备状态；结算时败方播 `Death`、胜方播 `Victory`。
4. 三个数据表脚本（create/verify/export）同步 13 号表（不运行重建，避免覆盖手动编辑）。
5. 编译通过（7 次提交：`7b327aa..06cae5d`）。

**结果/解决方案：** 战斗动画接入代码完成，表资产与占位动画待编辑器操作：新建 `DT_CombatAnimConfig` 填 `drifter`/`satan` 行、填占位动画、ABP_Dale 增加 `ClashReady` 状态、PIE 验证（10 项清单见实现计划 Task 9）。

**经验教训：** 动画事件以 DataTable 一行一实体 + `FAnimRef` 软引用承载，播放/回落逻辑留在组件层，符合三层架构；状态型姿态（碰撞准备）沿用 `bWeaponDrawn` 的 AnimInstance 状态变量模式，避免 ABP 硬编码 Cast。

### 2026-08-05 | 程序 ⚡

**事项：** 战斗 HUD 结构调整——行动按钮增加文字描述（BP 侧）、同色碰撞提示从 HUD 移除、结算横幅独立为单独 HUD。

**处理过程：**
1. `UCombatHUDWidget` 移除碰撞提示（`ClashPanel` / `ClashPromptText` / `ClashWindowBar`）与结算横幅（`ResultPanel` / `ResultText`），删除 `NativeTick` 及 `ShowClashPrompt` / `ShowResult` 等接口；碰撞判定计时保留在 `UBattleComponent`（后续改用玩家/敌人专属碰撞动画提示）。
2. 新增 `UBattleResultHUDWidget`（C++ 基类，BindWidget `ResultText`）与独立结算 HUD：`UBattleComponent::BattleResultHUDClass`（默认自动加载 `/Game/UI/HUD/WBP_BattleResult`），`FinishBattle` 时 `AddToViewport(20)` 显示结果，胜利清理/失败重开/调试结束时移除。
3. 行动按钮文字描述由 WBP 静态文本承担（推荐文案：红防=防御姿态克制蓝攻；蓝攻=高伤害克制白攻；白攻=快速攻击克制红防；蓄力=最高2层强化蓝攻；技能=未实装禁用）。
4. 行动按钮悬停效果内置到 C++ 基类：`UCombatHUDWidget::HoverScale`（默认 1.1，可配 1.0 关闭），`NativeConstruct` 自动绑定 5 个按钮的 OnHovered/OnUnhovered 并围绕按钮中心缩放（`SetRenderScale` + `SetRenderTransformPivot`），无需 BP 连线。
5. 按 GDD 落实蓝攻蓄力门槛：蓝攻需要至少 1 层蓄力才能使用（玩家入口拦截 + HUD 0 层时禁用按钮），使用后清空蓄力层数（结算矩阵原有逻辑）；敌人 AI 同步遵守（0 层时不会选蓝攻，额外回合 0 层只能继续蓄力）。
6. 蓄力规则细化：红防/白攻会清空自身蓄力层数（不获得蓄力加成）；蓄力满上限后不能再选蓄力（玩家入口拦截 + HUD 禁用 + 敌人 AI 排除该选项）；敌人侧红防/白攻同样清空层数。
7. 战斗结束收刀：`UBattleComponent::SheathePlayerWeapon()` 在胜利清理/失败重开/调试结束时把武器直接挂回背部 socket（`AttachWeaponToSocket(BackSocketName)`，**无收刀动画**），并停止入场拔刀 Montage 防止通知回调再次拔刀；`bWeaponDrawn` 自动置回 false，玩家动画回到未拔刀 Idle。
8. 规则确认收口：额外回合触发条件=对方出白攻（白攻已清空出刀方自身层数，无需额外清空对方残留层数）；蓝攻打断蓄力/蓄力被蓝攻打断均清空对方层数（已实现）。文档同步（GDD v0.8 / DataTable_Spec v0.8 / AGENTS.md / 计划书）并推送远程 GitHub。
9. 收尾确认：`WBP_CombatHUD` 控件树已补全；`WBP_BattleResult` 暂不创建（基础战斗系统完善后再做，当前战斗结束仅日志提示未配置）；PIE 初步验证无明显问题，后续 Bug 另行修复。

**结果/解决方案：** 编译通过。待编辑器操作：`WBP_CombatHUD` 删除 5 个旧控件并补按钮描述；新建 `WBP_BattleResult`（父类 `UBattleResultHUDWidget`，含 `ResultText` 控件）。

**经验教训：** HUD 职责分层——状态区/行动区/实时提示/结算层分开承载；结算层独立 Widget 便于皮肤替换与后续扩展（重试/继续按钮等）。

### 2026-08-04 | 策划 ⚡ + 程序

**事项：** 为 Dale（漂泊者）定案并落地默认武器：新增 `dale_sword`（漂泊者短剑，单手剑系，SM_Sword_B）；主角改为可随身携带武器（非战斗背在背上、战斗时拔出，动作后续接入）；C++ 新增背上武器显示组件。

**处理过程：**
1. 方案确认：不重建表（编辑器数据由用户手动填写）；新增专属行 `dale_sword` 而非复用 `sword`；`drifter.DefaultWeaponID = dale_sword`。
2. 主角美术设定更新（v2.1）：从“不携带可见武器”改为“短剑背于背上、战斗时拔出”；补背部 socket `weapon_back` 设计。
3. C++：新增 `UWeaponVisualComponent`（`USceneComponent` 子类，`ABaseCharacter` 默认子对象；按 `EquippedWeaponID → GetWeaponRow → MeshAsset` 加载并挂到背部 socket，socket 缺失回退根节点+偏移，提供 `SetWeaponVisible` 供后续拔出/收回）；`UInventoryComponent` 新增 `OnWeaponChanged` 委托并在装备/卸下后广播。
4. Bug 修复：PIE 复现“武器停在场景中心不跟随人物”——日志定位 `Template Mismatch during attachment`（`WeaponMesh` 作为 `WeaponVisualComponent` 的嵌套默认子对象创建，UE 实例化时未正确挂到实例）与 `weapon_back` socket 不在身体网格实际骨架；修复为 `WeaponMesh` 在 `ABaseCharacter` 构造器中以 Actor 级默认子对象创建并 `SetupAttachment` 到显示组件，编译通过。
5. 入场拔刀接入：新增 `UAnimNotify_Hold`（按 Montage 类别归档在 `Animation/AnimNotifies/Entrance/`，不按 Montage 命名）；触发时调用 `UWeaponVisualComponent::AttachWeaponToSocket(HandSocketName)` 把武器从背上转移到 `weapon_hand_r`；组件新增 `HandSocketName` 与通用挂载方法 `AttachWeaponToSocket`，缓存网格引用，`BeginPlay` 背部挂载复用同一方法。
6. 入场 Montage 起始 Section：`UBattleComponent` 新增 `PlayerEntrySectionName`（默认 `Draw`），`PlayAnimMontage` 从该 section 开始，支持 `Draw_A_Great_Sword_1 → _2` 完整播放；Montage 内两段序列需在同一 section 内或通过 Next Section 串联。
7. 流程定名：Boss 开场动画归类为**剧情动画**（过场），玩家入场动画在战斗开始（Boss 剧情结束、战斗 HUD 已出现）后播放；`UBattleComponent` 现有 `PlayerEntryMontage` 时序满足，无需改流程。
8. 拔剑状态：`UWeaponVisualComponent` 新增 `bWeaponDrawn` / `IsWeaponDrawn()`，`AttachWeaponToSocket` 挂到 `weapon_hand_r` 时置 true、挂回 `weapon_back` 时置 false；`UBaseCharacterAnimInstance` 暴露 `bWeaponDrawn`（初始化缓存组件、每帧同步），ABP 直接读变量，无需 Cast 到 BP_Dale，供“拔剑后 Idle”等状态切换。
9. 文档/脚本同步：`create_datatables.py` 新增行与默认武器字段、DataTable_Spec v0.7、GDD v0.7、角色设计文档 v2.1；不运行表重建命令let。
10. 编辑器资产落地（用户手动）：导入 Fab 武器包 `SM_Sword_B`（SwordB 网格/材质/纹理）、Mixamo `Draw A Great Sword 1/2` 与 `Great Sword Idle`；生成 `Entrance/` 目录拔刀动画与 `MTG_DrawGreatSword`（Section `Draw`，挂 `AnimNotify_Hold`）；ABP_Dale 补充 `DefaultSlot` 节点并按 `bWeaponDrawn` 切换拔剑 Idle；DataTable 手填 `dale_sword` 行与 `drifter.DefaultWeaponID`。
11. 敌人动画蓝图模板：确认 UE 5.6 支持 Template Animation Blueprint（勾选 `bTemplate`，无 Target Skeleton，模板内不能直接引用动画资产）；新建 `ABP_Enemy_Template`（Parent Class = `EnemyAnimInstance`，不选骨架），搭建状态机与 `DefaultSlot` 结构；按敌人新建 `ABP_Satan` 时选择 Sevarog 骨架 + 模板，再填充 Idle/Walk/Run 等动画并指定到 BP_Satan 的 Mesh Anim Class。
12. PIE 验证：入场 Draw Montage 完整播放（`Hold` 通知触发时武器挂到 `weapon_hand_r`），拔剑后 Great_Sword_Idle 按 `bWeaponDrawn` 切换生效。
13. 项目管理：本次会话的编辑器资产与 DevLog 一并提交，并通过 HTTP 推送到 GitHub 远程仓库 `origin`（https://github.com/lott-coder/Kami.git），`master` 较远程领先 36 个提交。

**结果/解决方案：** C++ 编译通过（修正 `SetWeaponVisible` 参数名与 `USceneComponent::bVisible` 冲突；AnimNotify 头文件在 UE 5.6 位于 `Animation/AnimNotifies/AnimNotify.h`）；用户按操作流程手动填表、建 socket、微调挂载、把 Montage 中 `Hold` 通知替换为 `AnimNotify_Hold`；PIE 验证通过（入场拔刀 Montage + 拔剑 Idle 切换正常）；敌人动画蓝图模板 `ABP_Enemy_Template` 与子 ABP `ABP_Satan` 已创建并挂到 BP_Satan。

**经验教训：** 武器“实体”（`AWeapon`/`WeaponClass`）与“角色可见表现”（`UWeaponVisualComponent`/`MeshAsset`）职责分离，共用同一行数据；形参命名注意避免与 UE 基类成员冲突；UE 中场景子组件不要在组件构造器里用嵌套默认子对象创建（会 Template Mismatch），应作为 Actor 级默认子对象创建后 `SetupAttachment`；AnimNotify 类按 Montage 类别归档目录（`Animation/AnimNotifies/<类别>/`），不按具体 Montage 命名，便于同类通知复用；模板动画蓝图无 Target Skeleton、不能直接引用动画资产，只搭状态机结构，动画由子 ABP 按骨架填充，便于同类敌人复用。

### 2026-08-02 | 程序 ⚡

**事项：** 战斗系统 v1（Dale vs Satan）开发落地：战斗会话/HUD/AI C++、12 号表 `DT_BattleStage`、输入与蓝图接线、落点与入场动画修复。

**处理过程：**
1. 计划书：产出并确认 `Plans/combat-system.md`（大纲 → 完整计划书）；定案决策——普通中立开局、失败回到 Boss 触发点重开、战斗固定镜头沿用玩家 SpringArm（策划可调）、C++ 基类 + BP 皮肤、敌人 AI v1 全随机 `[PLAYTEST]`。
2. 行动输入改为纯 HUD 鼠标点击（红/蓝/白/蓄力/技能），移除 1-5 键位；格挡 E / 闪避 Shift 保留。
3. 站位/镜头参数收进 DataTable：新增 12 号表 `DT_BattleStage`（`FCombatStageRow`：`PlayerBattleOffset` / Boss 本地空间开关 / 双方朝向偏航 / `CameraPitch` / `CameraYawOffset` / `CameraArmLength` / `CameraFOV` / `SpringSocketOffset` / `SpringTargetOffset` / 镜头滞后开关与速度）；`UCombatFormulaSubsystem::GetBattleStageRow()` 读取，USTRUCT 默认值回退；DataAsset 方案（`UBattleStageConfig`）已废弃。`create/verify/export_datatables.py` 同步并已用命令let 重建校验；DataTable_Spec v0.6。
4. C++ 新增：`BattleTypes.h`、`UBattleComponent`（战斗状态机、16 项克制结算矩阵、同色碰撞实时格挡/闪避、额外回合、闪避Buff、胜利/失败与失败重开）、`UEnemyCombatAIComponent`、`UCombatHUDWidget`；`UBossIntroComponent` 增加 `OnIntroFinished` 委托联动开战；`ARole::Look` 增加战斗锁定门控；每次改动即时编译通过。
5. 编辑器接线（用户手动）：`IA_CombatBlock` / `IA_CombatDodge` / `IMC_Combat`、`WBP_CombatHUD`（`Content/UI/HUD`）、BP_Dale 挂 `BattleComponent`、BP_Satan 加 Tag `Boss` + `UEnemyCombatAIComponent`。
6. 修复：玩家进入战斗从天上落下 → 落点地面射线检测（Z=地面+胶囊半高）+ 清除移动残留速度；新增玩家入场 Montage 占位 `PlayerEntryMontage`（空值跳过，有值则播完再进第 1 回合）。
7. 资产迁移：`WBP_CombatHUD` 从 `Content/HUD` 迁至 `Content/UI/HUD`（UE 重命名接口 + 删除旧路径）；迁移过程导致 BP_Dale 的 BattleComponent 4 个引用被清空，需编辑器内重新指定。

**结果/解决方案：** 全部 C++ 编译通过，提交 `9d7b6bf..d9e273e`（13 个提交）。待办：WBP_CombatHUD 补 15 个 BindWidget 控件、BP_Dale 重设 4 个引用、`PlayerEntryMontage` 资源、PIE 全流程验证、文档同步收尾。

**经验教训：**
- UE 命令let 自动保存蓝图时，若被引用资产编译报错，可能把 BP 上的引用清空——资产迁移后必须在编辑器中核对引用。
- 大体型 Boss 根节点 Z 不等于地面高度，传送玩家到“Boss 位置+偏移”必须做落地检测，否则会从空中坠落。
- 全局单例参数（战斗舞台）放 DataTable 与 `DT_CombatParams` 口径一致，且能走 CSV/脚本重建，比 DataAsset 更符合本项目数据驱动约定。

### 2026-08-01 | 程序 ⚡

**事项：** 按用户方案定案落地：战斗规则/数值字段/武器槽/文档同步（GDD v0.6 / DataTable_Spec v0.4）。

**处理过程：**
1. DT_CombatParams 新增 6 参数：`CritDamageMultiplier`(1.5)、`DodgeBuffDamageScale`(1.2)、`DodgeBuffTurns`(1)、`FirstStrikeDamageScale`(0.3)、`GoldAttackDamageMin/Max`(25/35)，均标 [PLAYTEST]。
2. DT_CharacterConfig 新增 `DailySmokeRecoveryMin/Max`（艾斯=1/2，每日烟回复）；DT_AreaConfig 新增 `bFirstLoopOnly`（洞穴=True，UnlockRound 1→0）。
3. `AttributeNames` 新增 14 个常量（武器倍率/修正、暴击、闪避Buff、技能树被动），子系统为角色/敌人注入全部基值。
4. 公式改造：白/蓝攻从属性读取武器修正，加入 `NextAttackDamageScale`（闪避Buff）；蓝攻加入暴击判定（BlueCritChance × CritDamageMultiplier）；新增 `CalculateGoldDamage`（金色攻击=反击，×(1+CounterDmgBonus)）。
5. `UInventoryComponent` 新增武器槽 `EquipWeapon/UnequipWeapon`（修正全部走 AddModifier），`ABaseCharacter` 自动装备 `DefaultWeaponID`。
6. 文档同步 26 项决策：同时选择/共用回合、红色反击、蓄力抵抗白攻+额外回合、蓝攻全额打断蓄力、红vs红跳过、金色攻击、暴击、闪避Buff、先制参数、时间回溯技能停用、货币定名金币、主角=Dale 无能力者、洞穴仅首轮回、技能树/博士第0次轮回、多阶段与难度暂缓、面具改掉落不走烟转化、SpwanAreas 定案保持 FString。

**结果/解决方案：** 编译通过；11 张表重建并校验（新字段值、洞穴 UnlockRound=0、BP 类引用保留）。

**经验教训：** 武器/装备效果统一走 AddModifier，公式只读属性，保持"数据驱动"单一链路；方案先定案再落代码，避免返工。

### 2026-08-01 | 策划 ⚡

**事项：** 解锁轮回口径修正 + 全文档逻辑/运行链路检查。

**处理过程：**
1. 口径修正：艾斯 `UnlockCondition_Round` 1→0（首次游戏即可加入，与 GDD §3.3 前5分钟剧情一致）；边境/地狱 `UnlockRound` 4→3（第4次游戏解锁）；同步 GDD v0.5 / DataTable_Spec v0.3 / 11 张资产并校验。
2. 全文档检查（GDD 19 章 + DataTable_Spec 11 表 + 代码链路）发现：
   - **战斗规则缺口（阻塞战斗开发）**：回合结算时序未定义；"红色攻击"概念未定义（表里有红攻倍率但行为表只有红防）；蓄力打断规则 §5.2.3 与 §5.2.4 互相矛盾；红vs红对抗未闭环；被克制伤害比例、反击规则、闪避Buff、暴击机制、先制伤害、状态回合语义均未参数化。
   - **数据链路缺口**：烟→面具转化无字段（ConvertedItemID 只能指向消耗品）；面具掉落无敌人关联；技能树 EffectType 无字典；边境之烟"HP上限提升"、撒旦之烟"终极技能/结局道具"无对应行；日期/烟储备/武器槽/击杀回血执行链路未落地。
   - **策划案可优化点**：货币名称待定影响本地化；FTUE 与 §3.3 前5分钟流程有出入；Boss 多阶段与高难度无数据模型等。
3. 完整清单与战斗开发计划已提交用户确认（本轮不实现战斗代码）。

**结果/解决方案：** 解锁修正已落库并校验（ace=0、border/hell=3）；检查报告输出给用户确认。

### 2026-08-01 | 策划

**事项：** GDD v0.3→v0.4 与 DataTable_Spec v0.1→v0.2 一致性修订，确认文档完整性。

**处理过程：**
1. GDD 对齐：初始HP口径（100=全局回退，主角 drifter=120）、艾斯解锁 第2次轮回→第1次轮回（对齐 §6.4 与 UnlockRound=1）、边境/地狱 第3次+→第4次起（对齐 §7.1 与 UnlockRound=4）、格挡/闪避窗口 0.25/0.35 标记 [PLAYTEST]、治愈之烟兑换 80、§19.2 指向 DataTable_Spec。
2. Spec 对齐：敌人表新增 WalkSpeed/SprintSpeed/LandingLockTime 列、面具倍率默认值 0.0→1.0 并注明倍率语义、枚举清单更新（10 个已实现，EAreaID/ECurrencyType 不采用）、表索引行数实况、修订记录 v0.2。
3. 剩余 `[待定]` 项保留不强行确定（货币名称、satan/time_mage HP、inept_char 解锁、武器/面具/技能数量、耐力/无敌帧），已在文档中列明。

**结果/解决方案：** 两份文档版本同步为 v0.4 / v0.2，GDD 与数据表的口径一致。

### 2026-08-01 | 程序 ⚡

**事项：** 战斗地基 — UCombatFormulaSubsystem 接入蓝/白攻伤害公式。

**处理过程：**
1. `CalculateWhiteDamage` = (Rand(15~25) × 角色BaseDamageScale × 武器白攻Scale × 白攻面具倍率) + 角色白攻加成 + 武器Mod + 技能树加值。
2. `CalculateBlueDamage` = (Rand(20~30) × 角色BaseDamageScale × 武器蓝攻Scale × 蓝攻面具倍率 × 蓄力倍率 1.0/1.5/2.25) + 角色蓝攻加成 + 武器Mod + 技能树加值；蓄力层数按 MaxChargeStacks 截断。
3. 属性值从 `UAttributeComponent` 读取（可空回退 1.0/0.0），表缺失时用 USTRUCT 默认值。

**结果/解决方案：** 编译通过；公式与 GDD §5.2.7 / DataTable_Spec §4.4.1 完全一致，供后续战斗系统调用。

**经验教训：** 公式统一收口在 Subsystem；USTRUCT 默认值是唯一回退源。

### 2026-08-01 | 策划 ⚡

**事项：** 通读 GDD v0.3 与 DataTable_Spec v0.1，确认数据逻辑/运行链路，按规范补全 3 张基础表。

**处理过程：**
1. 确认链路：DataTable → UCombatFormulaSubsystem（跨表合并）→ UAttributeComponent / UInventoryComponent → 角色/敌人；蓝/白攻伤害公式尚未接入子系统（战斗系统未落地，属后续工作）。
2. 核对 11 张表与 GDD 数值一致性：武器/面具/技能树/经济/区域均对齐；DT_CombatParams 原本已完整（15 字段 = 规范值），未改动数值。
3. 发现并修正：`drifter` 行 MaxHP 100→120、BaseDamageScale 1.1→1.05、WhiteAttackBonus 0→3、WalkSpeed 200→300（规范 §4.3）；`satan` 行补显示名、AI 偏好 Balanced→Adaptive、掉落 `satan_smoke`、区域 `hell`（规范 §5.4）；新增 ace / inept_char / doctor / time_mage 与 7 种敌人行。
4. 保留 BP 类引用：drifter→BP_Dale、satan→BP_Satan（CSV 往返导入成功）。
5. 占位处理：`[待定]` 项（inept_char 解锁条件、time_mage HP、satan MaxHP=80 保持原值）用 0/原值占位，待策划确定。

**结果/解决方案：** 11 张表全部生成并校验：CombatParams 1 行、CharacterConfig 5 行、EnemyConfig 8 行、Weapon 4、Mask 5、Smoke 9、Skill 10、SkillTree 15、Area 6、Consumable 10、Economy 8；BP 类引用保留验证通过。

**经验教训：** 重建带 `TSubclassOf` 引用的表时，CSV 必须使用 UE 导出的类路径格式，否则会丢失蓝图绑定；`[待定]` 数值在 DataTable 中只能先用占位值并在 DevLog 标注。

**可优化点（后续）：** ① 伤害公式函数（Blue/WhiteDamage）未实现；② `AttributeNames` 缺技能树 EffectType 对应常量（BlueCritChance / WhiteDmgBonus / InterruptDmgScale / BlockDmgReduce / DodgeBuffBonus）；③ `Movement_BackwardSpeedScale` / `FirstStrikeDisableChargeTurns` / `RunAwayHPThreshold` 等表字段代码未消费；④ 武器修正未接 AddModifier。

### 2026-08-01 | 程序 ⚡

**事项：** 完成代码审计后续优化 — 动画实例去重、面具/装备 AddModifier 链路、补齐 7 张 DataTable USTRUCT 与 8 张数据表资产。

**处理过程：**
1. 新增 `UBaseCharacterAnimInstance` 公共基类，`URoleAnimInstance` / `UEnemyAnimInstance` 改为薄壳（ABP 兼容），消除两份重复的逐帧逻辑。
2. 新增 `UInventoryComponent`（面具装备槽）：`EquipMask` / `UnequipMask` 把 `FMaskConfigRow` 的修正字段转为永久 AddModifier（倍率类 Multiply / 百分比类 Add），按 SourceTag 整组移除；`ABaseCharacter` 自动创建组件并按 `DT_CharacterConfig.DefaultMaskID` 自动装备；`AttributeNames` 新增 6 个面具属性，`UCombatFormulaSubsystem` 提供 `GetMaskRow` 并注入基值。
3. 按 DataTable_Spec v0.1 补齐 `FWeaponConfigRow` / `FSmokeConfigRow` / `FSkillConfigRow` / `FSkillTreeConfigRow` / `FAreaConfigRow` / `FConsumableConfigRow` / `FEconomyConfigRow` 及配套枚举；新增 `AWeapon` / `ASkill` / `AConsumable` 占位基类（`TSubclassOf` 引用需要，UHT 不允许纯前置声明）。
4. 用编辑器命令let + Python 创建 8 张表：DT_WeaponConfig(4) / DT_MaskConfig(5) / DT_SmokeConfig(9) / DT_SkillConfig(10) / DT_SkillTreeConfig(15) / DT_AreaConfig(6) / DT_ConsumableConfig(10) / DT_EconomyConfig(8)，保留 `Scripts/create_datatables.py` 与 `verify_datatables.py` 供重建/校验。

**结果/解决方案：** 每步编译通过；8 张表创建完成，行数与关键字段值经命令let 校验正确（含面具倍率、技能跨轮回保留、消耗品携带量）。

**经验教训：** UE 5.6 Python 的 `DataTable` 没有 `add_row`，需要用 `fill_from_csv_string`；CSV 未显式填写的字段会变成 0/False 而非 USTRUCT 默认值，倍率/布尔类字段必须逐行显式写入。Python 布尔属性名不带 `b_` 前缀。

### 2026-08-01 | 程序 ⚡

**事项：** 代码审计后修复底层基础问题 — 敌人属性初始化断链、三层架构落地（新增 UCombatFormulaSubsystem）、动画性能、BossIntro 生命周期。

**处理过程：**
1. 修复敌人属性无自动初始化路径：`ABaseCharacter` 新增 `bAttributesInitialized` 标记，`BeginPlay` 兜底逻辑改为"未初始化则初始化"；`AEnemy::InitializeAttributes` 完成后置位。
2. 按三层架构新建 `UCombatFormulaSubsystem`（GameInstanceSubsystem）：持有并缓存 DataTable 引用，负责跨表合并（DT_CharacterConfig / DT_EnemyConfig + DT_CombatParams）与 modifier 最终值公式；`UAttributeComponent` 回归纯存储（不再直接 `LoadObject` 表），硬编码回退值改为 USTRUCT 默认值。
3. `FEnemyConfigRow` 新增 `WalkSpeed` / `SprintSpeed` / `LandingLockTime` 列；`AEnemy` 逐字段复制改为整体持有 `FEnemyConfigRow EnemyConfig`（消除双源真相）。
4. 动画每帧 Sphere Sweep 改为仅在空中时扫描，避免地面状态每帧物理查询。
5. BossIntro：接好 `CinematicCameraOut` / `CameraBlend*` 参数（新增 `CameraBlendInTime`）；`PlayIntroSequence` 失败时回滚到 Idle 并解锁输入；回到 Idle 时重新启用触发球（修复 ResetIntro 后无法重播的问题）。
6. 其他：`AddMappingContext` 判空；删除 `bIsSprinting` 死状态与 `ADale::BeginPlay` 空重写；`GetFinal` 移除 `const_cast`（缓存改 mutable）。

**结果/解决方案：** HoleEditor Win64 Development 编译通过（约 81s）。

**经验教训：** 三层架构中"跨表/公式"必须进 Subsystem，组件只存状态；DataTable USTRUCT 默认值应作为唯一回退源，避免散落硬编码。

### 2026-08-01 | 项目管理

**事项：** AGENTS.md（AI 面向的项目 Memory）改为英文；`.codex/` 目录纳入 git 跟踪；确认网络沙箱机制。

**处理过程：**
1. 按用户要求将 AGENTS.md 全部内容改写为英文，规则与约定保持不变，并新增"语言约定"（AGENTS.md 为 AI 面向记忆，使用英文；GDD/DevLog 等用户面向文档保持中文）。
2. `git add .codex/config.toml`（仅 GitHub MCP 配置，无密钥）并提交。
3. 排查 `CODEX_SANDBOX_NETWORK_DISABLED=1`：由 Codex 桌面端按会话注入沙箱策略（对应 CLI 的 `sandbox --sandbox-state-disable-network`），不是系统环境变量；需要联网的命令可通过权限审批在沙箱外运行。

**结果/解决方案：** AGENTS.md 已转英文并提交；.codex 已纳入版本控制。

**经验教训：** AGENTS.md/DevLog 为 UTF-8 无 BOM，PowerShell 默认读取会乱码，读写需显式指定 UTF-8。

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

### 2026-07-31 | 程序 ⚡

**事项：** 实现 Boss 出场动画系统 — `UBossIntroComponent` 模块化组件，Tag 驱动的玩家检测，可复用于任意 Boss。

**处理过程：**
1. `Component/BossIntroComponent.h/.cpp` — 新建 `UBossIntroComponent : UActorComponent`（ClassGroup=Boss, Blueprintable, BlueprintSpawnableComponent），提供三态状态机（Idle/Playing/Combat）、球形触发器、跳过输入绑定、玩家输入锁定/解锁
2. `Component/BossIntroComponent.cpp` — 全 C++ 实现 Level Sequence 播放（`CreateLevelSequencePlayer` + `ALevelSequenceActor::SetBindingByTag` 动态绑定 Player/Boss Actor）、镜头混合切换（`SetViewTargetWithBlend` Cubic/EaseOut）、`OnFinished` 委托回调；四个事件从 BlueprintImplementableEvent 改为 protected virtual 方法（`PlayIntroSequence` / `StopIntroSequence` / `CinematicCameraIn` / `CinematicCameraOut`）
3. `Character/Role.h/.cpp` — 新增 `DisablePlayerInput()` / `EnablePlayerInput()` 薄封装（内部调用堆叠安全的 `APawn::DisableInput`）；`BeginPlay` 中添加 `Tags.Add("Player")` 标签
4. `Hole.Build.cs` — 添加 `"LevelSequence"` + `"MovieScene"` 模块依赖
5. 编译通过（0 error / 0 warning）

**设计理由：**
- **Tag vs Cast**：用 `ActorHasTag("Player")` 检测玩家 — FName 索引比较（~几个 CPU 周期）vs `Cast<ARole>` 的 UHT 类型层级遍历（~几十个周期），且零耦合
- **组件 vs 基类**：`UActorComponent` 方案让 Boss 选择性挂载，AEnemy 不膨胀
- **全 C++ 动画**：Level Sequence 播放、动态 Actor 绑定、镜头混合切换均在 C++ 完成，BP 侧零节点
- **APawn 内置机制**：`DisableInput`/`EnableInput` 是 UE5 堆叠安全的 API；`ClearBindingsForObject(this)` 精确解绑跳过输入

**后续待办：** 在 BP_Satan 上挂载组件，创建 `IA_Skip` InputAction 资产，制作 `LS_SatanIntro` Level Sequence

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

### 2026-07-31 | 程序

**事项：** 动画实例类重命名 — DaleAnimInstance → RoleAnimInstance，新建 EnemyAnimInstance。

**处理过程：**
1. `DaleAnimInstance.h/.cpp` 重命名为 `RoleAnimInstance.h/.cpp`，类名改为 `URoleAnimInstance`，持有 `TObjectPtr<ARole> OwnerRole`
2. 新建 `EnemyAnimInstance.h/.cpp`，`UEnemyAnimInstance : UAnimInstance`，持有 `TObjectPtr<AEnemy> OwnerEnemy`，提供相同的地面移动数据接口
3. 二进制编辑 `ABP_Dale.uasset`：将所有 12 处 `DaleAnimInstance` 替换为 `RoleAnimInstance`（等长字符串，安全替换）
4. `Role.cpp` BeginPlay 中 `Tags.Add(FName(TEXT("Player")))`，供 BossIntroComponent 用 Tag 检测代替 Cast<ARole>

**设计理由：**
- 动画实例层级与角色层级对齐：ABaseCharacter → ARole(Uses RoleAnimInstance) / AEnemy(Uses EnemyAnimInstance)
- Tag 检测（FName 索引比较）比 Cast<ARole>（UHT 类型层级遍历）更快，且零耦合

---

### 2026-07-31 | Bug修复

**问题：** 每次打开编辑器时地图被重置为 OpenWorld 模板。

**解决：** `DefaultEngine.ini` 中 `GameDefaultMap` 从 `/Engine/Maps/Templates/OpenWorld` 改为 `/Game/Map/Untitled`。

---

### 2026-07-31 | 程序 ⚡

**事项：** 创建 Boss 出场动画系统 — `UBossIntroComponent`。

**处理过程：**
1. 新建 `Component/BossIntroComponent.h/.cpp`，UActorComponent（Blueprintable, BlueprintSpawnableComponent）
2. 三态状态机：Idle → Playing → Combat，通过 `ResetIntro()` 回到 Idle
3. 触发器：`USphereComponent`（CreateDefaultSubobject），仅响应 ECC_Pawn Overlap，半径可配置
4. 镜头管理：播放时 `SetViewTargetWithBlend(SequenceActor)`，结束时 `SetViewTarget(Player)` 即时切回
5. Level Sequence 播放：`ULevelSequencePlayer::CreateLevelSequencePlayer` + `bPauseAtEnd` + `OnFinished` 委托
6. 跳过支持：EnhancedInput `IA_Skip`，绑定到玩家 Pawn 的 EIC，Playing 进入时绑定/退出时解绑
7. 输入锁定：`ARole::SetCinematicLocked(bool)` 门控 Move/Sprint/Jump，不影响 EnhancedInput 处理
8. `Hole.Build.cs` 添加 `"LevelSequence"` 和 `"MovieScene"` 模块依赖

**设计理由：**
- 组件模式：任意 Boss 只需在蓝图挂载组件 + 配置属性即可，无需修改 AEnemy 基类
- `ActorHasTag("Player")` 替代 `Cast<ARole>()`：FName 索引比较比 UHT 类型层级遍历快一个数量级
- 纯 C++ 实现：Sequence 播放、镜头切换、输入绑定全部在 C++ 完成，无需 BP 事件
- 离开触发区域即重置到 Idle，支持每次进入都重播（回合制战斗不需要持久化标记）

**修复历程：**
1. **相机不回玩家**：根因是 `CompleteIntro` 只把指针置 nullptr 但未销毁 `ALevelSequenceActor`，该 Actor 仍存活抢占相机。修复：`StopIntroSequence` 中调用 `SequenceActor->Destroy()`，`CompleteIntro`/`SkipIntro` 先 Stop+Destroy 再 `SetViewTarget(Player)`
2. **Skip 按键无效**：`BindAction` 使用了函数名字符串版本（需 UFUNCTION），但 `OnSkipPressed` 没有 UFUNCTION 宏。修复：改用模板方法指针版本 `&UBossIntroComponent::OnSkipPressed`
3. **IA_Skip 无按键映射**：IA_Skip 未添加到 IMC_PlayerInputMapping，EnhancedInput 链路断裂（按键→IMC→IA→回调）。需在编辑器中手动映射

**经验教训：**
- `ULevelSequencePlayer::CreateLevelSequencePlayer` 创建的 `ALevelSequenceActor` 必须手动 `Destroy()`，否则即使序列播放完毕也持续占用相机控制
- EnhancedInput 的 `BindAction` 方法指针版本（模板）不需要 UFUNCTION，函数名字符串版本需要
- `SetViewTarget` 必须在 `Stop()`+`Destroy()` 序列 Actor 之后调用，否则会被覆盖

---

### 2026-07-31 | 程序

**事项：** 动画蓝图修复 — ABP_Dale 在 C++ 类重命名后显示损坏。

**处理过程：**
二进制编辑 `ABP_Dale.uasset`：将 `DaleAnimInstance` 替换为 `RoleAnimInstance`（12 字节等长），蓝图正常打开。

**经验教训：** `.uasset` 中类名引用是行内文本，等长替换安全。不等长需要修改偏移表，不推荐手动操作。

---

## 关键决策记录

> 此处记录项目中所有重要的 ⚡ 决策（如引擎版本选择、核心机制变更、技术方案选型等），便于后续回顾"为什么当时这么做"。

| 日期 | 决策 | 理由 | 影响 |
|------|------|------|------|
| 2026-08-09 | ⚡ 序章教学敌人定案 + C++ 落地：`apprentice_cave` 单场完整教学 + 玩家先制开场 + 无掉落必逃 | 教学集中避免打断节奏；玩家先制支撑"主角发现→突袭"叙事；烟只在击败时掉落保持语义一致 | Plans/tutorial-enemy.md、GDD v0.19、DataTable_Spec v0.15、EnemyCombatAIComponent/BattleComponent/HUD |
| 2026-08-02 | ⚡ 战斗行动选择只用 HUD 鼠标点击（不用 1-5 键位） | 行动按钮即 UI 核心交互，避免键位与实时格挡/闪避混淆 | 战斗输入系统、Task 10 资产清单 |
| 2026-08-02 | ⚡ 战斗舞台配置入 `DT_BattleStage`（DataTable，非 DataAsset） | 与 `DT_CombatParams` 单例口径一致，策划调参 + CSV/脚本可重建；战斗/非战斗两套状态由保存恢复保证 | 12 号表、`FCombatStageRow`、DataTable_Spec v0.6 |
| 2026-08-02 | ⚡ 战斗失败回到 Boss 触发点直接重播入场动画 | 验证循环最短，先保证战斗闭环可重测 | 失败流程、`ResetIntro` + `UpdateOverlaps` 重开逻辑 |
| 2026-08-04 | ⚡ 敌人动画蓝图采用 Template Animation Blueprint（无骨架模板 + 每敌人子 ABP） | 同一套状态机/结构跨敌人复用；模板不能直接引用动画资产，由子 ABP 按骨架填充 | `ABP_Enemy_Template`、`ABP_Satan`、BP_Satan Mesh Anim Class 配置 |
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
| 开场后武器已切换但拔刀动画不播放 | ABP 缺少 `DefaultSlot` 节点；补充后正常播放 | 2026-08-04 |
| Montage 从指定 Section 跳转不生效 | 确认 Montage 已保存落盘；`PlayAnimMontage(Montage, 1.0, "Draw")` 从 `Draw` Section 起始 | 2026-08-04 |
| 武器停在场景中心不跟随人物 | `Template Mismatch`：网格用嵌套默认子对象创建；改为 Actor 级默认子对象 + `SetupAttachment` | 2026-08-04 |
| 模板动画蓝图能否直接引用动画资产 | 不能；模板无 Target Skeleton，只搭结构，动画在子 ABP 中填充 | 2026-08-04 |
| 多个 Tag=Boss 敌人同关卡时，入场序列播完不进战斗 | 战斗组件原先只绑定第一个 Boss 的 `OnIntroFinished`；改为绑定全部，按完成者（委托携带敌人参数）开战 | 2026-08-09 |
| 教学战入场动画播完编辑器崩溃 | HUD `NativeConstruct` 在 `BindToBattle` 前对空 `Battle` 弱引用调用 `GetTutorialHintText`；调用前加 `IsValid()` 判空 | 2026-08-09 |

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
| v0.6 | 2026-08-01 | 战斗规则/数值定案、武器槽与 12 号表 `DT_BattleStage` | DataTable_Spec v0.6 |
| v0.7 | 2026-08-04 | 主角默认武器 `dale_sword`（SM_Sword_B）+ 随身携带/入场拔剑 + 敌人动画蓝图模板 | DataTable_Spec/GDD/角色设计文档同步；C++ 武器显示组件与拔剑状态 |
