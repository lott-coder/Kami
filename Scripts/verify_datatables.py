# -*- coding: utf-8 -*-
"""
Verify the 11 DataTable assets under /Game/DataTable:
prints row count per table and spot-checks key values.
"""

import unreal

TABLES = [
    "DT_CombatParams", "DT_CharacterConfig", "DT_EnemyConfig",
    "DT_WeaponConfig", "DT_MaskConfig", "DT_SmokeConfig",
    "DT_SkillConfig", "DT_SkillTreeConfig", "DT_AreaConfig",
    "DT_ConsumableConfig", "DT_EconomyConfig",
    "DT_BattleStage",
    "DT_CombatAnimConfig",
    "DT_SettlementConfig",
    "DT_TutorialConfig",
    "DT_AreaBGMConfig",
    "DT_EnemyBGMConfig",
]


def main():
    for name in TABLES:
        path = "/Game/DataTable/{}".format(name)
        dt = unreal.load_asset(path)
        if dt is None:
            print("MISSING {}".format(path))
            continue
        rows = dt.get_row_names()
        print("{} : {} rows".format(name, len(rows)))

    # Spot check: export DT_MaskConfig back to CSV to confirm stored values
    mask = unreal.load_asset("/Game/DataTable/DT_MaskConfig")
    if mask is not None:
        csv_text = mask.export_to_csv_string()
        for line in csv_text.splitlines()[:6]:
            print("MASK_CSV: " + line)

    skill = unreal.load_asset("/Game/DataTable/DT_SkillConfig")
    if skill is not None:
        for line in skill.export_to_csv_string().splitlines()[:3]:
            print("SKILL_CSV: " + line)

    consumable = unreal.load_asset("/Game/DataTable/DT_ConsumableConfig")
    if consumable is not None:
        for line in consumable.export_to_csv_string().splitlines()[:3]:
            print("CONSUMABLE_CSV: " + line)

    character = unreal.load_asset("/Game/DataTable/DT_CharacterConfig")
    if character is not None:
        for line in character.export_to_csv_string().splitlines()[:6]:
            print("CHARACTER_CSV: " + line)

    enemy = unreal.load_asset("/Game/DataTable/DT_EnemyConfig")
    if enemy is not None:
        for line in enemy.export_to_csv_string().splitlines()[:9]:
            print("ENEMY_CSV: " + line)

    stage = unreal.load_asset("/Game/DataTable/DT_BattleStage")
    if stage is not None:
        for line in stage.export_to_csv_string().splitlines()[:3]:
            print("STAGE_CSV: " + line)

    anim = unreal.load_asset("/Game/DataTable/DT_CombatAnimConfig")
    if anim is not None:
        for line in anim.export_to_csv_string().splitlines()[:3]:
            print("ANIM_CSV: " + line)


main()
