# LOST ISLAND: Development Plan

## First compile (2026-11-18)

Built on Windows 11 with **UE 5.8.4** and Visual Studio 2026 (MSVC 14.52): **Development Editor | Win64 succeeds**.

| Problem | Fix |
|---|---|
| `error C4458`: local `OldPawn` hides `AController::OldPawn` (`LIPlayerController.cpp`) | renamed to `ExistingPawn` |
| `LNK2019 FUniqueNetIdWrapper::ToString` (`ULISaveSubsystem::GetPlayerKey`) | added `CoreOnline` to the module's private dependencies |
| Project pointed at UE 5.5 | `EngineAssociation` set to `5.8` |
| Terrain rendered white in PIE (the engine debug `VertexColorMaterial` is unlit and blows out) | terrain split into one mesh section per surface colour, each with a tinted `BasicShapeMaterial` instance |
| Noon washed out (auto exposure off, default EV) | unbound post process with manual exposure, `ExposureCompensation = -3`; `MoonIntensity` raised to 2.5 lux to match |

Headless smoke run (`-game -nullrhi`, ~4 min): the item registry loads (9 items, 4 recipes), the island generates in ~80 ms (219 actors, crash site placed), the character spawns with generated bindings, and autosave writes `LostIsland_Slot0`. The game module logged no errors and nothing crashed. Milestones 1–9 still need the in-editor PIE checklists below.

Build environment notes:
- Windows **Smart App Control** blocks the DLLs that UBT compiles locally (`0x800711C7`). It must be off to build C++.
- A project path with non-ASCII characters (e.g. `Рабочий стол`) breaks MSVC's PCH path (`C1083`). Keep the project in an ASCII path outside OneDrive, e.g. `C:\Dev\NewGame`.
- The interface default `_Implementation` bodies and the `LIItems::*` inline `FName` globals compiled without problems.

## 1. Project inspection (2026-11-18)

| Item | Finding |
|---|---|
| Repository | `zazochannel-art/NewGame`. Empty: no commits, no files, nothing on the remote. |
| Unreal project | **None existed.** No `.uproject`, no `Source/`, no `Config/`, no `Content/`. |
| Engine version | Not determinable from the repo. **Targeting UE 5.5.** You can change `EngineAssociation` in `LostIsland.uproject`; the target files use `BuildSettingsVersion.Latest` and `EngineIncludeOrderVersion.Latest`, so other 5.x versions should work as well. |
| Existing Third Person character / GameMode | None. |
| Compilation status | Compiled on 2026-11-18 with UE 5.8; see "First compile" above. |

Because nothing existed, Milestone 1 creates a minimal C++ project. It does not replace any template. If a local Third Person template project exists that was never pushed, push it and these classes can be merged into it. All new classes are prefixed `LI` and live in the `LostIsland` module, so they won't collide with template classes.

## 2. Milestones

