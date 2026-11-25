# Menu and settings

**P** (or **Esc** in a packaged / standalone game; in Play-In-Editor Esc stops the session) or the gamepad **View/Back** button opens the menu, on foot or at the wheel. In a single-player game the world pauses while it is open; in multiplayer it keeps running.

| Row | Values | Stored in |
|---|---|---|
| Resume | closes the menu | |
| Graphics quality | Low, Medium, High, Epic, Cinematic ("Custom" if mixed) | GameUserSettings (scalability) |
| Display mode | Fullscreen, Borderless window, Windowed | GameUserSettings |
| Resolution | 1280x720 ... 3840x2160 (borderless always uses the desktop size) | GameUserSettings |
| VSync | On / Off | GameUserSettings |
| Frame rate limit | 30, 60, 90, 120, 144, 165, 240, Unlimited | GameUserSettings |
| Render scale | 50% ... 100% (lower = faster, softer image) | `[LostIsland.Settings]` |
| Mouse sensitivity | 0.1 ... 3.0 (also the right stick, on foot and in vehicles) | `[LostIsland.Settings]` |
| Invert mouse Y | On / Off | `[LostIsland.Settings]` |
| Field of view | 60 ... 120 (vehicles add their speed effect on top) | `[LostIsland.Settings]` |
| Show FPS | On / Off: frame rate and frame time, top right | `[LostIsland.Settings]` |
| Save game | same as F5 (only on the host) | |
| Quit to desktop | | |

Controls in the menu: **Up/Down** (or W/S, D-pad) choose a row, **Left/Right** (A/D, D-pad) change it, **Enter/Space** (gamepad A) selects, **Esc/P** (gamepad B) closes. The mouse works too: click a row, or the `<` `>` arrows.

Every change applies at once and is saved in `Saved/Config/<Platform>/GameUserSettings.ini`, so it is back the next time the game starts.

Code: `ULISettingsMenu` (rows, values, drawing on the canvas HUD), `FLIGameSettings` (the game's own settings), `ALIPlayerController::SetSettingsMenuOpen` (cursor, input, pause).

## Test checklist (PIE)

1. P opens the menu, the game pauses (standalone/PIE single player), the mouse cursor appears, the character does not move with W/S.
2. Up/Down and the mouse move the highlight; Left/Right change values.
3. Graphics quality Low -> Epic: the picture changes at once. Render scale 50%: blurrier, faster.
4. Mouse sensitivity 2.0 and Invert Y: looking around on foot and in the pickup follows it.
5. Field of view 110: wider view on foot and in vehicles.
6. Show FPS: counter top right, also while the menu is open.
7. Close with P or Resume: cursor gone, game runs. Restart the game: the settings are kept.
