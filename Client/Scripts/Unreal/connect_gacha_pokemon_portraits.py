"""Import missing gacha portraits and connect the existing Bloodmoon Ursaluna.

Run inside the Unreal editor. Uses the shared species ProfileIcon reference;
does not change probabilities, rarity, meshes, animations, or the species catalog.
"""

import json
import sys
from pathlib import Path

import unreal

SCRIPT_ROOT = Path(__file__).resolve().parent
if str(SCRIPT_ROOT) not in sys.path:
    sys.path.insert(0, str(SCRIPT_ROOT))
from import_round_pokemon_portraits import import_species_portraits


def main():
    library = unreal.EditorAssetLibrary
    bear = library.load_asset("/Game/Pokemon/SpeciesData/붉은달다투곰/DA_붉은달다투곰")
    if bear is None:
        raise RuntimeError("Existing Bloodmoon Ursaluna species data is missing")
    species_by_name = {}
    for type_name in ("Fire", "Water", "Grass", "Normal", "Electric"):
        pool = library.load_asset(f"/Game/Gacha/Data/DA_Gacha_{type_name}")
        if pool is None:
            raise RuntimeError(f"Missing gacha pool: {type_name}")
        entries = list(pool.get_editor_property("entries"))
        changed = False
        for entry in entries:
            if type_name == "Normal" and entry.get_editor_property("dex_number") == 901:
                entry.set_editor_property("species", bear)
                entry.set_editor_property("display_name", unreal.Text("붉은달다투곰"))
                changed = True
            species = entry.get_editor_property("species")
            if species is None:
                raise RuntimeError(f"Missing species: {entry.get_editor_property('display_name')}")
            name = species.get_name().removeprefix("DA_")
            species_by_name[name] = species
        if changed:
            pool.set_editor_property("entries", entries)
            library.save_loaded_asset(pool)

    # 팀원이 이미 지정한 초상화는 덮어쓰지 않고 그대로 보존한다.
    missing = [name for name, species in species_by_name.items()
               if species.get_editor_property("profile_icon") is None]
    import_species_portraits(missing)

    report = {name: species.get_editor_property("profile_icon").get_path_name()
              for name, species in species_by_name.items()}
    report_path = Path(unreal.Paths.project_saved_dir()) / "Codex/Gacha/connected_portraits.json"
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log(f"[GACHA PORTRAITS] Saved {len(missing)} new portraits; {len(report)} species connected")


if __name__ == "__main__":
    main()
