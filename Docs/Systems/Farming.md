# Farming

The farm loop in the valley: **plough, sow, wait, harvest, sell**.

## Fields (`ALIFarmField`)

- Four fields of 20 x 20 m, one in each corner around the farmyard, marked by wooden corner posts. The valley generator flattens them (`GetFieldRects`, `GetFieldWeight`) and keeps grass, trees and rocks off them.
- Each field is a grid of 2 m patches (10 x 10). A patch goes:
  Grass -> **Plowed** (plough) -> **Sown** (seeder) -> Sprout -> Growing -> **Ripe** (time) -> **Stubble** (harvester, gives grain) -> plough again.
- Growing from sowing to ripe takes `GrowthSeconds` (8 real minutes by default). Ploughing destroys whatever grows.
- The server owns the state; the per-patch states replicate as one byte array, and every machine rebuilds the look (soil and furrows as a procedural mesh, wheat as instanced cones) at most 4 times a second.

## Tractor implements

The tractor carries one implement on its hitch:

| Key | Action |
|---|---|
| **T** (gamepad LB) | switch implement: Plough, Seeder, Harvester (only while raised) |
| **Q** (gamepad RB) | lower / raise the implement |

With the implement lowered the tractor is limited to 14 km/h, and every patch under the implement's 3.2 m width is worked as you drive. The HUD shows the field under the tractor ("Field 2: ploughed 40% growing 30% ripe 0%").

## Selling

Harvested grain goes into the farm's store (top left on the HUD: coins and grain). The **grain silo** in the north-west corner of the yard buys it: look at it and press E ("Sell 2500 kg grain (+1250 coins)"). The farm starts with 500 coins; coins will pay for buildings in the next step.

Saving: coins, grain and every field's patches (state and growth) are saved with the valley game.

## Test checklist (PIE)

1. Four fields with corner posts around the yard; no grass blades or trees on them.
2. Tractor: T cycles Plough / Seeder / Harvester (the model behind changes). Q lowers it, the speed drops to ~14 km/h.
3. Drive over a field with the plough: brown furrows appear behind the tractor; the HUD line shows the ploughed percentage.
4. Seeder over ploughed ground: darker sown soil. After ~1 minute green shoots, later tall green wheat, after 8 minutes golden wheat.
5. Harvester over ripe wheat: it turns to stubble and the grain count goes up.
6. Silo: E sells the grain, coins go up, a "Sold grain" message appears.
7. F5 save, F9 load: fields, coins and grain are as they were.