| # | Milestone | Status |
|---|---|---|
| 1 | Third Person character: movement, camera, jump, sprint, interaction foundation, anim-instance base | **Compiles; awaiting PIE test** |
| 2 | Survival component: health, hunger, thirst, stamina (timer-driven, configurable, replicated) | **Compiles; awaiting PIE test.** See [Systems/Survival.md](Systems/Survival.md) |
| 3 | Resource nodes: wood, stone and fiber; gathering through `ILIInteractable` | **Compiles; awaiting PIE test.** See [Systems/Resources.md](Systems/Resources.md) |
| 4 | Inventory component: item definitions (Data Assets), IDs, stacks, add/remove, basic UI | **Compiles; awaiting PIE test.** See [Systems/Inventory.md](Systems/Inventory.md) |
| 5 | Procedural island: seeded terrain (beach, meadow, forest, hills), water, environment, resource spawn points | **Compiles; awaiting PIE test.** See [Systems/Island.md](Systems/Island.md) |
| 6 | SaveGame: player transform, survival values, inventory, world seed | **Compiles; awaiting PIE test.** See [Systems/Save.md](Systems/Save.md) |
| 7 | Food and water: consumables, berry bushes, fresh-water springs, eat key | **Compiles; awaiting PIE test.** See [Systems/FoodWater.md](Systems/FoodWater.md) |
| 8 | Crafting and tools: recipes, stone axe and pickaxe, bandage, durability | **Compiles; awaiting PIE test.** See [Systems/Crafting.md](Systems/Crafting.md) |
| 8b | Plane wreck: seed-placed landmark, searchable loot, Scrap Metal | **Compiles; awaiting PIE test.** See [Systems/CrashSite.md](Systems/CrashSite.md) |
| 9 | Day/night cycle, cold at night, placeable campfire with fuel, save v2 (v3: wreck and resources) | **Compiles; awaiting PIE test.** See [Systems/DayNight.md](Systems/DayNight.md) |
| 10 | Swimming: deep water, swim stamina, sinking when exhausted, breath and drowning, `LIGoToSea` | **Compiles; headless test passed (swim + drown); awaiting PIE test.** See [Systems/Swimming.md](Systems/Swimming.md) |
| 11 | Death and respawn: dropped inventory bundle, respawn after 6 s, sleeping bag (craft, place, respawn point, sleep through the night) | **Compiles; headless test passed (place bag, die, drop, respawn at bag); awaiting PIE test.** See [Systems/DeathRespawn.md](Systems/DeathRespawn.md) |
| 12 | Building: wooden floor, wall and roof on a 3 m grid with snapping; a roof overhead blocks the night cold; G places the last crafted placeable | **Compiles; placement tested (screenshot); awaiting PIE test.** See [Systems/Building.md](Systems/Building.md) |
| 13 | Build mode (B) with a green placement preview, dismantle pieces with E (item back), wooden doorway | **Compiles; preview and doorway checked (screenshot); awaiting PIE test.** See [Systems/Building.md](Systems/Building.md) |
| 14 | Crabs on the beaches (wander, flee, catch with E) -> Raw Crab Meat; cooking at a lit campfire -> Cooked Crab Meat; recipes on keys 1-9 and 0 | **Compiles; headless test passed (14 crabs spawned, cooking only at a fire); awaiting PIE test.** See [Systems/FoodWater.md](Systems/FoodWater.md) |
| 15 | Scrap Axe and Scrap Pickaxe (metal tools), Empty Flask filled at springs into a Water Flask (drink returns the flask), crafting pages (Q) | **Compiles; headless test passed (metal tools, fill only at a spring, drink returns flask); awaiting PIE test.** See [Systems/Crafting.md](Systems/Crafting.md) |
| 16 | Goal: Raft Kit (60 Wood, 40 Fiber, 6 Scrap) placed in shallow water at the shore; E sails away, everyone sees the ending, then a new island starts | **Compiles; headless test passed (craft, place at shore, ending, new island); awaiting PIE test.** See [Systems/Raft.md](Systems/Raft.md) |
| 17 | Storage chest (20 Wood + 4 Fiber, 30 stacks): E stores resources and food, R takes everything; saved with contents | **Compiles; headless test passed (store 85 items, take back, tools stay); awaiting PIE test.** See [Systems/Inventory.md](Systems/Inventory.md) |
| 18 | Wild boars (7, forest and meadow): charge players within 9 m, bite for 8 HP every 1.5 s, 6 hits for 6-12 Raw Meat; crabs and boars share ALIAnimalNode | **Compiles; headless test passed (boar charges, bites, kills an idle player, respawn); awaiting PIE test.** See [Systems/FoodWater.md](Systems/FoodWater.md) |

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

## 4c. Milestone 3 + 4 test checklist

- [ ] On Play, 3 trees (tall cylinders), 3 rocks (spheres) and 3 bushes (cones) appear around the player.
- [ ] Looking at a tree shows `[E] Chop Tree (6/6)`. E adds 2–3 Wood, `+N Wood` appears, and stamina drops by 6.
- [ ] Spamming E doesn't gather faster than about one hit every 0.4 s.
- [ ] After 6 hits the tree disappears (it returns after 10 min; set `RespawnTime` lower to test this).
- [ ] With stamina below the hit cost the prompt disappears, then comes back once stamina regenerates.
- [ ] Tab opens the inventory: Wood, Stone and Fiber stacks with quantities. Wood stacks up to 50 per slot.
- [ ] `LIGive Wood 2000` fills the inventory. After that, gathering shows `Inventory full` and spends no stamina.
- [ ] `LIClearInventory` empties the inventory.
- [ ] Co-op: the client gathers, sees its own inventory, and the tree disappears for both players.

