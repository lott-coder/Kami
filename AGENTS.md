# Kami Project Memory (AGENTS.md)

> This file is Codex's persistent project memory for this repository, migrated from the original Claude Code project memory.
> Update rule: for changes to project conventions, architecture principles, or development process, update this file first, then sync to DevLog.

## 1. Project Overview

- **Engine:** Unreal Engine 5.6, project root `d:\UE5\UE_project\Kami`, UE project in the `Hole/` subdirectory
- **Game:** Hole — a roguelike semi-turn-based RPG with red/blue/white three-color counters + real-time parry/dodge + time loop
- **Key documents:**
  - `GDD_Outline.md` — game design document (v0.26, 19 chapters); the reference baseline for design and implementation
  - `DataTable_Spec.md` — data table specification (v0.20, 13 tables); the basis for C++ USTRUCT definitions
  - `DevLog.md` — development log (timeline + key decisions + FAQ)
- **Version control:** Git (branch master), remote managed via GitHub MCP; generated directories (`Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Saved/`, etc.) are not tracked

## 2. Development Process Conventions

### Document conventions
- `GDD_Outline.md`: update the relevant chapter on design changes and bump the version number; `[待定]` = pending decision, `[PLAYTEST]` = parameter requiring prototype verification; do not force a value once marked
- `DataTable_Spec.md`: sync whenever GDD values change or a new system is added; consult this document before writing C++ USTRUCTs
- `DevLog.md` has three record types: timeline log (date + category + problem + handling + lesson), key decisions table (⚡ records "why we did it this way at the time"), and FAQ
  - Category tags: `策划 (Design)` `程序 (Code)` `美术 (Art)` `音频 (Audio)` `UE引擎 (UE Engine)` `项目管理 (Project Mgmt)` `Bug修复 (Bug Fix)`
  - Merge changes from the same conversation/feature area into one entry instead of creating separate entries per item
- **Language:** this file is AI-facing project memory and is intentionally written in English; user-facing documents (`GDD_Outline.md`, `DataTable_Spec.md`, `DevLog.md`) remain in Chinese

### Operational conventions
- **Retry limit 3:** any task (compile, code change, bug fix, etc.) may be retried at most 3 times; if it still fails, stop immediately and send the complete error to the user for a decision (unless the user explicitly asks to "keep retrying until success")
- **Build stall rule (2026-08-06):** if a build stalls (e.g. UBA repeatedly logs "Delaying ... due to memory pressure" with no new outputs for several minutes), cancel the stale Build.bat/UnrealBuildTool processes immediately and relaunch the build; do not keep waiting on the old process. When the machine is under memory pressure, relaunch with `-NoUba` so UBT compiles locally without UBA throttling.
- **Confirm the path before creating new files:** always confirm the target path with the user before creating any new file; editing existing files is exempt
- **Code reading route (2026-08-07):** prefer `rg -n` to index large files first, then read only the target function bodies in one batched call; do not re-read unchanged files within a session; verify uncertain engine APIs in their headers before writing code; reuse AGENTS.md/DevLog conclusions instead of re-deriving them from source
- **At the end of each conversation:** consolidate important information back into this file

### Animation blueprint conventions
- Player/Dale ABP reads `bWeaponDrawn` directly from `UBaseCharacterAnimInstance` (no Cast to BP_Dale) for state switching (e.g. drawn Great Sword Idle).
- Enemy ABPs use UE Template Animation Blueprints: `ABP_Enemy_Template` (Parent Class `EnemyAnimInstance`, no Target Skeleton, no direct animation asset references) owns the state machine / DefaultSlot structure; each enemy ABP (e.g. `ABP_Satan`) selects the enemy skeleton + template and fills real animation assets, then is assigned to the enemy BP's Mesh Anim Class.
- AnimNotify classes are organized by Montage category directories under `Animation/AnimNotifies/<Category>/`, not named per Montage.

## 3. Data Architecture Principles (all entities follow)

Applies to characters, enemies, weapons, masks, skills, consumables, areas, and **all other data and config tables**.

