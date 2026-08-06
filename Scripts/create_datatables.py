# -*- coding: utf-8 -*-
"""
Regenerate all DataTable assets per DataTable_Spec.md (currently 13 tables).

Usage (UE editor commandlet):
  UnrealEditor-Cmd.exe <Hole.uproject> -run=pythonscript -script=create_datatables.py -unattended -nop4 -nosplash -NoSound -nullrhi

Deterministic regeneration: existing tables are deleted and recreated every run.
Rows are written through DataTable.fill_from_csv_string (UE 5.6 Python API).
"""

import csv
import io
import unreal

DATA_TABLE_PATH = "/Game/DataTable"


def make_table(asset_name, struct_name):
    asset_path = "{}/{}".format(DATA_TABLE_PATH, asset_name)
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.EditorAssetLibrary.delete_asset(asset_path)
        print("DELETE {}".format(asset_path))

    factory = unreal.DataTableFactory()
    struct_obj = unreal.load_object(None, "/Script/Hole.{}".format(struct_name))
    if struct_obj is None:
        raise RuntimeError("struct not found: {}".format(struct_name))

    for prop in ("script_struct", "struct", "row_struct"):
        if hasattr(factory, prop):
            setattr(factory, prop, struct_obj)
            break
    else:
        raise RuntimeError("unknown struct property on DataTableFactory")

    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name, DATA_TABLE_PATH, unreal.DataTable, factory)
    if asset is None:
        raise RuntimeError("create_asset failed: {}".format(asset_name))

    print("CREATED {} ({})".format(asset_path, struct_name))
    return asset, True


def rows_to_csv(headers, rows):
    """rows: list of (row_name, {header: value})"""
    buf = io.StringIO()
    writer = csv.writer(buf, lineterminator="\n")
    writer.writerow(["---"] + headers)
    for row_name, values in rows:
        line = [row_name]
        for h in headers:
            v = values.get(h, "")
            if isinstance(v, bool):
                v = "True" if v else "False"
            line.append("" if v is None else str(v))
        writer.writerow(line)
    return buf.getvalue()


def fill_rows(dt, headers, rows):
    csv_text = rows_to_csv(headers, rows)
    ok = dt.fill_from_csv_string(csv_text)
    added = len(dt.get_row_names())
    print("  fill_from_csv_string={} rows_after={}/{}".format(ok, added, len(rows)))
    return added == len(rows)


def save(asset_name):
    asset_path = "{}/{}".format(DATA_TABLE_PATH, asset_name)
    saved = unreal.EditorAssetLibrary.save_asset(asset_path)
    print("SAVE {} -> {}".format(asset_path, saved))


# ---------------------------------------------------------------------------
# 01 — DT_CombatParams (already complete; regenerated for single-source consistency)
# ---------------------------------------------------------------------------
COMBATPARAMS_HEADERS = [
    "WhiteAttackDamageMin", "WhiteAttackDamageMax", "BlueAttackDamageMin_0Charge",
    "BlueAttackDamageMax_0Charge", "ChargeDamageMultiplier_1", "ChargeDamageMultiplier_2",
    "MaxChargeStacks", "WhiteInterruptChargeDamageScale", "BlockWindowSeconds",
    "DodgeWindowSeconds", "DodgeFailDamageScale", "FirstStrikeDisableChargeTurns",
    "ClashInputCooldown", "RedDefenseLeadTime", "HitStopDuration",
    "RunAwayHPThreshold", "PlayerDefaultHP", "Movement_BackwardSpeedScale",
    "CritDamageMultiplier", "DodgeBuffDamageScale", "DodgeBuffTurns",
    "FirstStrikeDamageScale", "GoldAttackDamageMin", "GoldAttackDamageMax",
]
combat_params = [
    ("Default", {
        "WhiteAttackDamageMin": 15.0, "WhiteAttackDamageMax": 25.0,
        "BlueAttackDamageMin_0Charge": 20.0, "BlueAttackDamageMax_0Charge": 30.0,
        "ChargeDamageMultiplier_1": 1.5, "ChargeDamageMultiplier_2": 2.25,
        "MaxChargeStacks": 2, "WhiteInterruptChargeDamageScale": 0.3,
        "BlockWindowSeconds": 0.25, "DodgeWindowSeconds": 0.35,
        "DodgeFailDamageScale": 1.2, "FirstStrikeDisableChargeTurns": 1,
        "ClashInputCooldown": 0.15, "RedDefenseLeadTime": 0.3, "HitStopDuration": 0.12,
        "RunAwayHPThreshold": 0.3, "PlayerDefaultHP": 100.0,
        "Movement_BackwardSpeedScale": 0.6,
        "CritDamageMultiplier": 1.5, "DodgeBuffDamageScale": 1.2, "DodgeBuffTurns": 1,
        "FirstStrikeDamageScale": 0.3, "GoldAttackDamageMin": 25.0, "GoldAttackDamageMax": 35.0,
    }),
]