## 4d. Milestone 5 test checklist

- [ ] Play: an island appears with water, sky and sun. The log shows `Island generated: seed X, ~195 resource nodes, T ms`.
- [ ] The player starts on a beach, facing inland.
- [ ] Visible terrain: sand at the edge, light green meadows, dark green forest, grey-green hills and rock on slopes, a faceted low-poly look.
  If you only see water and fall through, the winding is still wrong: set `bFlipWinding` (see Island.md) and report it.
- [ ] Trees in the forest, rocks on hills and meadows, bushes all over. All of them can be gathered.
- [ ] `LIRegenerateIsland 42` twice gives the same island both times. `LIRegenerateIsland 7` gives a different island.
- [ ] `?Seed=42` in the URL gives the same island as the command above.
- [ ] Co-op: the client sees the same terrain and the same resources as the host.
- [ ] Measure the generation time (log) and FPS on the target PC.

## 4e. Milestone 6 test checklist

- [ ] Gather a few resources, walk somewhere, note your health, food and water. Press F5: "Game saved" appears top-right, and the log shows `Save 'LostIsland_Slot0' written`.
- [ ] Walk somewhere else, use `LIClearInventory`. Press F9: the level reloads with the same island, you're back at the save point, and your stats and inventory are restored.
- [ ] Stop PIE and start it again: the game automatically continues the save (same island and position).
- [ ] `LIDamage 200` then `LIRevive`: you respawn on the beach with fresh stats, not the save's. Also after an F5 in the same session.
- [ ] `LINewGame`: a new random island and an empty inventory. F9 shows "No save found".
- [ ] `?Seed=5` or `?NewGame` in the URL ignores the existing save.
- [ ] Started with `?Seed=5`: `LINewGame` gives a different (random) island, and F9 loads the save's island.
- [ ] Chop a tree twice and deplete a bush, search the wreck once, F5, F9: the tree shows 4/6, the bush is still gone, the wreck says `3 left`.
- [ ] Co-op: a client can't save. The host's save includes the client; when the client reconnects to the loaded world, their state comes back.

## 4f. Milestone 7 test checklist

- [ ] The island has red spheres (berry bushes) and flat blue discs (springs). There are about 5 springs, far apart.
- [ ] `LISetStat Thirst 40` and go to a spring: `[E] Drink Fresh Water`. Each press adds +15. At full thirst the prompt disappears.
- [ ] Pick berries: `+N Berries`. With hunger below max, `[F] Eat Berries (xN)` appears at the bottom.
- [ ] F eats one berry (+8 food, +2 water) and shows `Consumed Berries`. At full hunger and thirst the hint disappears and F does nothing.
- [ ] Balance: play 20–30 min without commands. Can you survive by drinking and eating? Note what you felt (too easy or too hard).
- [ ] Co-op: the client drinks and eats correctly, and the berry count updates.

## 4g. Milestone 8 test checklist

- [ ] Tab: a CRAFTING panel appears to the right of the inventory with 4 recipes (Campfire, Stone Axe, Stone Pickaxe, Fiber Bandage). Missing ingredients are red.
- [ ] `LIGive Wood 10`, `LIGive Stone 10`, `LIGive Fiber 20`. Press 2 with the inventory open (1 is the Campfire, which sorts first): Stone Axe appears in its own slot with a full green bar, and the ingredients are consumed exactly.
- [ ] With the inventory closed, 1 does nothing.
- [ ] Chop a tree with the axe: the prompt says `with Stone Axe`, you get 4–6 wood per hit, stamina drops by ~3.6, and the bar shrinks.
- [ ] `LIGive StoneAxe 1` twice: two axes in separate slots.
- [ ] After 40 hits: `Stone Axe broke!` and the slot empties.
- [ ] `LIDamage 30`, craft a bandage (4): `[F] Use Fiber Bandage` appears, and F gives +15 HP.
- [ ] F5, then F9: the axe keeps its remaining durability.
- [ ] Co-op: the client crafts and uses tools correctly.