### Three-layer separation (common to all DataTables)
1. **USTRUCT (FTableRowBase)** — pure data + editor defaults; no cross-table lookups, no calculation logic, no `PostLoad()`
2. **UGameInstanceSubsystem** — cross-table formula calculation / multi-table data merging (e.g. `UCombatFormulaSubsystem`, `UEconomySubsystem`; split by domain)
3. **Runtime attribute component** — stores runtime state loaded from DT (`UAttributeComponent` shared by player/enemy, `UInventoryComponent` for items/equipment)

Judgment standard: a USTRUCT may only contain "what is the value of this cell in the Excel row"; anything that needs to "look at another table" or "compute" belongs in a Subsystem.

### Runtime attributes: TMap + modifier stack
All attributes affected by buff/debuff/equipment/passives use `UAttributeComponent`:

```cpp
TMap<FName, float> BaseAttributes;          // base values loaded from DT
TMap<FName, float> CachedFinalAttributes;   // base + all modifiers combined
TArray<FAttributeModifier> ActiveModifiers; // unified stack for buff/debuff/equipment/passives

struct FAttributeModifier
{
    FName AttributeName;  // "MaxHP", "WhiteAtkBonus", "AIDifficulty", etc.
    float Value;          // modifier value or multiplier
    EModifierOp Op;       // Add / Multiply
    int32 RemainingTurns; // 0 = permanent (equipment/mask/skill-tree passive/permanent item)
};
```

**Exceptions not stored in TMap:** high-frequency runtime state (`CurrentHP`, etc.), entity identity fields (`CharacterID`, `bIsPlayable`, `DisplayName`), asset references (`PortraitTexture`, `MeshAsset`).

Players and enemies share `UAttributeComponent`; only initialization reads different DataTables:
- `ABaseCharacter::InitializeAttributes()` → DT_CharacterConfig + DT_WeaponConfig + DT_MaskConfig
- `AEnemy::InitializeAttributes()` → DT_EnemyConfig

### Entity data flows
```
DT_CharacterConfig / DT_WeaponConfig / DT_MaskConfig / DT_SkillTreeConfig
    → UCombatFormulaSubsystem → UAttributeComponent (player)
DT_EnemyConfig → UCombatFormulaSubsystem → UAttributeComponent (enemy)
DT_SmokeConfig → drop/conversion system (pure item output)
DT_SkillConfig → skill system (effects applied via AddModifier; skills never modify attributes directly)
DT_EconomyConfig → UEconomySubsystem
DT_AreaConfig → level management (pure static config)
DT_ConsumableConfig → inventory system → AddModifier on use
```

### Weapon visual mounting (2026-08-04)
- `UWeaponVisualComponent` (a `USceneComponent` subclass, default subobject on `ABaseCharacter`) mounts the equipped weapon's `MeshAsset` (from `DT_WeaponConfig`) onto the character's back socket (`BackSocketName`, default `weapon_back`; falls back to mesh root + `BackAttachOffset`).
- `WeaponMesh` is an actor-level default subobject created in `ABaseCharacter` and attached via `SetupAttachment(WeaponVisualComponent)`; do NOT create it as a nested default subobject inside the component constructor (UE fails to instance it → `Template Mismatch during attachment`, mesh stays at world origin).
- `UWeaponVisualComponent::AttachWeaponToSocket(FName)` moves the weapon between sockets (`BackSocketName` default `weapon_back`, `HandSocketName` default `weapon_hand_r`); `UAnimNotify_Hold` (under `Animation/AnimNotifies/Entrance/`) triggers it during the player entry Montage. AnimNotify classes are organized by Montage category directories (`Animation/AnimNotifies/<Category>/`), not named per Montage.
- `UWeaponVisualComponent::IsWeaponDrawn()` (backed by `bWeaponDrawn`) is set true when the weapon is attached to `HandSocketName` and false when returned to the back; the AnimBlueprint reads it to switch Idle/locomotion variants (e.g. `Great_Sword_Idle`).
- `UBaseCharacterAnimInstance::bWeaponDrawn` mirrors `IsWeaponDrawn()` (component cached once at init, bool copied per frame); AnimBlueprints read this variable directly instead of casting to concrete Blueprint classes.
- `UInventoryComponent::OnWeaponChanged(FName)` broadcasts on equip/unequip (`NAME_None` = unequip); the visual component subscribes instead of polling.
- `SetWeaponVisible(bool)` is reserved for future combat draw/sheath; currently the weapon is always visible on the back.
- `AWeapon` / `WeaponClass` remains the future weapon-actor placeholder and is separate from the visual mounting path (both share the same `FWeaponConfigRow`).
- Battle terminology: the Boss opening sequence is a **cinematic (剧情动画)**; the player entry Montage plays after the battle starts (HUD already visible) and `UAnimNotify_Hold` moves the weapon to the hand.
- `UBattleComponent::PlayerEntrySectionName` (default `Draw`) is passed to `PlayAnimMontage` so the entry Montage starts from the designated section (e.g. `Draw_A_Great_Sword_1 → _2`); the Montage must have both segments inside one section or chained via Next Section.

