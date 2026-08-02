# -*- coding: utf-8 -*-
"""
Export the existing DataTable assets under /Game/DataTable to CSV (stdout/log).
Used to inspect current row contents before regeneration.
"""

import unreal

TABLES = [
    "DT_CombatParams",
    "DT_CharacterConfig",
    "DT_EnemyConfig",
    "DT_WeaponConfig",
    "DT_MaskConfig",
    "DT_SmokeConfig",
    "DT_SkillConfig",
    "DT_SkillTreeConfig",
    "DT_AreaConfig",
    "DT_ConsumableConfig",
    "DT_EconomyConfig",
    "DT_BattleStage",
]


def main():
    for name in TABLES:
        path = "/Game/DataTable/{}".format(name)
        dt = unreal.load_asset(path)
        if dt is None:
            print("{} : MISSING".format(name))
            continue
        print("===== {} =====".format(name))
        try:
            print(dt.export_to_csv_string())
        except Exception as exc:
            print("export failed: {}".format(exc))


main()
