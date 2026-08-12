# Palworld Character Customization

This folder contains the runtime code for the Palworld-style character customization level.

## Runtime Pieces

- `Data/UEPalworldCustomizationTypes.*`
  Defines the Blueprint-visible data model: body type, face, hair, eyes, outfit, head gear, color channels, and the catalog data asset.

- `Preview/UEPalworldCustomizationPreviewActor.*`
  Owns the visible character preview. It loads the selected skeletal meshes from `DA_PalworldCustomizationCatalog`, keeps the outfit mesh as the animation leader, applies eye materials, and attaches head gear using the socket names from the extracted Palworld character creation tables.

- `UI/UEPalworldCustomizationWidget.*`
  Temporary runtime UI fallback used by `WBP_PalworldCustomization`. The current widget blueprint is intentionally still present, but its designer tree is empty, so this C++ class builds the working screen until the UI is migrated into UMG designer widgets.

- `Framework/UEPalworldCustomizationPlayerController.*`
  Finds the preview actor in the level, loads the catalog, creates the customization widget, and wires UI selections to the preview actor.

## Source Data

The catalog is built from:

- `Saved/Codex/PalworldImport/palworld_customization_manifest.json`
- `Saved/Codex/PalworldImport/palworld_unreal_import_manifest.json`
- `Saved/Codex/PalworldImport/palworld_fbx_asset_map.json`

Those files are generated from the extracted Palworld character creation tables. Do not build option lists by scanning random filenames; use the manifest order and IDs.

## Important Rules

- Original materials are the default. Runtime RGB tint is only an override API and should not replace source textures.
- The current extract does not include a separate naked base body skeletal mesh. Outfit/body equipment is the leader mesh for animation.
- Do not scale head, hair, and outfit separately. Without a real Palworld morph/retarget bridge, separate scaling breaks necks, sockets, and accessory placement.
- Head gear should use Palworld table sockets first. Bounds-based fallback exists only for assets without usable sockets.
- Model thumbnails are generated for visible selection buttons so the UI shows actual shapes, not raw texture swatches.

## Validation Scripts

Useful scripts live under `Saved/Codex/PalworldImport/Scripts`:

- `validate_palworld_catalog_asset.py`
- `validate_palworld_selection_flow.py`
- `validate_palworld_runtime_preview.py`
- `validate_palworld_fbx_catalog_sections.py`
- `generate_palworld_model_thumbnails.py`
- `import_palworld_model_thumbnails_to_catalog.py`

Reports are written to `Saved/Codex/PalworldImport/Reports`.