### Battle System v1 (2026-08-05)
- `UBattleComponent` (player-mounted, BP_Dale) owns the battle session: phase state machine, simultaneous-turn resolution matrix, same-color clash timers, extra turns, victory/defeat and failure restart. Damage formulas live only in `UCombatFormulaSubsystem`.
- `UEnemyCombatAIComponent` (BP_Satan) picks enemy actions; v1 is uniform random subject to rules (BlueAttack requires >=1 charge stack, Charge disabled at max stacks, extra turn only BlueAttack/Charge, RedDefense disabled while the player has 0 charge stacks — `ChooseAction` takes `PlayerChargeStacks`).
- Battle rules (v1.1, 2026-08-07): first strike = the striking side starts with 1 charge stack (`bEnemyFirstStrike`, default enemy for Boss fights such as Satan), so battle never opens at 0:0. BlueAttack requires >=1 charge stack and clears stacks on use; RedDefense/WhiteAttack clear the actor's own charge stacks (no charge bonus); charge caps at `MaxChargeStacks` (2) and cannot be selected at cap. Any normal damage dealt to the opponent grants the attacker +1 stack (capped at 2), EXCEPT WhiteAttack vs charging — the white attacker gets no reward in this matchup; extra-turn BlueAttack follows the same +1 rule. The +1 always applies AFTER the move's own clear, so the attacker ends at exactly 1 stack. Same-color clash pending damage is NOT normal damage: no general +1 at resolution, stack rewards come only from `ResolveClash` (block/dodge success → player 1, failure → enemy 1), avoiding double stacking. WhiteAttack vs charging: WhiteAttack never interrupts the charge pose — the charger always keeps the charge animation and never plays the hurt reaction (`bEnemyChargeResisted`/`bPlayerChargeResisted`); a 0-stack charge resists (x0.3 white damage + 1 stack) and gains an extra turn, while a 1-stack charge takes FULL white damage, still gains +1 stack (to 2), and gets no extra turn. Extra turns allow only BlueAttack or Charge.
- Prologue teaching enemy (2026-08-09, GDD v0.26): the cave (序章) contains ONE scripted full teaching fight against an apprentice mage (`apprentice_cave` row: MaxHP 180, BaseDamageScale 0.4, no drops, SpawnAreas=hole, flee-only; `inept` removed from hole). The prologue opens with Dale ambushing the mage = PLAYER first strike, identical to enemy first strike (striker starts with 1 stack, opponent 0; no other effects — the old `FirstStrikeDamageScale`/`FirstStrikeDisableChargeTurns` params were removed from `FCombatParamsRow`). Enemy C++ classes follow the ASatan pattern: `AApprentice` (EnemyID=apprentice, BP_Apprentice) and `AApprenticeCave` (EnemyID=apprentice_cave, BP_Apprentice_Cave). C++ implemented: `UBattleComponent::StartBattle` auto-sets `bEnemyFirstStrike=false` for tutorial; `UEnemyCombatAIComponent` is plain RANDOM AI again (fixed script/route system removed); tutorial guidance is a director in `UBattleComponent` (`UpdateTutorialDirector`): ONE opening rules hint after entering battle, then two CONDITIONAL teaching moments — (A) when the player has 1 stack (round > 1, not yet taught) the enemy is forced to RedDefense and the player is locked to Charge, hinting the charge-to-2 auto-enhanced blue that breaks RedDefense; (B) when the player has 0 stacks (not yet taught) the enemy is forced to WhiteAttack and the player is locked to Charge, hinting the 0→1 charge resist (0.3x damage + extra turn). The FIRST time the player uses WhiteAttack (free round), the enemy is force-overridden to WhiteAttack too so a white-vs-white clash is guaranteed (`bTutorialFirstWhiteAttackUsed`, once per battle, reset on retry). Otherwise no button locks and no per-turn hints; player extra turns show a "only Blue/Charge" hint; a same-color clash shows the block/dodge prompt. NOTE WhiteAttack/RedDefense clear the attacker's OWN stacks each use (including red-vs-red skip). Flee (`FinishBattle(true, true)`) triggers once BOTH teaching moments are done AND HP <= `RunAwayHPThreshold`, with a 15-round cap as soft-lock safety; `ApplyDamageTo` clamps the mage at 1 HP. Clash block window applies world time dilation `ClashTimeDilation` TUTORIAL-ONLY (0.05 default, 0=off, gated by `IsTutorialBattle()`, restored on block/dodge input or clash end/cleanup); the slow-mo starts at `ClashHitTime - min(blockWindow, dodgeWindow)` so both windows are already open when it appears and an immediate click succeeds through the ORIGINAL elapsed-window checks (input logic unchanged). `WBP_CombatHUD` optional `TutorialHintText` shows the current hint. Full guidance design and data: `Plans/tutorial-enemy.md`; editor-side assets (DT row, BP_Apprentice/BP_Apprentice_Cave/ABP, montages, WBP binding, Level Sequence) remain manual.
- Battle start / multiple bosses (2026-08-09): `FOnBossIntroFinished` now carries the owning enemy (`AActor* EnemyActor`); `UBattleComponent` binds `OnIntroFinished` for ALL Tag-Boss enemies in the level (`FindBossEnemies()`), and `HandleIntroFinished` starts the battle against the enemy whose intro actually finished (resets `bBossDefeated` so several bosses/tutorial mages in one test map can each trigger a battle). `FindBossEnemy()` keeps first-found as fallback. Editor note: any BP binding to `OnIntroFinished` must now pass the `EnemyActor` pin (none currently wired).
- Same-color clash rules (2026-08-06): Blue vs Blue and White vs White deal no damage to either side — they only enter the real-time defend phase (the enemy attack damage is pending and resolved by Block/Dodge); Red vs Red skips the turn.
- HUD: `UCombatHUDWidget` + `WBP_CombatHUD` (state bars, action buttons with descriptions and hover scale `HoverScale`); the result banner is a separate `UBattleResultHUDWidget` + `WBP_BattleResult` (default `/Game/UI/HUD/WBP_BattleResult`, viewport layer 20). `WBP_BattleResult` asset creation is deferred until the basic combat loop is finalized; until then `FinishBattle` logs a warning and shows no banner. Same-color clash text prompts were removed; gameplay timers remain and dedicated collision animations are the planned prompt.
- Enemy action hint (2026-08-07): only the enemy's moves are shown — `EnemyActionHintText` (`BindWidgetOptional`) displays `敌方出招：{action}` after the player selects a valid action on a normal turn, and also when the enemy's extra turn begins (phase enters `Resolving`); it hides after `EnemyHintDuration` (default 3s) or on new round / battle end. The player's extra turn shows nothing (the enemy does not act); the turn that triggers an extra turn keeps the hint during its resolution, and the extra-turn select phase (`IsPlayerExtraTurn() && !HasPlayerChosenAction()`) clears it before the player's new selection. Player-extra-turn clicks must check `IsPlayerExtraTurn()` BEFORE calling `PlayerChooseAction` (it clears the flag), then hide.
- Stage/camera tuning lives in `DT_BattleStage` (12th table, `FCombatStageRow`, row `Default`): player offset (world or boss-local), facing yaw offsets, camera pitch/yaw/arm/FOV/SocketOffset/TargetOffset/lag. Battle start snapshots exploration transform/camera and restores on end; the player landing spot is ground-traced to avoid falling.
- Battle end: `SheathePlayerWeapon()` attaches the weapon back to `BackSocketName` directly (no sheath animation) and stops the entry Montage, so the player returns to the not-drawn Idle; runs on victory cleanup, defeat restart, and debug end.

