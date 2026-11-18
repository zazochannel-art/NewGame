# LOST ISLAND: Development Plan

## 1. Project inspection (2026-11-18)

| Item | Finding |
|---|---|
| Repository | `zazochannel-art/NewGame`. Empty: no commits, no files, nothing on the remote. |
| Unreal project | **None existed.** No `.uproject`, no `Source/`, no `Config/`, no `Content/`. |
| Engine version | Not determinable from the repo. **Targeting UE 5.5.** You can change `EngineAssociation` in `LostIsland.uproject`; the target files use `BuildSettingsVersion.Latest` and `EngineIncludeOrderVersion.Latest`, so other 5.x versions should work as well. |
| Existing Third Person character / GameMode | None. |
| Compilation status | **Not compiled.** The development container has no Unreal Engine or MSVC toolchain. The first build has to happen on a Windows machine with UE 5.5 installed. |

Because nothing existed, Milestone 1 creates a minimal C++ project. It does not replace any template. If a local Third Person template project exists that was never pushed, push it and these classes can be merged into it. All new classes are prefixed `LI` and live in the `LostIsland` module, so they won't collide with template classes.

## 2. Milestones

| # | Milestone | Status |
|---|---|---|
| 1 | Third Person character: movement, camera, jump, sprint, interaction foundation, anim-instance base | **Code written; awaiting first compile** |
| 2 | Survival component: health, hunger, thirst, stamina (timer-driven, configurable, replicated) | **Code written; awaiting first compile.** See [Systems/Survival.md](Systems/Survival.md) |
| 3 | Resource nodes: wood, stone and fiber; gathering through `ILIInteractable` | Planned |
| 4 | Inventory component: item definitions (Data Assets), IDs, stacks, add/remove, UMG UI | Planned |
| 5 | Procedural island: seeded heightmap (beach, forest, hills), instanced trees and rocks, resource spawn points | Planned |
| 6 | SaveGame: player transform, survival values, inventory, world seed | Planned |

Each milestone ends with a compile, a PIE test, and a fix pass before the next one starts.

## 3. Milestone 1: what was built

| File | Purpose |
|---|---|
| `LostIsland.uproject`, `Source/*.Target.cs`, `Source/LostIsland/LostIsland.Build.cs` | Project and module definition |
| `Config/DefaultEngine.ini` | Default GameMode, startup map, DX12 |
| `Config/DefaultInput.ini` | Enhanced Input player and component classes |
| `Config/DefaultGame.ini` | Project name and version |
| `Core/LIGameMode` | Default classes; optionally spawns 3 test crates |
| `Core/LIPlayerController` | Game-only input mode (will own UI later) |
| `Character/LICharacter` | Movement, look, zoom, jump, replicated sprint, input |
| `Character/LIAnimInstance` | AnimBP parent with locomotion variables |
| `Interaction/LIInteractable` | Interface for anything the player can interact with |
| `Interaction/LIInteractionComponent` | Timer-based focus sweep and server-validated interact |
| `Interaction/LIInteractableActor` | Test and base interactable (mesh, prompt, use limit) |
| `UI/LIHUD` | Centre dot and interaction prompt |

See [Systems/Character.md](Systems/Character.md) and [Systems/Interaction.md](Systems/Interaction.md).

## 4. Milestone 1 test checklist (run in PIE after compiling)

- [ ] Project compiles in Development Editor with no errors or warnings.
- [ ] Press Play: the placeholder cylinder spawns on the floor, and the log shows "using generated default bindings".
- [ ] WASD moves relative to the camera; the body turns toward the movement direction.
- [ ] Mouse look works; moving the mouse up looks up. The wheel zooms between 150 and 700 cm.
- [ ] Space jumps.
- [ ] Holding Shift raises speed from 380 to 650 cm/s; releasing it returns to 380.
- [ ] Looking at a crate within about 2.5 m shows `[E] Inspect Crate`. Looking away hides it.
- [ ] E on the "3 uses" crate works three times, then the prompt disappears.
- [ ] Co-op sanity check: PIE with 2 players (Listen Server). Sprint and interaction work on the client, and crate use counts match on both sides.

## 4b. Milestone 2 test checklist

- [ ] Four bars appear bottom-left. FOOD and WATER start at 80% and drop slowly.
- [ ] Holding Shift while moving drains STAMINA. At 0 the sprint stops and the bar turns grey. Sprint can't start again until the bar is back to 30%.
- [ ] Holding Shift while standing still doesn't drain stamina.
- [ ] After you stop sprinting, stamina starts regenerating about 1.5 s later.
- [ ] `LISetStat Thirst 0`: HP drops by ~0.8/s.
- [ ] `LISetStat Hunger 100` + `LISetStat Thirst 100`: HP slowly regenerates.
- [ ] `LIDamage 200`: "YOU DIED", the character stops moving and interacting. `LIRevive` respawns it with fresh stats.
- [ ] Co-op (2 players): the client sees its own bars update correctly, and exhaustion stops sprinting on the client too.

## 5. Known limitations and follow-ups

- **Animations:** there is no skeletal mesh or Anim Blueprint in the repo, and no `.uasset` can be authored outside the editor. To set one up, migrate or import a mannequin, create `ABP_Survivor` with parent `LIAnimInstance`, and create `BP_Survivor` (parent `LICharacter`) with the mesh and AnimBP assigned. Then point the GameMode's `DefaultPawnClass` at it through a `BP_LIGameMode`.
- **Sprint in co-op:** sprint speed is predicted on the client and confirmed by an RPC. Under real latency this can cause small corrections. Before co-op, move sprint into a custom `UCharacterMovementComponent` with saved-move flags.
- **Input assets:** runtime-generated bindings work but can't be rebound in the editor. Create `IMC_Default` and `IA_*` assets and assign them on `BP_Survivor`; the C++ fallback is skipped automatically.
- **HUD:** the canvas HUD is temporary until the UMG inventory and HUD milestone.
- **Death:** no respawn or loot drop yet, only the `LIRevive` debug command. The Blueprint event `On Died` is where ragdoll goes.
- **Survival difficulty:** values live on the component. A `ULIDifficultySettings` Data Asset (Easy/Normal/Hard) can come later.
- **Prototype crates:** turn off `bSpawnPrototypeTestActors` on the GameMode once real content exists.