# ---------------------------------------------------------------------------
# 02 — DT_CharacterConfig
# ---------------------------------------------------------------------------
CHARACTER_HEADERS = [
    "DisplayName", "PortraitTexture", "CharacterClass", "MaxHP", "MaxSmokeReserve",
    "BaseDamageScale", "BlueAttackBonus", "WhiteAttackBonus", "WalkSpeed", "SprintSpeed",
    "LandingLockTime", "DefaultWeaponID", "DefaultMaskID", "bIsPlayable",
    "bHasSmokeGland", "DailySmokeRecoveryMin", "DailySmokeRecoveryMax",
    "UnlockCondition_Round", "UnlockCondition_Desc",
]
# CharacterClass/EnemyClass 保留现有蓝图引用（BP_Dale / BP_Satan），其余按 DataTable_Spec §4.3/§5.4
BP_DALE_CLASS = "/Script/Engine.BlueprintGeneratedClass'/Game/Blueprint/Character/Roles/Dale/BP_Dale.BP_Dale_C'"
characters = [
    ("drifter", {
        "DisplayName": "漂泊者（主角）", "CharacterClass": BP_DALE_CLASS,
        "MaxHP": 120.0, "MaxSmokeReserve": 10.0,
        "BaseDamageScale": 1.05, "BlueAttackBonus": 0.0, "WhiteAttackBonus": 3.0,
        "WalkSpeed": 300.0, "SprintSpeed": 600.0, "LandingLockTime": 0.3,
        "DefaultWeaponID": "dale_sword",
        "bIsPlayable": True, "bHasSmokeGland": False,
        "DailySmokeRecoveryMin": 0.0, "DailySmokeRecoveryMax": 0.0,
        "UnlockCondition_Round": 0,
    }),
    ("ace", {
        "DisplayName": "艾斯", "MaxHP": 100.0, "MaxSmokeReserve": 10.0,
        "BaseDamageScale": 0.95, "BlueAttackBonus": 5.0, "WhiteAttackBonus": 0.0,
        "WalkSpeed": 300.0, "SprintSpeed": 600.0, "LandingLockTime": 0.3,
        "bIsPlayable": True, "bHasSmokeGland": True,
        "DailySmokeRecoveryMin": 1.0, "DailySmokeRecoveryMax": 2.0,
        "UnlockCondition_Round": 0,
    }),
    ("inept_char", {
        "DisplayName": "[待定] 无能力者", "MaxHP": 90.0, "MaxSmokeReserve": 10.0,
        "BaseDamageScale": 1.0, "BlueAttackBonus": 0.0, "WhiteAttackBonus": 0.0,
        "WalkSpeed": 300.0, "SprintSpeed": 600.0, "LandingLockTime": 0.3,
        "bIsPlayable": True, "bHasSmokeGland": False,
        "DailySmokeRecoveryMin": 0.0, "DailySmokeRecoveryMax": 0.0,
        "UnlockCondition_Round": 0,
    }),
    ("doctor", {
        "DisplayName": "博士", "MaxHP": 80.0, "MaxSmokeReserve": 10.0,
        "BaseDamageScale": 0.8, "BlueAttackBonus": 0.0, "WhiteAttackBonus": 0.0,
        "WalkSpeed": 300.0, "SprintSpeed": 600.0, "LandingLockTime": 0.3,
        "bIsPlayable": False, "bHasSmokeGland": True,
        "DailySmokeRecoveryMin": 0.0, "DailySmokeRecoveryMax": 0.0,
        "UnlockCondition_Round": 0,
    }),
    ("time_mage", {
        "DisplayName": "时间魔法师", "MaxHP": 100.0, "MaxSmokeReserve": 10.0,
        "BaseDamageScale": 1.3, "BlueAttackBonus": 0.0, "WhiteAttackBonus": 0.0,
        "WalkSpeed": 300.0, "SprintSpeed": 600.0, "LandingLockTime": 0.3,
        "bIsPlayable": False, "bHasSmokeGland": True,
        "DailySmokeRecoveryMin": 0.0, "DailySmokeRecoveryMax": 0.0,
        "UnlockCondition_Round": 0,
    }),
]

