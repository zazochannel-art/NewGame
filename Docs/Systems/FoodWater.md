# Food and water (Milestone 7)

Without this, hunger and thirst could only go down, so every game ended in death after ~30 min.

## Consumables
`ULIItemDefinition` has new fields: `FoodValue`, `WaterValue` and `HealthValue`. An item is consumable when at least one of them is set (`IsConsumable()`).

| Built-in item | Category | Food | Water | Stack |
|---|---|---|---|---|
| Berries | Food | +8 | +2 | 20 |

New food types are added through Data Assets (`/Game/Items`), with no code.

## Eating: the F key (gamepad: D-pad up)
- `ULIInventoryComponent::FindBestConsumable()` picks the item that **best covers what's actually missing**: min(food, hunger deficit) + min(water, thirst deficit). Over-eating isn't rewarded, and if the benefit is below 1 nothing is offered.
- `RequestConsumeBest()` asks the server, which re-validates and calls `ConsumeItem()`: removes 1 item and applies the effects to `ULISurvivalComponent`.
- The HUD shows `[F] Eat Berries (x4)` at the bottom, only when an item would help, and `Consumed Berries` after eating.

## Food source: `ALIBerryBushNode`
A resource node (red sphere placeholder): 1–2 berries per pick, 2 picks, respawns in 15 min, 1 stamina per pick.
The island places **18 bushes** in forest and meadow, at least 9 m apart.

## Water source: `ALIWaterSource` (fresh-water spring)
- Interact (E): **+15 thirst** per drink. Infinite, but limited by the interaction rate (0.4 s).
- Only offered when you're thirsty (missing at least 2 thirst).
- The island places **5 springs** inland (meadow, forest, hills), at least **60 m** apart. Placed first so the spacing is guaranteed.
- Placeholder: a flat blue disc.

## Hard difficulty balance
- Thirst drops ~4/min, hunger ~2.5/min. With springs 60 m+ apart, the player has to plan routes.
- The food on the island exceeds what one player needs, but it's spread out. The real cost is the journey: hunger, thirst and stamina.
- Seawater is **not** drinkable. On purpose.

## Spawn spacing (generator change)
`FLIResourceSpawnRule::NodeClass` is now `TSubclassOf<AActor>`, so it can place any type of actor whose origin is at ground level.
The minimum spacing of a rule only applies **between points of the same type**. Different types only need `OtherTypeClearance` (3.5 m). Without this change, the springs' 60 m spacing would have blocked trees around them.

## Crabs and cooking (Milestone 14)

- `ALICrabNode` is a resource node (Raw Crab Meat, 2 hits, 1-2 per hit, respawn 600 s) that moves: it wanders within 6 m of its beach spawn point (70 cm/s, short pauses) and runs from any player within 4 m (200 cm/s). It stays on ground above Z = 15 cm (never in the sea). Movement is server-side and replicated. 14 crabs spawn on Beach (spawn rule appended last so old saves keep their node indices).
- Raw Crab Meat: +3 food. Cooked Crab Meat: +22 food, -2 water.
- Recipe `CookedMeat` (1 Raw Crab Meat) has `bRequiresFire`: craftable only within a lit campfire's warmth radius. The HUD marks such recipes `(at a fire)`.
- Debug: `LICraft <RecipeId>`.
