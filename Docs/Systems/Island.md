# Procedural island

## `ALIIslandGenerator` (`Source/LostIsland/Public/World/LIIslandGenerator.h`)
Builds a small island (~350 m across by default) from a single **int32 seed**.

### Pipeline
1. **Seed to noise offsets** (`FRandomStream(Seed)`).
2. **Height** `GetHeightAt(XY)`:
   - radial mask with a noisy coastline (`CoastNoiseStrength`)
   - fractal Perlin hills (`HillHeight`, `HillContrast`) that grow toward the centre
   - small surface detail
   - outside the island, the seabed sits at `SeabedHeight`
3. **Biome** `GetBiomeAt(XY)`:
   | Biome | Condition |
   |---|---|
   | Ocean | h < 0 |
   | Beach | h < 3 m |
   | Hills | h > 15 m |
   | Forest / Meadow | separate Perlin mask (`ForestThreshold`) |
4. **Terrain mesh**: a 192x192-cell grid (2 m cells; was 128 x 3 m), **flat-shaded low-poly** (face normals, a vertex colour per face with ±6% variation), with collision. Steep slopes are coloured as rock.
5. **Water**: a plane at Z=0, 4x the size of the island.
6. **Environment**: sun, sky atmosphere, real-time sky light and fog. These turn off automatically if the map already has a `DirectionalLight`.
7. **Spawn points**: one placement rule per resource type (`FLIResourceSpawnRule`: allowed biomes, count, minimum spacing, maximum slope).
   | Rule | Biomes | Count | Spacing |
   |---|---|---|---|
   | Trees | Forest | 90 | 6.5 m |
   | Rocks | Hills, Meadow | 45 | 8 m |
   | Fiber bushes | Forest, Meadow, Beach | 60 | 4.5 m |
8. **Player spawn**: `FindPlayerSpawnTransform()` searches for flat, dry beach and faces the player toward the island centre.

### Networking (co-op)
- **Only `Seed` replicates.** Each client rebuilds the identical terrain locally in `OnRep_Seed`, because all the functions are deterministic.
- **Only the server** spawns the resource nodes, which replicate as normal actors.

### Seed configuration
The seed is chosen in this order (`ALIGameMode::ChooseIslandSeed`):
1. URL option: `?Seed=12345` (e.g. `UnrealEditor.exe LostIsland.uproject Entry?Seed=12345 -game`)
2. `IslandSeed` on the GameMode (0 = random)
3. random

From Milestone 6 on, a loaded save takes priority (it keeps the world seed).

Debug: `LIRegenerateIsland 12345` (or `0` for a random seed) rebuilds the island and moves the players to the new beach.

### Map
The default map is the empty engine map `/Engine/Maps/Entry`, and the GameMode generates everything. You can create your own empty map `L_Island`. If you add your own sun to it, the generator won't create one.

### Cost and performance
- Generation runs once. There's no Tick. The duration is logged (`Island generated: seed X, N resource nodes, T ms`).
- ~98k vertices and ~195 resource nodes. Collision is cooked synchronously, so the player can't fall through the terrain on the first frame.
- Planned optimisations: decorative vegetation through HISM (instanced meshes), resource nodes as HISM instances plus a lightweight server registry instead of separate actors, terrain split into chunks with LOD.

### Tuning
The parameters were tuned offline with a Python simulation of the same function, across 4 seeds. The result was ~40% land, ~5% beach and 2–7% hills, with peaks of 27–47 m. The actual output in the engine still needs checking in PIE.

### Known risk: triangle winding
The generator emits triangles in Unreal's front-face order: `CrossProduct(B - A, C - A)` points away from the viewer, i.e. **down** for an upward-facing terrain triangle (the same convention as `UKismetProceduralMeshLibrary::GenerateBoxMesh`). The first version had this reversed. It still hasn't been verified in the engine: **if the terrain is invisible from above** (you only see water and fall through), set `bFlipWinding = true` on the generator, through a Blueprint subclass assigned to `ALIGameMode::IslandGeneratorClass`, and report it.