# ---------------------------------------------------------------------------
# 03 — DT_EnemyConfig
# ---------------------------------------------------------------------------
ENEMY_HEADERS = [
    "DisplayName", "Tier", "EnemyClass", "MaxHP", "BaseDamageScale", "AIPreference",
    "AIDifficulty", "WalkSpeed", "SprintSpeed", "LandingLockTime", "DropSmokeType",
    "DropSmokeCount", "DropCurrencyMin", "DropCurrencyMax", "SpawnAreas",
    "AlertRange", "ChaseRange",
]
BP_SATAN_CLASS = "/Script/Engine.BlueprintGeneratedClass'/Game/Blueprint/Character/Enemies/Satan/BP_Satan.BP_Satan_C'"
enemies = [
    ("inept", {
        "DisplayName": "无能力者", "Tier": "Tutorial", "MaxHP": 50.0,
        "BaseDamageScale": 1.0, "AIPreference": "PreferWhite", "AIDifficulty": 0.5,
        "WalkSpeed": 300.0, "SprintSpeed": 600.0, "LandingLockTime": 0.3,
        "DropSmokeType": "hunter_smoke", "DropSmokeCount": 1,
        "SpawnAreas": "hole,town_outskirts", "AlertRange": 1500.0, "ChaseRange": 900.0,
    }),
    ("apprentice", {
        "DisplayName": "低级魔法师", "Tier": "Normal", "MaxHP": 80.0,
        "BaseDamageScale": 1.0, "AIPreference": "Balanced", "AIDifficulty": 0.5,
        "WalkSpeed": 300.0, "SprintSpeed": 600.0, "LandingLockTime": 0.3,
        "DropSmokeType": "apprentice_smoke", "DropSmokeCount": 1,
        "SpawnAreas": "town,market", "AlertRange": 1500.0, "ChaseRange": 900.0,
    }),
    ("adept", {
        "DisplayName": "高级魔法师", "Tier": "Elite", "MaxHP": 150.0,
        "BaseDamageScale": 1.0, "AIPreference": "Adaptive", "AIDifficulty": 0.5,
        "WalkSpeed": 300.0, "SprintSpeed": 600.0, "LandingLockTime": 0.3,
        "DropSmokeType": "adept_smoke", "DropSmokeCount": 1,
        "SpawnAreas": "market,mansion", "AlertRange": 1500.0, "ChaseRange": 900.0,
    }),
    ("commander", {
        "DisplayName": "魔法师统领", "Tier": "Boss", "MaxHP": 500.0,
        "BaseDamageScale": 1.0, "AIPreference": "Adaptive", "AIDifficulty": 0.5,
        "WalkSpeed": 300.0, "SprintSpeed": 600.0, "LandingLockTime": 0.3,
        "DropSmokeType": "commander_smoke", "DropSmokeCount": 1,
        "SpawnAreas": "mansion", "AlertRange": 1500.0, "ChaseRange": 900.0,
    }),
    ("border_guard", {
        "DisplayName": "边境守卫", "Tier": "Elite", "MaxHP": 200.0,
        "BaseDamageScale": 1.0, "AIPreference": "PreferCharge", "AIDifficulty": 0.5,
        "WalkSpeed": 300.0, "SprintSpeed": 600.0, "LandingLockTime": 0.3,
        "DropSmokeType": "border_smoke", "DropSmokeCount": 1,
        "SpawnAreas": "border", "AlertRange": 1500.0, "ChaseRange": 900.0,
    }),
    ("demon", {
        "DisplayName": "恶魔", "Tier": "Elite", "MaxHP": 350.0,
        "BaseDamageScale": 1.0, "AIPreference": "Random", "AIDifficulty": 0.5,
        "WalkSpeed": 300.0, "SprintSpeed": 600.0, "LandingLockTime": 0.3,
        "DropSmokeType": "demon_smoke", "DropSmokeCount": 1,
        "SpawnAreas": "hell", "AlertRange": 1500.0, "ChaseRange": 900.0,
    }),
    ("satan", {
        "DisplayName": "撒旦", "Tier": "FinalBoss", "EnemyClass": BP_SATAN_CLASS,
        "MaxHP": 80.0, "BaseDamageScale": 1.0, "AIPreference": "Adaptive", "AIDifficulty": 0.5,
        "WalkSpeed": 300.0, "SprintSpeed": 600.0, "LandingLockTime": 0.3,
        "DropSmokeType": "satan_smoke", "DropSmokeCount": 1,
        "SpawnAreas": "hell", "AlertRange": 1500.0, "ChaseRange": 900.0,
    }),
    ("friendly_creature", {
        "DisplayName": "友善生物", "Tier": "Tutorial", "MaxHP": 20.0,
        "BaseDamageScale": 1.0, "AIPreference": "Balanced", "AIDifficulty": 0.5,
        "WalkSpeed": 300.0, "SprintSpeed": 600.0, "LandingLockTime": 0.3,
        "DropSmokeType": "healing_smoke", "DropSmokeCount": 1,
        "SpawnAreas": "all_areas_hidden", "AlertRange": 1500.0, "ChaseRange": 900.0,
    }),
]

# ---------------------------------------------------------------------------
# 04 — DT_WeaponConfig
# ---------------------------------------------------------------------------
WEAPON_HEADERS = [
    "DisplayName", "Description", "IconTexture", "MeshAsset", "Category", "WeaponClass",
    "BlueAttackDamageMod", "WhiteAttackDamageMod", "BlueAttackDamageScale",
    "WhiteAttackDamageScale", "BlockWindowBonus", "DodgeWindowBonus",
    "RedPenetrationScale", "ExtraChargeTurns", "Price",
]
weapons = [
    ("great_sword", {
        "DisplayName": "大剑", "Category": "GreatSword",
        "BlueAttackDamageScale": 1.3, "WhiteAttackDamageScale": 1.0,
        "ExtraChargeTurns": 1,
    }),
    ("hammer", {
        "DisplayName": "锤子", "Category": "Hammer",
        "BlueAttackDamageScale": 1.0, "WhiteAttackDamageScale": 1.0,
        "RedPenetrationScale": 0.5,
    }),
    ("sword", {
        "DisplayName": "单手剑", "Category": "Sword",
        "BlueAttackDamageScale": 1.0, "WhiteAttackDamageScale": 1.0,
        "BlockWindowBonus": 0.1,
    }),
    ("dale_sword", {
        "DisplayName": "漂泊者短剑",
        "Description": "在洞穴边缘拾获的旧短剑，刃口磨损却保养得当；战斗时才会拔出。",
        "Category": "Sword",
        "MeshAsset": "/Game/Fab/Weapon/Stylized_Dark_Sword/Assets/SwordB/Meshes/SM_Sword_B.SM_Sword_B",
        "BlueAttackDamageScale": 1.0, "WhiteAttackDamageScale": 1.0,
        "BlockWindowBonus": 0.1,
        "Price": 0,
    }),
    ("tbd_weapon_1", {
        "DisplayName": "[待定] 武器4", "Category": "TBD1",
    }),
]

