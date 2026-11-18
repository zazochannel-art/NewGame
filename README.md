# LOST ISLAND

An original stylized low-poly survival game set on a procedurally generated island.
Windows PC / Steam · Unreal Engine 5 · C++ with Blueprint configuration · single-player first, 2–4 player co-op later.

## Status

**Milestones 1–8: character + interaction, survival (health/hunger/thirst/stamina), resource gathering, inventory, procedural island, save system, food and water, crafting and tools.** The code is written. It has **not been compiled yet**; see [Docs/DevelopmentPlan.md](Docs/DevelopmentPlan.md).

## Getting started (Windows)

1. Install **Unreal Engine 5.5** from the Epic Games Launcher, and **Visual Studio 2022** with the "Game development with C++" workload.
   If you use another 5.x version, right-click `LostIsland.uproject` → *Switch Unreal Engine version*.
2. Run `git lfs install` once. Binary assets are stored with Git LFS.
3. Right-click `LostIsland.uproject` → *Generate Visual Studio project files*.
4. Open `LostIsland.sln`, select **Development Editor | Win64**, and build.
5. Open the project and press **Play**.

The project contains no `.uasset` content yet. It starts in the empty engine map and generates the island from a seed (`?Seed=123`). Input bindings are generated in C++, and placeholder shapes stand in for the character and objects, so it runs as soon as it compiles.

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
| Save / Load | F5 / F9       | —                |

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
  Public|Private/World/       ALIIslandGenerator (procedural island)
  Public|Private/Save/        ULISaveGame, ULISaveSubsystem
  Public|Private/UI/          ALIHUD (prototype canvas HUD: bars + prompt)
Docs/                         Development plan and per-system documentation
```

All classes use the `LI` prefix (`ALICharacter`, `ULIInteractionComponent`, …).
