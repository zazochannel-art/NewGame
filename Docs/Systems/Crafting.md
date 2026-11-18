# Crafting and tools (Milestone 8)

## Recipes
- **`ULIRecipeDefinition`** (`Crafting/LIRecipeDefinition.h`) is a Primary Data Asset of type `LIRecipe` with `RecipeId`, `DisplayName`, `Ingredients[]`, `Output` and `SortOrder`. The Asset Manager scans `/Game/Recipes`.
- `ULIItemRegistrySubsystem` also holds recipes (`FindRecipe`, `GetRecipes()`, sorted). Data Assets override built-in recipes with the same ID.

| Built-in recipe | Ingredients | Output |
|---|---|---|
| Stone Axe | 3 Wood, 2 Stone, 4 Fiber | Stone Axe (40 uses) |
| Stone Pickaxe | 3 Wood, 4 Stone, 4 Fiber | Stone Pickaxe (40 uses) |
| Fiber Bandage | 6 Fiber | Fiber Bandage (+15 health, stack 10) |

## `ULICraftingComponent` (on the character)
- `CanCraft(RecipeId)` checks ingredients and room for the output. It works on the client too, for the UI.
- `RequestCraft` / `RequestCraftByIndex` go to the server through `ServerCraft`, which runs `Craft`: re-validate, remove ingredients, add the output. Crafting is instant.
- Known limitation: the room check doesn't count slots the ingredients would free up, so with a completely full inventory you can't craft.

## Tools
New fields on `ULIItemDefinition`:
| Field | Description |
|---|---|
| `ToolType` | None, Axe or Pickaxe |
| `ToolYieldMultiplier` | 2.0 by default |
| `ToolStaminaMultiplier` | 0.6 by default |
| `MaxDurability` | 0 = never breaks |

- Items with durability never stack (1 per slot). `FLIInventorySlot::Durability` stores the remaining uses and is saved automatically. Older saves without durability load as fresh tools.
- **No equip step:** when gathering, the best matching tool in the inventory is used automatically (`ALIResourceNode::EffectiveTool`).
  | Node | Tool |
  |---|---|
  | Tree | Axe |
  | Rock | Pickaxe |
  | Bushes | bare hands |
- With a tool: **double the yield**, **40% less stamina**, and each hit spends 1 use. At 0 uses: `Stone Axe broke!`.
- The prompt shows the tool in use: `Chop Tree (4/6) with Stone Axe`.

## Consumables
`FindBestConsumable` now also counts missing **health**, so F uses the bandage when you're hurt. The hint is `[F] Use ...`.

## Interface (Tab)
- The inventory shows a **durability bar** on tools (red below 30%).
- A **CRAFTING** panel to the right: `[1] Stone Axe` plus the ingredients (`3/3 Wood` in green, `1/2 Stone` in red).
- **Keys 1–9 only craft while the inventory is open**, so they stay free for a future hotbar.

## Balance (hard difficulty)
An axe costs ~1–2 trees of wood plus a bit of stone and fiber, and lasts 40 hits (~6–7 trees at 4–6 wood per hit). Tools pay off, but they have to be replaced regularly.

## Debug
`LIGive StoneAxe 1`, `LIGive Fiber 30`, and so on.
