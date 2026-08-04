# Kami Project Memory (AGENTS.md)

> This file is Codex's persistent project memory for this repository, migrated from the original Claude Code project memory.
> Update rule: for changes to project conventions, architecture principles, or development process, update this file first, then sync to DevLog.

## 1. Project Overview

- **Engine:** Unreal Engine 5.6, project root `d:\UE5\UE_project\Kami`, UE project in the `Hole/` subdirectory
- **Game:** Hole — a roguelike semi-turn-based RPG with red/blue/white three-color counters + real-time parry/dodge + time loop
- **Key documents:**
  - `GDD_Outline.md` — game design document (v0.3, 19 chapters); the reference baseline for design and implementation
  - `DataTable_Spec.md` — data table specification (v0.1, 11 tables); the basis for C++ USTRUCT definitions
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
- `UInventoryComponent::OnWeaponChanged(FName)` broadcasts on equip/unequip (`NAME_None` = unequip); the visual component subscribes instead of polling.
- `SetWeaponVisible(bool)` is reserved for future combat draw/sheath; currently the weapon is always visible on the back.
- `AWeapon` / `WeaponClass` remains the future weapon-actor placeholder and is separate from the visual mounting path (both share the same `FWeaponConfigRow`).
- Battle terminology: the Boss opening sequence is a **cinematic (剧情动画)**; the player entry Montage plays after the battle starts (HUD already visible) and `UAnimNotify_Hold` moves the weapon to the hand.
- `UBattleComponent::PlayerEntrySectionName` (default `Draw`) is passed to `PlayAnimMontage` so the entry Montage starts from the designated section (e.g. `Draw_A_Great_Sword_1 → _2`); the Montage must have both segments inside one section or chained via Next Section.

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
