# Portal and minimap UI

- `/Game/UI/Navigation/WBP_PortalConfirm`: editable centered confirmation dialog; `UEOptionsConfirmWidget` handles its buttons and ESC. Both portal directions require confirmation. Cancelling does not move the player; leave the trigger and re-enter to prompt again.
- `/Game/UI/Navigation/WBP_Minimap`: editable circular minimap below the menu button in `WBP_OptionsHUD`. North is world +X; white arrow is the local pawn heading and cyan sector is camera heading. The map follows the pawn.
- `UEMinimapWidget`: local orthographic capture, 256 × 256, refreshed every 0.15 seconds. `ViewWidth`, `CaptureHeight`, and `UpdateInterval` are editable class defaults. Only the local player has a marker. The widget's `MarkerLayer` canvas is available for later replicated player markers; no additional server messages are sent now.
- Photo mode hides the parent gameplay HUD, including the minimap. Map captures stop when the widget is not ticking and the capture actor is destroyed with the widget.
- `BP_LoginPlayerController.PortalConfirmClass` selects the confirmation WBP. Runtime C++ does not construct the UI layout.

Authoring: run `create_portal_minimap.py` in the editor after building C++. Existing navigation widgets are preserved on subsequent runs. Edit their layout directly in UMG.

Local working copies also contain an unpublished offline portal exception. Do not include that exception or other local login fallback code in a commit. The new confirmation path still applies the existing server party and leader checks when confirming travel.

Validation: C++ build and authored widget compilation only; gameplay tests are performed by the project owner.