### Combat animations (2026-08-05)
- `DT_CombatAnimConfig` (13th table; `FCombatAnimRow` + `FAnimRef`) — one row per combat entity; each action column is a Montage soft reference + `SectionName` + `PlayRate` + `BlendOutTime`; lookup via `UCombatFormulaSubsystem::GetCombatAnimRow(EntityID)`; playback orchestration lives in `UBattleComponent`.
- Fallback conventions (runtime, not USTRUCT): empty `DodgeFail`/`ChargeInterrupted` → play `Hurt`; there is only ONE block animation (`Block`; success and fail share it, no `BlockFail` column) — block success plays `Block` (parry) first, then `GoldCounter` via `PlayAnimThenReaction` + `OnActionMontageEnded`; charge resist (WhiteAttack vs charging) plays `Charge` pose instead of `Hurt` — the charge is never interrupted by WhiteAttack, and the resist pose is a gated looping pose (`PlayChargeResistPose`): it plays one full loop then is stopped, so the extra-turn animation starts only after the charge pose ends. If the charge pose is already playing at the hit frame, it continues without restarting (no visible replay).
- `OnMontageEnded` binding (2026-08-06): `PlayCombatAnim` is the single owner of the binding (RemoveDynamic → AddDynamic); `PlayAnimThenReaction` must NOT bind again (duplicate AddDynamic triggers an ensure). Chain playback ("play A, then B") always goes through `PlayAnimThenReaction` and the reaction is played by `PlayCombatAnim` after A ends.
- Full-charge auto blue attack (2026-08-06): when a Charge completes to 2 stacks against RedDefense (either side), `RegisterSideHit` treats it as an enhanced BlueAttack but plays the full Charge animation first, then chains the BlueAttack (`PlayAnimThenReaction(Charge, BlueAttack)`) — the blue attack must never interrupt the charge animation; damage still resolves at the BlueAttack hit frame/montage end.
- Red-defense counter sequence (2026-08-06, revised 2026-08-08): the defender's `RedDefense` starts early so its `GuardReady` marker frame aligns with the blue attack's `BlueAttackHit` notify. `RegisterBlueVsRedHit` uses BIDIRECTIONAL offset scheduling: red defense starts at `max(0, HitReal − GuardReal)` and the blue attack starts at `max(0, GuardReal − HitReal)` — when GuardReady is later than the hit frame, the red defense leads and the blue attack is delayed (positive-delay timer), so the two frames coincide in all data cases. `GetNotifyRealTime` converts notify times to REAL playback seconds (notify time − section start, then ÷ `PlayRate`); missing GuardReady falls back to `RedDefenseLeadTime`. `FTimerManager::SetTimer` ignores `Rate <= 0`, so a non-positive delay must play immediately instead of scheduling a timer; delays are clamped to the blue attack's real play length and non-finite values reset to 0 to avoid stalling the turn. After RedDefense ends the defender plays `GoldCounter`; damage and `Hurt` apply at the `GoldCounterHit` notify — or at `BlueAttackHit` when the charge-auto enhanced blue breaks the defense. A directly selected BlueAttack is ALWAYS countered by RedDefense, even at 2 stacks; only the auto-enhanced blue from `Charge` vs `RedDefense` (1→2 stacks) breaks the defense.
- Player clash-ready state: `UBaseCharacterAnimInstance::bClashReady` + `SetClashReady(bool)`, set by `UBattleComponent` in `StartClash` and cleared in `ResolveClash`/`SheathePlayerWeapon`; ABP switches Idle → ClashReady stance (same mirror pattern as `bWeaponDrawn`).
- Entry montage reads the table `Entry` first; `UBattleComponent::PlayerEntryMontage`/`PlayerEntrySectionName` remain BP fallbacks. `SheathePlayerWeapon` stops all montages and unbinds the block-success chain delegate.
- Damage is applied at animation hit frames (2026-08-06): attack montages carry `UAnimNotify_CombatDamage` (EventName: WhiteAttackHit/BlueAttackHit/GoldCounterHit/ClashAttackHit); `UBattleComponent` registers `FPendingHitEvent` per side at resolution and consumes it first-wins via notify or fallback (montage end / clash impact timer). Clash block/dodge windows are anchored to `ClashHitTime`, computed via `GetNotifyRealTime` from the clash attack notify (real seconds after subtracting the randomly selected section start and dividing by PlayRate). `GetNotifyRealTime` matches the notify INSIDE the selected section's time range — multiple same-name notifies are resolved per section, never by the montage's first match. If the selected section lacks the notify, `StartClash` falls back to that SECTION'S END time (real seconds) — never the global `ClashAttackTime` (0.8s); `ClashAttackTime` is used only when the section is unknown or the hit time is non-positive (a 0-delay timer would never fire). The turn gate blocks advancement only while the clash is UNRESOLVED (`Phase == Clash && !bClashResolved`) — the clash notify may consume the pending hit early (Amount=0) and montages may end, but the phase stays Clash until `OnClashImpact`/`ResolveClash` runs; once `ResolveClash` sets `bClashResolved = true`, the gate advances normally (never both before resolution nor stuck after it). Block/dodge input compares ELAPSED time since clash start (`Now − ClashStartTime`), never absolute world time; `ClashInputCooldown` blocks input spam. There is only ONE block animation (the `Block` column; success and fail share it, the old `BlockFail` column was removed): an early block press (outside the window) is a committed fail — it plays the block animation immediately via `PlayBlockAnimNow` and locks re-blocking for `BlockFailLockoutSeconds` (default 1s, `LastBlockFailTime`); at impact the player immediately blends into the `Hurt` animation. Block/dodge/red-counter success triggers `StartHitStop` (`HitStopDuration`); a blocked attacker immediately blends into `BlockedReaction`. Hit-stop (2026-08-06): triggers when the block/dodge takes effect (damage judgment), runs as a fixed countdown accumulated from frame `DeltaTime` in `TickComponent` (overlapping triggers keep the longer remaining time); on end only montages still active (`Montage_IsActive`) are resumed, and `EndHitStop()` cleans up on battle end/retry. Red-defense pre-play: `RedDefenseStartDelay = BlueAttackHitTime - GuardReadyTime` (GuardReady marker on the RedDefense montage; fallback `RedDefenseLeadTime`).
- Clash attack random sections (2026-08-06): `ClashAttackBlueSections`/`ClashAttackWhiteSections` are pipe-separated section names; when non-empty, `StartClash` picks one at random for the clash attack montage, otherwise it falls back to `ClashAttack*.SectionName`.
- Animation playback rights (2026-08-06): the resolution layer suppresses action animations for the countered/interrupted side (`IsActionSuppressed`: WhiteAttack vs BlueAttack, RedDefense vs WhiteAttack, Charge vs BlueAttack); hit-reaction priority is Death > BlockedReaction > ChargeInterrupted > Charge (resist) > Hurt; a 2-stack charge auto-blue plays as BlueAttack.
- Hit reactions always interrupt the target's current montage (`Montage_Stop` before playing the reaction in `ApplyPendingHitNow`); pending-hit `HitReaction` comes from the TARGET's anim row, never the attacker's.
- Interruption rule (2026-08-07): only explicitly documented animations may interrupt/mix with others; new sequences must wait for the full playback chain (e.g. the extra-turn animation waits for the charge-resist pose).
- Extra-turn playback (2026-08-07): `ResolveExtraTurn` marks `bPlayerOnlyAction`/`bEnemyOnlyAction` on `FTurnResolution`; `PlayResolutionAnimations` plays only the acting side, and `RegisterSideHit` treats the other side as `None` — the other side's stale `LastAction`/`EnemyChosenAction` from the previous turn must never play an animation or suppress the acting side.
- Turn gate (2026-08-06): every resolution opens a generic gate (`bTurnGateOpen` + `GatedMontages` + `TryAdvanceTurnIfGateDone`) — `EndTurnAndAdvance` is suspended until the full playback chain of that resolution (all non-looping action/reaction montages, `Montage->bLoop == false`) has ended, no pending hits remain, and no pending reactions remain. Looping poses (e.g. Charge) normally do not block the gate — exception: the charge-resist pose (白攻 vs 蓄力) is gated via `PlayChargeResistPose` (added to `GatedMontages`; a timer stops it after the current loop completes — `GetPlayLength()/PlayRate`, or the remaining loop time when the pose was already playing — then `OnActionMontageEnded` advances the gate), so the extra-turn animation never interrupts the charge pose. The pose is continued, not restarted, at the hit frame. `OnActionMontageEnded` only removes a montage from `GatedMontages` when it is no longer active on either combatant (`IsMontageActiveOnCombatants`) — when a stop+replay of the same asset does occur, the old instance's end callback must not remove the new instance's gate entry. Pending hits without a carrying montage settle immediately to avoid deadlock. This replaces the earlier blue-vs-red-only `bAwaitingDefenderChain`, so a fast next action cannot cancel pre-scheduled reactions (e.g. RedDefense → GoldCounter) in any resolution.

