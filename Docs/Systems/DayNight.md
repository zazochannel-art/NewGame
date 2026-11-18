# Day/night cycle and campfire (Milestone 9)

## World clock: `ALIGameState`
- **1 in-game hour = 60 real seconds**, so a full day takes 24 minutes (`RealSecondsPerGameHour`). The game starts on **day 1 at 08:00**.
- **Night: 20:30 to 05:30** (`NightStartHours` / `NightEndHours`).
- Networking: the server replicates only an **anchor** (the server time at which the clock showed X hours). Each machine computes the current time from the synchronised server time (`GetServerWorldTimeSeconds`), so there's **no constant network traffic**.
- A local 10 Hz timer (no Tick) moves the light through `ALIIslandGenerator::ApplyTimeOfDay`.

## Sun and moon
| Time | Light |
|---|---|
| 06:00 | sun rises (warm colour near the horizon) |
| 12:00 | at its highest (`SunIntensity` 10 lux, neutral light) |
| 18:00 | sunset |
| night | a dim, bluish moon (`MoonIntensity` 0.35 lux): dark, but playable |

- This only works if the island generator created its own sun. If your map has its own `DirectionalLight`, the clock still runs, but the light doesn't move.

## Cold (hard difficulty)
At night, without a lit campfire within **6 m**, you're **cold** (`ULISurvivalComponent::IsCold`):
- hunger drops **1.6x** faster (the body burns calories to stay warm);
- stamina regenerates at **half** speed.

The HUD shows `COLD - find or build a fire` above the bars. Cold is computed on the server and replicated to the owner.

## Campfire: `ALICampfire`
- **Recipe**: 5 Wood + 6 Stone, giving the **Campfire** item (key 1 with the inventory open; it's first in the list).
- **G** (gamepad: D-pad down) places it on the ground 2.2 m in front of you. It fails under water, on steep slopes, or if something is in the way. Hint: `[G] Place Campfire`.
- It starts with **2 min** of fuel. **E** with wood in the inventory: `Add Wood (burns 1:45)` adds **+90 s**, up to a maximum of 10 min.
- While lit: orange flame + point light (no shadows, cheap), and **warmth within 6 m**. When the fuel runs out it goes out, and wood relights it.
- Generic: any item with `PlaceableActorClass` can be placed with G.

## Saving (save v2)
- New fields: `Day`, `Hours`, and `Structures` (class, transform and fuel for each campfire).
- **v1 saves load fine**, as day 1 08:00 with no structures.
- `LIRegenerateIsland` removes the campfires (they belonged to the old terrain).

## Debug
- `LISetTime 21`: jump to night.
- `LISetTime 12`: noon.
