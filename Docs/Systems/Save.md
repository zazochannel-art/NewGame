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
`ULISaveGame::CurrentVersion = 3`. A save from a newer version is ignored with a warning. Migration code for older versions goes in `ULISaveSubsystem::LoadFromDisk`.

| Version | Adds |
|---|---|
| v1 | world seed; per player: position, survival stats, inventory |
| v2 | world clock (day, hour), placed campfires (class, transform, fuel) |
| v3 | crash-site searches left; every gathered or depleted resource node (spawn index, class, hits left, respawn countdown) |

Older saves load with the defaults of the missing fields (day 1 08:00, no campfires, fresh wreck and nodes).

Resource nodes are stored by their index in `ALIIslandGenerator::GetSpawnedNodes()`. That order is deterministic for a seed, and the class name is checked as well, so a node is left fresh if the spawn rules changed between versions.

## Respawn after death
Only a player's **first** spawn in a map session can restore saved state. `LIRevive` (and later a real respawn) always starts fresh on the beach, even after F5 or an autosave.

## URL options after an in-game restart
F9 and `LINewGame` restart the level and the original URL options (`?Seed=`, `?NewGame`) are carried along. They're **ignored** on those restarts, so F9 uses the save's seed and `LINewGame` really gives a new random island.

## Limitations
- Resources are saved (v3), so save + load can no longer be used to refill the wreck or the nodes.
- The game doesn't save on exit (on purpose: at that point the pawns may already be destroyed). Use F5 or autosave.
- No save-slot menu yet, only one slot. When a main menu exists, `bLoadSaveOnStart` should be set to false.