## 4h. Plane wreck test checklist

- [ ] The log shows `crash site X=... Y=... Z=...` after `Island generated`.
- [ ] `LIGoToCrashSite`: the plane sits on a beach or meadow, not floating or buried too deep, with no trees or rocks through it.
- [ ] `[E] Search Wreckage (4 left)`: 2 random items per search, −3 stamina. After 4 searches the prompt disappears.
- [ ] `LIRegenerateIsland 42` twice gives the wreck in the same place. A different seed puts it elsewhere.
- [ ] It's at least ~60 m from the start point (see the preview: `Tools/IslandPreview`).
- [ ] Co-op: both players see the wreck in the same place, and the search counter is shared.

## 4i. Milestone 9 test checklist

- [ ] Top-left shows `Day 1   08:00   Day`, and the clock advances 1 hour per minute. The sun moves across the sky.
- [ ] `LISetTime 19`: still daylight (low, warm sun), `Day`, no cold. `LISetTime 21`: dark, bluish moonlight, `Night`. After at most 0.25 s, `COLD - find or build a fire` appears and the food bar drops faster.
- [ ] `LIGive Wood 10`, `LIGive Stone 10`, Tab, 1: Campfire in the inventory, and `[G] Place Campfire` appears.
- [ ] G places the fire in front of you, lit (flame + orange light). Within 6 m the `COLD` message disappears. Walk away and it comes back.
- [ ] E with wood: `Add Wood (burns m:ss)` and the time goes up. With no fuel the fire goes out, and wood relights it.
- [ ] G in the water or on a slope does nothing, and the item stays in the inventory.
- [ ] F5 at night next to the fire, then F9: same hour and day, the fire is in place with the same fuel.
- [ ] An older save (v1 or v2) still loads (v1: day 1, 08:00; v1/v2: fresh wreck and resources).
- [ ] Co-op: the client sees the same time and light, the cold works on the client, and the client's fire warms both players.

## 4j. Fix pass after code review (2026-11-18)

| Problem | Fix |
|---|---|
| Terrain triangles had the reverse of Unreal's front-face winding (invisible from above, fall-through) | `AddTriangle` now emits Unreal's order; `bFlipWinding` stays as a debug fallback |
| `LIRevive` after any save or autosave restored the save instead of a fresh spawn | every first spawn is recorded in `AppliedPlayerKeys`, save or not |
| F5 + F9 refilled the wreck and all resource nodes (infinite loot) | save v3 stores wreck searches and gathered/depleted nodes |
| A sprint rejected by the server could leave the client at sprint speed | `ClientCorrectSprinting` RPC when the server's result differs from the request |
| Dark from ~18:15 but cold only from 20:30 | sun and moon follow `NightStartHours` / `NightEndHours` |
| `?Seed=` from the launch URL overrode F9 / `LINewGame` restarts | URL options are ignored on in-game restarts |
| Campfire placement could fail on flat ground (`DontSpawnIfColliding` counted the terrain) | own free-space test that ignores the ground |
| `IndexOfByKey` with a const pointer on `TObjectPtr` array (compile risk) | explicit loop with `.Get()` |

## 5. Known limitations and follow-ups

- **Animations:** there is no skeletal mesh or Anim Blueprint in the repo, and no `.uasset` can be authored outside the editor. To set one up, migrate or import a mannequin, create `ABP_Survivor` with parent `LIAnimInstance`, and create `BP_Survivor` (parent `LICharacter`) with the mesh and AnimBP assigned. Then point the GameMode's `DefaultPawnClass` at it through a `BP_LIGameMode`.
- **Sprint in co-op:** sprint speed is predicted on the client and confirmed by an RPC. Under real latency this can cause small corrections. Before co-op, move sprint into a custom `UCharacterMovementComponent` with saved-move flags.
- **Input assets:** runtime-generated bindings work but can't be rebound in the editor. Create `IMC_Default` and `IA_*` assets and assign them on `BP_Survivor`; the C++ fallback is skipped automatically.
- **HUD:** the canvas HUD is temporary until the UMG inventory and HUD milestone.
- **Death:** automatic respawn and a dropped-items bundle exist (Milestone 11). Dropped bundles are not saved, and there is no ragdoll yet (the Blueprint event `On Died` is where it goes).
- **Survival difficulty:** values live on the component. A `ULIDifficultySettings` Data Asset (Easy/Normal/Hard) can come later.
- **Prototype crates:** they now spawn only when the island is disabled (`bGenerateIsland = false`) or `bSpawnPrototypeTestActors` is on.
- **Water:** swimming is faked with `MOVE_Flying` (no water volume exists). There is no diving input yet, and the sea has no waves or underwater fog.
- **Island scale:** resource nodes are separate actors (~195). HISM is the plan if performance or bandwidth requires it.