### Iteration rules
- New attribute: add a column to the DT USTRUCT → add a constant to the AttributeNames namespace → load into BaseAttributes during initialization (no Subsystem interface change, no entity header change)
- New buff/debuff: one `AddModifier(AttributeName, Op, Value, Turns)` call; no header changes
- New entity type (pet/summon, etc.): create a DT + FTableRowBase → mount `UAttributeComponent` during initialization

## 4. Development Principles: Modularity + Performance First

All new code must proactively satisfy:
1. **Modularity first:** components, interface separation, and single responsibility on the code side; on the editor side, blueprints mount components instead of inheriting, and DataTables drive data instead of hardcoding. Prefer `UActorComponent` or `UDataAsset` for new features; avoid base-class bloat
2. **Performance first:** use `ActorHasTag` (FName index comparison, O(1)) instead of `Cast<>` (UHT type hierarchy traversal, O(n)); use TMap instead of `TArray::FindByKey`; cache frequently accessed data; avoid repeated computation in Tick; reuse objects via pooling
3. **Bidirectional offset scheduling (双向错峰, 2026-08-08):** whenever two montages/timings must coincide at a shared frame (e.g. red defense `GuardReady` vs blue attack `BlueAttackHit`), schedule BOTH sides — A starts at `max(0, B−A)`, B starts at `max(0, A−B)` — never play one side fixed first and only delay the other. A non-positive delay must execute IMMEDIATELY (`FTimerManager::SetTimer` ignores `Rate <= 0` and invalidates the handle); delayed timers need cleanup on battle end/retry.

Before writing any new code ask: ① Can this be a component/DataAsset instead of changing a base class? ② Is there a faster alternative?

## 5. Build Environment

- UE 5.6 install path: `D:\Software\UnrealEngine\UE_5.6`
- Build command:
  ```
  D:\Software\UnrealEngine\UE_5.6\Engine\Build\BatchFiles\Build.bat HoleEditor Win64 Development "d:\UE5\UE_project\Kami\Hole\Hole.uproject"
  ```
- **Compile immediately after modifying any C++ file** to catch errors early
