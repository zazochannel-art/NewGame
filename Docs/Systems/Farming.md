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

Harvested grain goes into the **grain trailer** (2000 kg), which starts hitched behind the tractor, behind the implement. Without a hitched trailer, or with a full one, the harvester does not cut (a message says why). The tractor HUD shows the trailer's load.

- Hitch / unhitch: get out, look at the trailer and press E. To hitch, the tractor's coupling (behind the implement) must be within about 4 m of the trailer's drawbar; the trailer then lines up behind the tractor. Unhitched, it stands on its brakes. It also lets go if the tractor is reset with R.
- Selling: park the loaded trailer within about 18 m of the **grain silo** (north-west corner of the yard), walk up to the silo and press E ("Unload and sell 1800 kg grain (+900 coins)"). A finished barn pays 25% more.

The farm starts with 500 coins; coins pay for building materials (see [Construction.md](Construction.md)). The trailer's load is not saved yet (vehicles are not saved).

Saving: coins, grain and every field's patches (state and growth) are saved with the valley game.

## Test checklist (PIE)

1. Four fields with corner posts around the yard; no grass blades or trees on them.
2. Tractor: T cycles Plough / Seeder / Harvester (the model behind changes). Q lowers it, the speed drops to ~14 km/h.
3. Drive over a field with the plough: brown furrows appear behind the tractor; the HUD line shows the ploughed percentage.
4. Seeder over ploughed ground: darker sown soil. After ~1 minute green shoots, later tall green wheat, after 8 minutes golden wheat.
5. Harvester over ripe wheat with the trailer hitched: it turns to stubble and the trailer fills (grain visible in the bin). Unhitched: nothing is cut and a message says to hitch the trailer.
6. Tow the trailer to the silo: it follows through turns without jitter. E at the silo empties it, coins go up, a "Sold grain" message appears.
7. F5 save, F9 load: fields, coins and grain are as they were.