## 4k. Milestone 10 test checklist (swimming)

- [ ] `LIGoToSea`: you appear in deep water off the start beach and float with the head above the surface. The log shows `started swimming`.
- [ ] WASD swims at ~260 cm/s. Shift does not sprint in water.
- [ ] STAMINA drains slowly while swimming (faster when moving) and does not regenerate in deep water.
- [ ] Swim to the beach: once the water is shallow you walk again (`stopped swimming`), and stamina regenerates.
- [ ] `LISetStat Stamina 0` in deep water: you sink, an AIR bar appears and empties in ~15 s, then HP drops ~10/s until you die.
- [ ] Back at the surface the AIR bar refills and disappears.
- [ ] Co-op: the client swims and sinks the same way, and the host sees the client in the water.

## 4l. Milestone 11 test checklist (death, respawn, sleeping bag)

- [ ] Tab: the crafting list shows **Sleeping Bag** (20 Fiber + 4 Wood) right after the Campfire.
- [ ] `LIGive Fiber 20`, `LIGive Wood 4`, craft it, then G: a teal mat appears in front of you, and the log says it is your respawn point.
- [ ] `LIDamage 500`: "YOU DIED" plus "Respawning...". After ~6 s you stand next to the bag with fresh stats and an empty inventory.
- [ ] Where you died there is a brown bundle: `[E] Take Dropped Items (N)` gives everything back, and tools keep their wear.
- [ ] Without a bag you respawn on the start beach.
- [ ] At night (`LISetTime 22`), E on the bag says `Sleep until morning`: the clock jumps to ~06:00 and food and water drop by 15.
- [ ] Place a second bag: it becomes the respawn point and the first one stops being yours.
- [ ] F5, F9: the bag is still there and still your respawn point.
- [ ] Co-op: each player respawns at their own bag. Sleeping does not skip the night with 2 players.

## 4m. Milestone 12 test checklist (building)

- [ ] Tab: Wooden Floor (8 Wood), Wooden Wall (6 Wood + 2 Fiber), Wooden Roof (6 Wood + 4 Fiber) at the end of the list (keys 6, 7, 8).
- [ ] Craft a floor, G: a 3 x 3 m plank floor appears in front of where the camera looks. On water or steep slopes G does nothing.
- [ ] A second floor snaps next to the first, on the side you look at, at the same height.
- [ ] Walls snap to the floor edge nearest to where you look, standing on the floor. The same edge can't get two walls.
- [ ] A roof goes 3 m above the floor cell you look at.
- [ ] Night (`LISetTime 22`): under a roof the COLD message disappears; step out and it comes back.
- [ ] After crafting walls, G places walls even if a campfire or bag sits earlier in the inventory.
- [ ] F5, F9: all pieces come back in place.
- [ ] Co-op: both players see the same pieces and can build on each other's floors.

## 4n. Milestone 13 test checklist (build mode, dismantle, doorway)

- [ ] B toggles build mode: `BUILD MODE` banner at the top. With a building piece in the inventory, a green ghost shows where G will put it; it hides where G would fail.
- [ ] The ghost follows the camera and snaps exactly where the real piece lands.
- [ ] Looking at a placed piece: `[E] Dismantle Wooden Wall`; E removes it and gives the item back. With a full inventory nothing happens.
- [ ] Wooden Doorway (5 Wood + 2 Fiber, key 9) snaps like a wall; you can walk through the opening.
- [ ] Co-op: the ghost is only visible to its owner (also for the host), and it is never saved.