# ---------------------------------------------------------------------------
# 05 — DT_MaskConfig
# ---------------------------------------------------------------------------
MASK_HEADERS = [
    "DisplayName", "Rarity", "Description", "SmokeGainScale", "ColorDamageScale_Red",
    "ColorDamageScale_Blue", "ColorDamageScale_White", "HPRegenOnKill",
    "SkillCostScale", "DropChance", "Price", "IconTexture", "MeshAsset",
]
masks = [
    ("mask_common", {
        "DisplayName": "普通面具", "Rarity": "Common",
        "SmokeGainScale": 1.1,
        "ColorDamageScale_Red": 1.0, "ColorDamageScale_Blue": 1.0, "ColorDamageScale_White": 1.0,
        "SkillCostScale": 1.0, "Price": 100,
    }),
    ("mask_rare_fire", {
        "DisplayName": "稀有面具（红攻）", "Rarity": "Rare",
        "SmokeGainScale": 1.0,
        "ColorDamageScale_Red": 1.15, "ColorDamageScale_Blue": 1.0, "ColorDamageScale_White": 1.0,
        "SkillCostScale": 1.0, "Price": 800,
    }),
    ("mask_legendary_lifesteal", {
        "DisplayName": "传说面具（击杀回血）", "Rarity": "Legendary",
        "SmokeGainScale": 1.0,
        "ColorDamageScale_Red": 1.0, "ColorDamageScale_Blue": 1.0, "ColorDamageScale_White": 1.0,
        "HPRegenOnKill": 0.05,
        "SkillCostScale": 1.0, "Price": 2000,
    }),
    ("mask_demonic_cost", {
        "DisplayName": "恶魔面具（技能减半）", "Rarity": "Demonic",
        "SmokeGainScale": 1.0,
        "ColorDamageScale_Red": 1.0, "ColorDamageScale_Blue": 1.0, "ColorDamageScale_White": 1.0,
        "SkillCostScale": 0.5, "Price": 5000,
    }),
    ("mask_tbd", {
        "DisplayName": "[待定] 面具", "Rarity": "Common",
        "SmokeGainScale": 1.0,
        "ColorDamageScale_Red": 1.0, "ColorDamageScale_Blue": 1.0, "ColorDamageScale_White": 1.0,
        "SkillCostScale": 1.0,
    }),
]

# ---------------------------------------------------------------------------
# 06 — DT_SmokeConfig
# ---------------------------------------------------------------------------
SMOKE_HEADERS = [
    "DisplayName", "Source", "PowerImprint", "bConvertToSkill", "ConvertedSkillID",
    "bConvertToItem", "ConvertedItemID", "bCanRefineToPassive",
    "bCanExchangeForCurrency", "CurrencyPerSmoke", "bCanDirectHealSmokeReserve",
    "DirectHealAmount",
]
smokes = [
    ("hunter_smoke", {"DisplayName": "猎手之烟", "Source": "Inept",
                      "bConvertToSkill": True, "bConvertToItem": True}),
    ("apprentice_smoke", {"DisplayName": "学徒之烟", "Source": "Apprentice",
                          "bConvertToSkill": True, "bConvertToItem": True}),
    ("adept_smoke", {"DisplayName": "术者之烟", "Source": "Adept",
                     "bConvertToSkill": True, "bConvertToItem": True,
                     "bCanRefineToPassive": True}),
    ("commander_smoke", {"DisplayName": "统领之烟", "Source": "Commander",
                         "bConvertToSkill": True, "bConvertToItem": True,
                         "bCanRefineToPassive": True}),
    ("border_smoke", {"DisplayName": "边境之烟", "Source": "BorderGuard",
                      "bConvertToItem": True, "bCanRefineToPassive": True}),
    ("demon_smoke", {"DisplayName": "恶魔之烟", "Source": "Demon",
                     "bConvertToSkill": True, "bConvertToItem": True,
                     "bCanRefineToPassive": True}),
    ("satan_smoke", {"DisplayName": "撒旦之烟", "Source": "Satan",
                     "bConvertToSkill": True, "bConvertToItem": True}),
    ("healing_smoke", {"DisplayName": "治愈之烟", "Source": "Friendly",
                       "bCanExchangeForCurrency": True, "CurrencyPerSmoke": 80,
                       "bCanDirectHealSmokeReserve": True, "DirectHealAmount": 3.0}),
    ("puzzle_smoke", {"DisplayName": "谜题之烟", "Source": "Puzzle",
                      "bConvertToSkill": True, "bConvertToItem": True}),
]

