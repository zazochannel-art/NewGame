# LOST ISLAND

An original stylized low-poly survival game set on a procedurally generated island.
Windows PC / Steam · Unreal Engine 5 · C++ with Blueprint configuration · single-player first, 2–4 player co-op later.

## Status

**Milestones 1–16: character + interaction, survival (health/hunger/thirst/stamina), resource gathering, inventory, procedural island, save system, food and water, crafting and tools, plane wreck, day/night with campfires., swimming, death and respawn, building, crabs and cooking, metal tools and flasks, and the raft escape (the goal of a run).** The code is written. It **compiles with UE 5.8** (Development Editor, Win64) and is awaiting in-editor testing; see [Docs/DevelopmentPlan.md](Docs/DevelopmentPlan.md).

**New direction (2026-11-25): building and farming with vehicles.** A new game starts in a farm valley with a drivable pickup, tractor and dump truck (E to get in, Q tips the dump bed). Plough, sow and harvest the four fields with the tractor (T switches the implement, Q lowers it), tow the grain trailer to the silo and sell the harvest. See [Docs/Systems/Farming.md](Docs/Systems/Farming.md). Buy more vehicles at the dealer on the west side of the yard, and keep them fuelled at the diesel pump by the yard road. Spend the coins on gravel, lumber and bricks at the yard depots, haul them with the dump truck and tip them on the plots to build a barn and a farmhouse: [Docs/Systems/Construction.md](Docs/Systems/Construction.md). Showers roll over the valley now and then: crops grow faster in the rain, but the roads get slippery ([Docs/Systems/Weather.md](Docs/Systems/Weather.md)). The original island is still there with `?World=Island`. See [Docs/Systems/Vehicles.md](Docs/Systems/Vehicles.md).

## Getting started (Windows)

1. Install **Unreal Engine 5.5** from the Epic Games Launcher, and **Visual Studio 2022** with the "Game development with C++" workload.
   If you use another 5.x version, right-click `LostIsland.uproject` → *Switch Unreal Engine version*.
2. Run `git lfs install` once. Binary assets are stored with Git LFS.
3. Right-click `LostIsland.uproject` → *Generate Visual Studio project files*.
4. Open `LostIsland.sln`, select **Development Editor | Win64**, and build.
5. Open the project and press **Play**.

The project contains no `.uasset` content yet. It starts in the empty engine map and generates the island from a seed (`?Seed=123`). Input bindings are generated in C++, and the low-poly art (trees, rocks, crabs, the survivor, foam, grass, clouds) is built in code at runtime, so it runs as soon as it compiles. See [Docs/Systems/Visuals.md](Docs/Systems/Visuals.md).

## Controls

| Action   | Keyboard / Mouse | Gamepad          |
|----------|------------------|------------------|
| Move     | WASD             | Left stick       |
| Look     | Mouse            | Right stick      |
| Jump     | Space            | A / Cross        |
| Sprint   | Hold Left Shift  | L3               |
| Interact | E                | X / Square       |
| Zoom     | Mouse wheel      | —                |
| Inventory| Tab              | Menu             |
| Eat / drink / heal | F      | D-pad up         |
| Craft (inventory open) | 1–9 | —            |
| Place campfire | G          | D-pad down       |
| Save / Load | F5 / F9       | —                |
| Drive a vehicle | E (look at it) | X / Square    |

In a vehicle: W/S drive and brake (reverse when stopped), A/D steer, Space handbrake, E get out, R put it back on its wheels. Full table in [Docs/Systems/Vehicles.md](Docs/Systems/Vehicles.md).

## Layout

```
LostIsland.uproject
Config/                       Engine, game and input settings
Source/LostIsland/
  Public|Private/Core/        GameMode, PlayerController
  Public|Private/Character/   ALICharacter, ULIAnimInstance
  Public|Private/Interaction/ ILIInteractable, ULIInteractionComponent, ALIInteractableActor
  Public|Private/Survival/    ULISurvivalComponent, survival types
  Public|Private/Items/       ULIItemDefinition, ULIItemRegistrySubsystem, item types
  Public|Private/Inventory/   ULIInventoryComponent
  Public|Private/Crafting/    ULIRecipeDefinition, ULICraftingComponent
  Public|Private/Resources/   ALIResourceNode + Tree / Rock / FiberBush presets
  Public|Private/World/       ALIIslandGenerator (procedural island / farm valley)
  Public|Private/Farm/        ALIFarmField (patches, growth), ALIGrainSilo (sells grain)
  Public|Private/Construction/ ALIMaterialDepot, ALIConstructionSite (barn, farmhouse)
  Public|Private/Vehicles/    ALIVehicle (raycast-wheel physics), ALIPickupTruck, ALITractor, ALIDumpTruck, ALIGrainTrailer, ALIVehicleDealer
  Public|Private/Save/        ULISaveGame, ULISaveSubsystem
  Public|Private/UI/          ALIHUD (prototype canvas HUD: bars + prompt)
Docs/                         Development plan and per-system documentation
```

All classes use the `LI` prefix (`ALICharacter`, `ULIInteractionComponent`, …).
