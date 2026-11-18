# Save system

## Classes
- **`ULISaveGame`** (`Save/LISaveGame.h`): `SaveVersion`, `WorldSeed`, `SavedAt`, `Players[]`.
  Each `FLIPlayerSaveData` holds `PlayerKey`, `Location`, `Rotation`, `Health/Hunger/Thirst/Stamina` and `InventorySlots`.
- **`ULISaveSubsystem`** (GameInstance subsystem) handles saving, loading, new games and level restarts. Slot: `LostIsland_Slot0`.

## What is saved
| Data | Source | How it's restored |
|---|---|---|
| World seed | `ALIIslandGenerator::GetSeed()` | `ALIGameMode::ChooseIslandSeed` rebuilds the **same** island |
| Player position and look direction | pawn + controller | `TeleportTo` + `SetControlRotation` |
| Survival stats | `ULISurvivalComponent` | `SetStatValue` (health is at least 1) |
| Inventory | `ULIInventoryComponent::GetSlots()` | `SetSlots` (unknown items are dropped, stacks are clamped) |

## Flow
1. **Map start** (`ALIGameMode::InitGame` calls `PrepareForNewMap`): if a save exists and the URL has neither `?NewGame` nor `?Seed=`, the save is loaded into memory (`PendingSave`).
2. The island is generated from `PendingSave->WorldSeed`.
3. When a player first appears (`FinishRestartPlayer` calls `ApplyToPlayer`), their state is restored.
   Respawns after death **don't** restore the save; the player starts fresh.
4. **Save** (`SaveCurrentGame`): written **asynchronously** (no hitch). Dead players keep their previous state. Co-op players who are offline keep their entries.
5. **Load** (`LoadAndRestart`): reloads the level with the save. **New game** (`NewGameAndRestart`): deletes the save and starts a random island.

## Controls
| Action | Key | Console |
|---|---|---|
| Quick save | F5 | `LISave` |
| Load | F9 | `LILoad` |
| New game (deletes the save!) | — | `LINewGame` |
| Autosave | every 5 min (`AutosaveInterval` on the GameMode, 0 = off) | — |

In co-op, **only the host** can save or load (the server enforces this). Each client is saved under its unique net ID.

## Versioning
`ULISaveGame::CurrentVersion = 1`. A save from a newer version is ignored with a warning. Migration code for older versions goes in `ULISaveSubsystem::LoadFromDisk`.

## Limitations (v1)
- The state of resource nodes (depleted or respawning) isn't saved. On load the island has all its resources again.
- The game doesn't save on exit (on purpose: at that point the pawns may already be destroyed). Use F5 or autosave.
- No save-slot menu yet, only one slot. When a main menu exists, `bLoadSaveOnStart` should be set to false.