# ---------------------------------------------------------------------------
# 07 — DT_SkillConfig
# ---------------------------------------------------------------------------
SKILL_HEADERS = [
    "DisplayName", "Description", "IconTexture", "SkillClass", "Category", "TargetType",
    "SmokeCost", "CooldownTurns", "MaxUsesPerLoop", "BaseEffectValue",
    "EffectDurationTurns", "bIgnoresElementalColor", "bRetainedAcrossLoops",
    "ObtainFromSmokeType", "ObtainDescription",
]
skills = [
    ("heal_30", {"DisplayName": "治愈 30%", "Category": "Healing", "TargetType": "Self",
                 "SmokeCost": 2.5, "CooldownTurns": 3, "BaseEffectValue": 30.0,
                 "bIgnoresElementalColor": True, "bRetainedAcrossLoops": True,
                 "ObtainFromSmokeType": "healing_smoke"}),
    ("heal_clear_debuff", {"DisplayName": "净化", "Category": "Healing", "TargetType": "Self",
                           "SmokeCost": 3.0, "CooldownTurns": 4, "BaseEffectValue": 0.0,
                           "bIgnoresElementalColor": True, "bRetainedAcrossLoops": True,
                           "ObtainFromSmokeType": "healing_smoke"}),
    ("phys_dmg_up_50", {"DisplayName": "肉体强化·伤害+50%", "Category": "PhysEnhance", "TargetType": "Self",
                        "SmokeCost": 3.5, "CooldownTurns": 2, "BaseEffectValue": 50.0,
                        "EffectDurationTurns": 1,
                        "bIgnoresElementalColor": True, "bRetainedAcrossLoops": True,
                        "ObtainFromSmokeType": "border_smoke"}),
    ("phys_def_up_30", {"DisplayName": "肉体强化·减伤30%", "Category": "PhysEnhance", "TargetType": "Self",
                        "SmokeCost": 3.0, "CooldownTurns": 2, "BaseEffectValue": 30.0,
                        "EffectDurationTurns": 2,
                        "bIgnoresElementalColor": True, "bRetainedAcrossLoops": True,
                        "ObtainFromSmokeType": "border_smoke"}),
    ("atk_fixed_dmg", {"DisplayName": "固定攻击", "Category": "Attack", "TargetType": "SingleEnemy",
                       "SmokeCost": 3.0, "CooldownTurns": 1, "BaseEffectValue": 40.0,
                       "bIgnoresElementalColor": True, "bRetainedAcrossLoops": True,
                       "ObtainFromSmokeType": "adept_smoke"}),
    ("atk_aoe", {"DisplayName": "范围攻击", "Category": "Attack", "TargetType": "AllEnemies",
                 "SmokeCost": 5.0, "CooldownTurns": 3, "BaseEffectValue": 25.0,
                 "bIgnoresElementalColor": True, "bRetainedAcrossLoops": True,
                 "ObtainFromSmokeType": "adept_smoke"}),
    ("mental_read_next", {"DisplayName": "读心", "Category": "Mental", "TargetType": "SingleEnemy",
                          "SmokeCost": 2.0, "CooldownTurns": 2, "BaseEffectValue": 0.0,
                          "EffectDurationTurns": 1,
                          "bIgnoresElementalColor": True, "bRetainedAcrossLoops": True,
                          "ObtainFromSmokeType": "hunter_smoke"}),
    ("mental_lower_ai", {"DisplayName": "精神压制", "Category": "Mental", "TargetType": "SingleEnemy",
                         "SmokeCost": 3.0, "CooldownTurns": 3, "BaseEffectValue": 0.0,
                         "EffectDurationTurns": 1,
                         "bIgnoresElementalColor": True, "bRetainedAcrossLoops": True,
                         "ObtainFromSmokeType": "hunter_smoke"}),
    ("exclusive_barrier", {"DisplayName": "统领屏障", "Category": "Exclusive", "TargetType": "Self",
                           "SmokeCost": 6.5, "CooldownTurns": 5, "BaseEffectValue": 0.0,
                           "EffectDurationTurns": 3,
                           "bIgnoresElementalColor": True, "bRetainedAcrossLoops": True,
                           "ObtainFromSmokeType": "commander_smoke"}),
    ("time_rewind_turn", {"DisplayName": "时间回溯", "Category": "TimeMagic", "TargetType": "Self",
                          "SmokeCost": 10.0, "CooldownTurns": 0, "BaseEffectValue": 0.0,
                          "MaxUsesPerLoop": 1,
                          "bIgnoresElementalColor": True, "bRetainedAcrossLoops": True,
                          "ObtainFromSmokeType": "puzzle_smoke"}),
]

