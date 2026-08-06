# Kami Project Memory (AGENTS.md)

> This file is Codex's persistent project memory for this repository, migrated from the original Claude Code project memory.
> Update rule: for changes to project conventions, architecture principles, or development process, update this file first, then sync to DevLog.

## 1. Project Overview

- **Engine:** Unreal Engine 5.6, project root `d:\UE5\UE_project\Kami`, UE project in the `Hole/` subdirectory
- **Game:** Hole — a roguelike semi-turn-based RPG with red/blue/white three-color counters + real-time parry/dodge + time loop
- **Key documents:**
  - `GDD_Outline.md` — game design document (v0.12, 19 chapters); the reference baseline for design and implementation
  - `DataTable_Spec.md` — data table specification (v0.12, 13 tables); the basis for C++ USTRUCT definitions
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
- **Confirm the path before creating new files:** always confirm the target path with the user before creating any new file; editing existing files is exempt
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
- `UEnemyCombatAIComponent` (BP_Satan) picks enemy actions; v1 is uniform random subject to rules (BlueAttack requires >=1 charge stack, Charge disabled at max stacks, extra turn only BlueAttack/Charge).
- Battle rules: normal neutral start (first-strike interface reserved); BlueAttack requires >=1 charge stack and clears stacks on use; RedDefense/WhiteAttack clear the actor's own charge stacks (no charge bonus); charge caps at `MaxChargeStacks` (2) and cannot be selected at cap; interrupting charge (BlueAttack vs Charge, either side) clears the charger's stacks; extra turn is triggered by WhiteAttack vs charging (resistance, x0.3 + extra turn) and only allows BlueAttack or Charge.
- Same-color clash rules (2026-08-06): Blue vs Blue and White vs White deal no damage to either side — they only enter the real-time defend phase (the enemy attack damage is pending and resolved by Block/Dodge); Red vs Red skips the turn.
- HUD: `UCombatHUDWidget` + `WBP_CombatHUD` (state bars, action buttons with descriptions and hover scale `HoverScale`); the result banner is a separate `UBattleResultHUDWidget` + `WBP_BattleResult` (default `/Game/UI/HUD/WBP_BattleResult`, viewport layer 20). `WBP_BattleResult` asset creation is deferred until the basic combat loop is finalized; until then `FinishBattle` logs a warning and shows no banner. Same-color clash text prompts were removed; gameplay timers remain and dedicated collision animations are the planned prompt.
- Stage/camera tuning lives in `DT_BattleStage` (12th table, `FCombatStageRow`, row `Default`): player offset (world or boss-local), facing yaw offsets, camera pitch/yaw/arm/FOV/SocketOffset/TargetOffset/lag. Battle start snapshots exploration transform/camera and restores on end; the player landing spot is ground-traced to avoid falling.
- Battle end: `SheathePlayerWeapon()` attaches the weapon back to `BackSocketName` directly (no sheath animation) and stops the entry Montage, so the player returns to the not-drawn Idle; runs on victory cleanup, defeat restart, and debug end.

### Combat animations (2026-08-05)
- `DT_CombatAnimConfig` (13th table; `FCombatAnimRow` + `FAnimRef`) — one row per combat entity; each action column is a Montage soft reference + `SectionName` + `PlayRate` + `BlendOutTime`; lookup via `UCombatFormulaSubsystem::GetCombatAnimRow(EntityID)`; playback orchestration lives in `UBattleComponent`.
- Fallback conventions (runtime, not USTRUCT): empty `BlockFail`/`DodgeFail`/`ChargeInterrupted` → play `Hurt`; block success plays `BlockSuccess` (parry) first, then `GoldCounter` via `PlayAnimThenReaction` + `OnActionMontageEnded`; charge resist (WhiteAttack vs charging) plays `Charge` pose instead of `Hurt` — the charge is never interrupted by WhiteAttack.
- Red-defense counter sequence (2026-08-06, revised): the defender's `RedDefense` starts early so its `GuardReady` marker frame aligns with the blue attack's `BlueAttackHit` notify (start delay = BlueAttackHit time − GuardReady time; fallback `RedDefenseLeadTime`); after RedDefense ends the defender plays `GoldCounter`; damage and `Hurt` apply at the `GoldCounterHit` notify — or at `BlueAttackHit` when a 2-stack enhanced blue breaks the defense.
- Player clash-ready state: `UBaseCharacterAnimInstance::bClashReady` + `SetClashReady(bool)`, set by `UBattleComponent` in `StartClash` and cleared in `ResolveClash`/`SheathePlayerWeapon`; ABP switches Idle → ClashReady stance (same mirror pattern as `bWeaponDrawn`).
- Entry montage reads the table `Entry` first; `UBattleComponent::PlayerEntryMontage`/`PlayerEntrySectionName` remain BP fallbacks. `SheathePlayerWeapon` stops all montages and unbinds the block-success chain delegate.
- Damage is applied at animation hit frames (2026-08-06): attack montages carry `UAnimNotify_CombatDamage` (EventName: WhiteAttackHit/BlueAttackHit/GoldCounterHit/ClashAttackHit); `UBattleComponent` registers `FPendingHitEvent` per side at resolution and consumes it first-wins via notify or fallback (montage end / clash impact timer). Clash block/dodge windows are anchored to `ClashHitTime` (from the clash attack notify); `ClashInputCooldown` blocks input spam. Block/dodge/red-counter success triggers `StartHitStop` (`HitStopDuration`); a blocked attacker immediately blends into `BlockedReaction`. Red-defense pre-play: `RedDefenseStartDelay = BlueAttackHitTime - GuardReadyTime` (GuardReady marker on the RedDefense montage; fallback `RedDefenseLeadTime`).
- Clash attack random sections (2026-08-06): `ClashAttackBlueSections`/`ClashAttackWhiteSections` are pipe-separated section names; when non-empty, `StartClash` picks one at random for the clash attack montage, otherwise it falls back to `ClashAttack*.SectionName`.

### Iteration rules
- New attribute: add a column to the DT USTRUCT → add a constant to the AttributeNames namespace → load into BaseAttributes during initialization (no Subsystem interface change, no entity header change)
- New buff/debuff: one `AddModifier(AttributeName, Op, Value, Turns)` call; no header changes
- New entity type (pet/summon, etc.): create a DT + FTableRowBase → mount `UAttributeComponent` during initialization

## 4. Development Principles: Modularity + Performance First

All new code must proactively satisfy:
1. **Modularity first:** components, interface separation, and single responsibility on the code side; on the editor side, blueprints mount components instead of inheriting, and DataTables drive data instead of hardcoding. Prefer `UActorComponent` or `UDataAsset` for new features; avoid base-class bloat
2. **Performance first:** use `ActorHasTag` (FName index comparison, O(1)) instead of `Cast<>` (UHT type hierarchy traversal, O(n)); use TMap instead of `TArray::FindByKey`; cache frequently accessed data; avoid repeated computation in Tick; reuse objects via pooling

Before writing any new code ask: ① Can this be a component/DataAsset instead of changing a base class? ② Is there a faster alternative?

## 5. Build Environment

- UE 5.6 install path: `D:\Software\UnrealEngine\UE_5.6`
- Build command:
  ```
  D:\Software\UnrealEngine\UE_5.6\Engine\Build\BatchFiles\Build.bat HoleEditor Win64 Development "d:\UE5\UE_project\Kami\Hole\Hole.uproject"
  ```
- **Compile immediately after modifying any C++ file** to catch errors early