## 4o. Milestone 14 test checklist (crabs and cooking)

- [ ] Orange flat crabs walk around the beaches, pausing now and then, and never go into the sea.
- [ ] Walking toward a crab makes it run away (200 cm/s); sprinting catches it. `[E] Catch Crab (2/2)`: two hits give 1-2 Raw Crab Meat each, then the crab disappears and comes back home after 10 min.
- [ ] Raw meat can be eaten with F (+3 food).
- [ ] Tab: `[6] Cooked Crab Meat  (at a fire)` is grey away from a fire, even with raw meat. Next to a lit campfire it turns white; crafting gives Cooked Crab Meat (+22 food, -2 water).
- [ ] Crafting list shows 10 recipes: building pieces moved to keys 7, 8, 9 and the doorway to 0.
- [ ] `LICraft CookedMeat` does the same without opening the inventory.
- [ ] Co-op: crabs move the same for both players.

## 4p. Milestone 15 test checklist (metal tools, flask, crafting pages)

- [ ] Tab: the crafting panel title shows `1/2  [Q] next page`; Q switches pages and the number keys craft from the visible page.
- [ ] Search the wreck for Scrap Metal (2-4 per find). Scrap Axe = 2 Wood + 4 Scrap + 2 Fiber, Scrap Pickaxe = 2 Wood + 5 Scrap + 2 Fiber.
- [ ] With both a stone and a scrap axe, chopping uses the scrap axe (prompt `with Scrap Axe`): about 6-9 wood per hit and less stamina. It lasts 120 hits.
- [ ] Empty Flask = 2 Scrap + 2 Fiber. `Water Flask (at a spring)` is grey away from a spring and works next to one.
- [ ] F with low water drinks the flask (+30 water) and the Empty Flask comes back.
- [ ] Debug: `LIGoToSpring`, `LIConsume`, `LICraft <RecipeId>`.

## 4q. Milestone 16 test checklist (raft ending)

- [ ] Crafting page 2: `Raft Kit` (60 Wood, 40 Fiber, 6 Scrap Metal).
- [ ] On the beach facing the sea, G puts the raft in the shallow water in front (0.3-2.5 m deep). On dry land or in deep water nothing happens. `LIGoToShore` puts you in a good spot.
- [ ] Wade to the raft: `[E] Sail away from the island`. The screen darkens: `YOU ESCAPED THE ISLAND`, `Rescued on day N`.
- [ ] About 15 s later a new random island starts, and the old save is gone (F9: "No save found").
- [ ] F5 before sailing, F9: the raft is still at the shore.
- [ ] Co-op: either player can sail; both see the ending and both move to the new island.

## 4r. Milestone 17 test checklist (storage chest)

- [ ] Crafting page 2: Storage Chest (20 Wood + 4 Fiber). G places it like a campfire.
- [ ] Looking at it: `[E] Store materials (0/30)   [R] Take all`.
- [ ] E moves every resource and food stack into the chest; tools, bandages, flasks and placeables stay in the inventory. The counter goes up.
- [ ] R takes everything back that fits; leftovers stay in the chest.
- [ ] F5, F9: the chest is in place with the same contents.
- [ ] Co-op: both players see the same counter; one stores, the other takes.
- [ ] Debug: `LIChest store`, `LIChest take` (nearest chest).

## 4s. Milestone 18 test checklist (wild boars)

- [ ] Dark brown long shapes roam forests and meadows (7 per island, far apart).
- [ ] Within ~9 m a boar charges (330 cm/s). Next to you it bites: -8 HP every 1.5 s.
- [ ] Sprinting (650 cm/s) outruns it; it gives up about 27 m from home. Swimming does not (it stops at the water's edge, though).
- [ ] `[E] Fight Boar (6/6)`: six hits give 6-12 Raw Meat (no tool bonus yet); then it disappears and returns after 20 min.
- [ ] Raw Meat and Cooked Meat are now generic (crab or boar).
- [ ] Debug: `LIGoToBoar` (6 m from the nearest boar).
- [ ] Co-op: the boar picks the nearest living player; both see it move.