# ---------------------------------------------------------------------------
# 08 — DT_SkillTreeConfig
# ---------------------------------------------------------------------------
SKILLTREE_HEADERS = [
    "DisplayName", "Description", "Branch", "Tier", "ParentNodeID",
    "RequiredSmokeType", "RequiredSmokeCount", "CurrencyCost",
    "EffectType", "EffectValue",
]
skill_tree = [
    ("foundation_hp_1", {"DisplayName": "基础强化 HP+20", "Branch": "Foundation", "Tier": 0,
                         "ParentNodeID": "", "RequiredSmokeType": "adept_smoke",
                         "RequiredSmokeCount": 1, "CurrencyCost": 100,
                         "EffectType": "MaxHP", "EffectValue": 20.0}),
    ("foundation_hp_2", {"DisplayName": "基础强化 HP+40", "Branch": "Foundation", "Tier": 0,
                         "ParentNodeID": "foundation_hp_1", "RequiredSmokeType": "adept_smoke",
                         "RequiredSmokeCount": 1, "CurrencyCost": 100,
                         "EffectType": "MaxHP", "EffectValue": 40.0}),
    ("red_extend_window_1", {"DisplayName": "红色抵抗时间+0.1s", "Branch": "RedSpecialty", "Tier": 1,
                             "ParentNodeID": "foundation_hp_1", "RequiredSmokeType": "border_smoke",
                             "RequiredSmokeCount": 1, "CurrencyCost": 100,
                             "EffectType": "BlockWindow", "EffectValue": 0.1}),
    ("red_extend_window_2", {"DisplayName": "红色抵抗时间+0.2s", "Branch": "RedSpecialty", "Tier": 1,
                             "ParentNodeID": "red_extend_window_1", "RequiredSmokeType": "border_smoke",
                             "RequiredSmokeCount": 1, "CurrencyCost": 100,
                             "EffectType": "BlockWindow", "EffectValue": 0.2}),
    ("blue_charge_speed", {"DisplayName": "蓄力速度+1", "Branch": "BlueSpecialty", "Tier": 1,
                           "ParentNodeID": "foundation_hp_1", "RequiredSmokeType": "adept_smoke",
                           "RequiredSmokeCount": 1, "CurrencyCost": 100,
                           "EffectType": "ChargeSpeedBonus", "EffectValue": 1.0}),
    ("blue_crit_15", {"DisplayName": "蓝攻暴击15%", "Branch": "BlueSpecialty", "Tier": 1,
                      "ParentNodeID": "blue_charge_speed", "RequiredSmokeType": "adept_smoke",
                      "RequiredSmokeCount": 1, "CurrencyCost": 100,
                      "EffectType": "BlueCritChance", "EffectValue": 15.0}),
    ("white_dmg_5", {"DisplayName": "白攻伤害+5", "Branch": "WhiteSpecialty", "Tier": 1,
                     "ParentNodeID": "foundation_hp_1", "RequiredSmokeType": "hunter_smoke",
                     "RequiredSmokeCount": 1, "CurrencyCost": 100,
                     "EffectType": "WhiteDmgBonus", "EffectValue": 5.0}),
    ("white_interrupt_double", {"DisplayName": "打断伤害翻倍", "Branch": "WhiteSpecialty", "Tier": 1,
                                "ParentNodeID": "white_dmg_5", "RequiredSmokeType": "hunter_smoke",
                                "RequiredSmokeCount": 1, "CurrencyCost": 100,
                                "EffectType": "InterruptDmgScale", "EffectValue": 2.0}),
    ("defense_block_window", {"DisplayName": "格挡窗口+0.05s", "Branch": "DefenseSpecialty", "Tier": 2,
                              "ParentNodeID": "red_extend_window_1", "RequiredSmokeType": "border_smoke",
                              "RequiredSmokeCount": 1, "CurrencyCost": 100,
                              "EffectType": "BlockWindow", "EffectValue": 0.05}),
    ("defense_block_reduce", {"DisplayName": "格挡减伤+20%", "Branch": "DefenseSpecialty", "Tier": 2,
                              "ParentNodeID": "defense_block_window", "RequiredSmokeType": "border_smoke",
                              "RequiredSmokeCount": 1, "CurrencyCost": 100,
                              "EffectType": "BlockDmgReduce", "EffectValue": 20.0}),
    ("counter_dmg_up", {"DisplayName": "反击伤害+30%", "Branch": "CounterSpecialty", "Tier": 2,
                        "ParentNodeID": "blue_crit_15", "RequiredSmokeType": "adept_smoke",
                        "RequiredSmokeCount": 1, "CurrencyCost": 100,
                        "EffectType": "CounterDmgBonus", "EffectValue": 30.0}),
    ("counter_heal", {"DisplayName": "反击回血10%", "Branch": "CounterSpecialty", "Tier": 2,
                      "ParentNodeID": "counter_dmg_up", "RequiredSmokeType": "adept_smoke",
                      "RequiredSmokeCount": 1, "CurrencyCost": 100,
                      "EffectType": "CounterHealPercent", "EffectValue": 10.0}),
    ("dodge_window_up", {"DisplayName": "闪避窗口+0.05s", "Branch": "DodgeSpecialty", "Tier": 2,
                         "ParentNodeID": "white_dmg_5", "RequiredSmokeType": "hunter_smoke",
                         "RequiredSmokeCount": 1, "CurrencyCost": 100,
                         "EffectType": "DodgeWindow", "EffectValue": 0.05}),
    ("dodge_buff_up", {"DisplayName": "闪避Buff+10%", "Branch": "DodgeSpecialty", "Tier": 2,
                       "ParentNodeID": "dodge_window_up", "RequiredSmokeType": "hunter_smoke",
                       "RequiredSmokeCount": 1, "CurrencyCost": 100,
                       "EffectType": "DodgeBuffBonus", "EffectValue": 10.0}),
    ("legendary_node", {"DisplayName": "[待定] 最深传说节点", "Branch": "Foundation", "Tier": 3,
                        "ParentNodeID": "defense_block_reduce", "RequiredSmokeType": "demon_smoke",
                        "RequiredSmokeCount": 1, "CurrencyCost": 100,
                        "EffectType": "", "EffectValue": 0.0}),
]

