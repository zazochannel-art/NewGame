# Plane wreck (crash site)

See it in the browser preview: `Tools/IslandPreview/island-preview.html` (the wreck with the smoke column).

## `ALICrashSite` (`Source/LostIsland/Public/World/LICrashSite.h`)
A small crashed plane: the island's landmark and the story's starting point.

### Placement: "random", but deterministic
`ALIIslandGenerator::ChooseCrashSite()` picks the location from the **world seed** (`FRandomStream(HashCombine(Seed, 0xC4A5E))`):
- biome **Beach** or **Meadow** (`CrashSiteBiomes`);
- the whole area (front, back including the debris trail, both sides, ±7 m) is **dry and fairly flat** (normal Z ≥ 0.88, height ≥ 0.6 m);
- at least **60 m from the player's start point** (`CrashSiteMinDistanceFromSpawn`), so finding the wreck is a small expedition. The requirement is relaxed (30 m, then 0) only if the island has no spot that far;
- random orientation (yaw);
- no resource spawns within **18 m** of it (`CrashSiteClearance`), so the landmark stays easy to read.

A different seed gives a different location. The same seed gives **the same location** for all co-op players and after loading a save. The server spawns the actor, which replicates.

### Gameplay
- E: **Search Wreckage (N left)**. 4 searches in total, shared by all players. Each costs 3 stamina and rolls the loot table twice:
  | Item | Quantity | Weight |
  |---|---|---|
  | Scrap Metal (new) | 2–4 | 3 |
  | Fiber | 3–6 | 2 |
  | Fiber Bandage | 1 | 2 |
  | Berries | 2–4 | 2 |
  | Wood | 2–4 | 1 |
- With a full inventory, the search isn't used up.
- **Scrap Metal** is a new resource (stack 30), reserved for better tools later.
- Blueprint event `On Searched` (server), for sound and particles.

### Visuals (placeholder)
Built from engine shapes:
- fuselage 9 m long, red nose and tail fin;
- intact left wing; right wing broken off at the root (burnt stub);
- the torn wing lying in the sand ~7 m away;
- 5 burnt debris pieces along the skid trail.

The plane body is tilted nose-down (−7°) and rolled 11° onto the broken side, slightly sunk into the ground. In a Blueprint subclass the parts can be replaced with a real low-poly mesh.

### Debug
`LIGoToCrashSite` teleports you 10 m from the wreck, looking at it.

### Limitations
- The number of searches left is saved (save v3), so F5 + F9 doesn't refill the wreck.