# ---------------------------------------------------------------------------
# 09 — DT_AreaConfig
# ---------------------------------------------------------------------------
AREA_HEADERS = [
    "DisplayName", "VisualTheme", "SpecialMechanics", "UnlockRound", "DifficultyStars",
    "bIsSafeZone", "bIsLinear", "bFirstLoopOnly", "EnemyLevelScale", "PrimaryColor", "LevelAsset",
]
areas = [
    ("hole", {"DisplayName": "洞穴", "UnlockRound": 0, "DifficultyStars": 1,
              "bIsSafeZone": False, "bIsLinear": False, "bFirstLoopOnly": True,
              "EnemyLevelScale": 1.0}),
    ("town", {"DisplayName": "小镇", "UnlockRound": 1, "DifficultyStars": 1,
              "bIsSafeZone": True, "bIsLinear": False, "bFirstLoopOnly": False,
              "EnemyLevelScale": 1.0}),
    ("market", {"DisplayName": "市中心", "UnlockRound": 1, "DifficultyStars": 2,
                "bIsSafeZone": False, "bIsLinear": False, "bFirstLoopOnly": False,
                "EnemyLevelScale": 1.0}),
    ("mansion", {"DisplayName": "统领者宅院", "UnlockRound": 1, "DifficultyStars": 3,
                 "bIsSafeZone": False, "bIsLinear": True, "bFirstLoopOnly": False,
                 "EnemyLevelScale": 1.0}),
    ("border", {"DisplayName": "边境", "UnlockRound": 3, "DifficultyStars": 4,
                "bIsSafeZone": False, "bIsLinear": False, "bFirstLoopOnly": False,
                "EnemyLevelScale": 1.0}),
    ("hell", {"DisplayName": "地狱", "UnlockRound": 3, "DifficultyStars": 5,
              "bIsSafeZone": False, "bIsLinear": True, "bFirstLoopOnly": False,
              "EnemyLevelScale": 1.0}),
]

# ---------------------------------------------------------------------------
# 10 — DT_ConsumableConfig
# ---------------------------------------------------------------------------
CONSUMABLE_HEADERS = [
    "DisplayName", "Description", "IconTexture", "ConsumableClass", "Type",
    "EffectValue", "EffectDurationTurns", "MaxCarryCount", "bUsableInCombat",
    "bRetainedAcrossLoops", "Price", "ObtainFromSmokeType",
]
consumables = [
    ("hp_potion_small", {"DisplayName": "小型HP药水", "Type": "HPRecovery",
                         "EffectValue": 25.0, "MaxCarryCount": 5, "Price": 30,
                         "bUsableInCombat": True, "bRetainedAcrossLoops": False}),
    ("hp_potion_large", {"DisplayName": "大型HP药水", "Type": "HPRecovery",
                         "EffectValue": 60.0, "MaxCarryCount": 5, "Price": 80,
                         "bUsableInCombat": True, "bRetainedAcrossLoops": False}),
    ("smoke_vial_small", {"DisplayName": "小型烟瓶", "Type": "SmokeRecovery",
                          "EffectValue": 2.0, "MaxCarryCount": 5, "Price": 50,
                          "bUsableInCombat": True, "bRetainedAcrossLoops": False}),
    ("atk_buff_scroll", {"DisplayName": "攻击增益卷轴", "Type": "TempBuff",
                         "EffectValue": 20.0, "EffectDurationTurns": 3, "MaxCarryCount": 5, "Price": 40,
                         "bUsableInCombat": True, "bRetainedAcrossLoops": False}),
    ("def_buff_scroll", {"DisplayName": "防御增益卷轴", "Type": "TempBuff",
                         "EffectValue": 20.0, "EffectDurationTurns": 3, "MaxCarryCount": 5, "Price": 40,
                         "bUsableInCombat": True, "bRetainedAcrossLoops": False}),
    ("white_dmg_booster", {"DisplayName": "白攻强化剂", "Type": "StatBoost",
                           "EffectValue": 5.0, "MaxCarryCount": 5, "bUsableInCombat": False,
                           "bRetainedAcrossLoops": True, "ObtainFromSmokeType": "hunter_smoke"}),
    ("blue_dmg_booster", {"DisplayName": "蓝攻强化剂", "Type": "StatBoost",
                          "EffectValue": 5.0, "MaxCarryCount": 5, "bUsableInCombat": False,
                          "bRetainedAcrossLoops": True, "ObtainFromSmokeType": "apprentice_smoke"}),
    ("charge_speed_booster", {"DisplayName": "蓄力速度强化剂", "Type": "StatBoost",
                              "EffectValue": 1.0, "MaxCarryCount": 5, "bUsableInCombat": False,
                              "bRetainedAcrossLoops": True, "ObtainFromSmokeType": "adept_smoke"}),
    ("block_window_booster", {"DisplayName": "格挡窗口强化剂", "Type": "StatBoost",
                              "EffectValue": 0.03, "MaxCarryCount": 5, "bUsableInCombat": False,
                              "bRetainedAcrossLoops": True, "ObtainFromSmokeType": "border_smoke"}),
    ("healing_smoke_direct", {"DisplayName": "治愈之烟（直接回复）", "Type": "SmokeRecovery",
                              "EffectValue": 3.0, "MaxCarryCount": 5, "bUsableInCombat": True,
                              "bRetainedAcrossLoops": False, "ObtainFromSmokeType": "healing_smoke"}),
]

# ---------------------------------------------------------------------------
# 11 — DT_EconomyConfig
# ---------------------------------------------------------------------------
ECONOMY_HEADERS = ["DisplayName", "Category", "Value", "Description"]
economy = [
    ("healing_smoke_to_currency", {"DisplayName": "治愈之烟兑换货币", "Category": "Exchange",
                                   "Value": 80.0, "Description": "1个治愈之烟兑换货币量"}),
    ("refine_cost_base", {"DisplayName": "博士提炼基础费用", "Category": "Refine",
                          "Value": 100.0, "Description": "博士提炼基础费用/次"}),
    ("refine_cost_advanced", {"DisplayName": "博士提炼高级费用", "Category": "Refine",
                              "Value": 200.0, "Description": "博士提炼高级费用/次"}),
    ("price_common_equipment", {"DisplayName": "基础装备价格基数", "Category": "Price",
                                "Value": 200.0, "Description": "基础装备价格基数"}),
    ("price_rare_mask", {"DisplayName": "稀有面具价格基数", "Category": "Price",
                         "Value": 1000.0, "Description": "稀有面具价格基数"}),
    ("price_consumable", {"DisplayName": "消耗品价格基数", "Category": "Price",
                          "Value": 35.0, "Description": "消耗品价格基数"}),
    ("currency_start", {"DisplayName": "初始货币量", "Category": "Drop",
                        "Value": 50.0, "Description": "[待定] 初始货币量"}),
    ("healing_smoke_per_loop", {"DisplayName": "每轮回治愈之烟产出", "Category": "Drop",
                                "Value": 4.5, "Description": "[待定] 每轮回平均治愈之烟产出（3-6）"}),
]


# ---------------------------------------------------------------------------
# 13 - DT_CombatAnimConfig
# ---------------------------------------------------------------------------
ANIM_COLUMNS = [
    "Entry", "Sheathe", "RedDefense", "GoldCounter", "BlueAttack", "WhiteAttack",
    "Charge", "ChargeInterrupted", "BlockedReaction", "Hurt", "BlockSuccess", "BlockFail",
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

# ---------------------------------------------------------------------------
# 12 - DT_BattleStage
# ---------------------------------------------------------------------------
BATTLESTAGE_HEADERS = [
    "PlayerBattleOffset", "bPlayerOffsetInBossLocalSpace", "bBossFacePlayer",
    "BossFacingYawOffset", "bPlayerFaceBoss", "PlayerFacingYawOffset",
    "CameraPitch", "CameraYawOffset", "CameraArmLength", "CameraFOV",
    "SpringSocketOffset", "SpringTargetOffset", "bSpringEnableCameraLag",
    "SpringCameraLagSpeed",
]
battle_stage = [
    ("Default", {
        "PlayerBattleOffset": "(X=0.000000,Y=-550.000000,Z=0.000000)",
        "bPlayerOffsetInBossLocalSpace": False,
        "bBossFacePlayer": True, "BossFacingYawOffset": 0.0,
        "bPlayerFaceBoss": True, "PlayerFacingYawOffset": 0.0,
        "CameraPitch": -12.0, "CameraYawOffset": 0.0,
        "CameraArmLength": 400.0, "CameraFOV": 90.0,
        "SpringSocketOffset": "(X=0.000000,Y=0.000000,Z=0.000000)",
        "SpringTargetOffset": "(X=0.000000,Y=0.000000,Z=0.000000)",
        "bSpringEnableCameraLag": False, "SpringCameraLagSpeed": 10.0,
    }),
]


def main():
    jobs = [
        ("DT_CombatParams", "CombatParamsRow", COMBATPARAMS_HEADERS, combat_params),
        ("DT_CharacterConfig", "CharacterConfigRow", CHARACTER_HEADERS, characters),
        ("DT_EnemyConfig", "EnemyConfigRow", ENEMY_HEADERS, enemies),
        ("DT_WeaponConfig", "WeaponConfigRow", WEAPON_HEADERS, weapons),
        ("DT_MaskConfig", "MaskConfigRow", MASK_HEADERS, masks),
        ("DT_SmokeConfig", "SmokeConfigRow", SMOKE_HEADERS, smokes),
        ("DT_SkillConfig", "SkillConfigRow", SKILL_HEADERS, skills),
        ("DT_SkillTreeConfig", "SkillTreeConfigRow", SKILLTREE_HEADERS, skill_tree),
        ("DT_AreaConfig", "AreaConfigRow", AREA_HEADERS, areas),
        ("DT_ConsumableConfig", "ConsumableConfigRow", CONSUMABLE_HEADERS, consumables),
        ("DT_EconomyConfig", "EconomyConfigRow", ECONOMY_HEADERS, economy),
        ("DT_BattleStage", "CombatStageRow", BATTLESTAGE_HEADERS, battle_stage),
        ("DT_CombatAnimConfig", "CombatAnimRow", COMBATANIM_HEADERS, combat_anims),
    ]
    for asset_name, struct_short, headers, rows in jobs:
        try:
            dt, _ = make_table(asset_name, struct_short)
            fill_rows(dt, headers, rows)
            save(asset_name)
        except Exception as exc:
            print("TABLE FAIL {} : {}".format(asset_name, exc))


main()
