# Visuals: art built in code (Milestones 18–19)

The project still has no `.uasset` art, and none can be authored outside the editor. Every model is therefore
built in C++ at runtime as **flat-shaded, faceted low-poly geometry**, using the engine's lit
`BasicShapeMaterial` with one tinted instance per colour.

## Building blocks: `FLILowPolyMesh` (`Visuals/LILowPolyMesh.h`)
- Primitives: `AddTri`, `AddBox`, `AddFrustum` (cylinder / cone / tapered trunk, optional jitter) and
  `AddBlob` (lumpy icosphere, optional "top colour" for moss, sunlit leaves or hair).
- Every triangle is wound automatically to face away from a point inside the shape, with the same front-face
  convention as the terrain (verified in the engine). Primitives stay correct under any transform.
- Geometry is grouped by colour (quantised to 1/64) into procedural mesh sections. `LIGetTintedMaterial`
  caches one material instance per colour, so all trees share their bark material.
- `LIVisualSeed(Location)` seeds the variation from the actor's position, rounded to 0.5 m, so every machine
  builds the same variant without replicating anything.

## Models
| Object | Model | Notes |
|---|---|---|
| Tree | 60% **pine** (6-sided trunk + 3–4 rotated cone tiers), 40% **broadleaf** (taller trunk, side branch, 3–4 canopy blobs lighter on top) | 0.85–1.2× size |
| Rock | lumpy boulder (half of them mossy on top) + 1–3 pebbles | sized to the collision ellipsoid |
| Fiber bush | 10–15 thin leaning grass blades over a low leafy base | mixed dry/green |
| Berry bush | three leafy blobs + 12–18 red berries | |
| Crab | flat shell (pale belly), 6 legs, two claws, eye stalks | faces its movement direction |
| Spring | dark rim, water surface, 10–13 edge stones, two clumps of reeds | |
| Campfire | ash bed, 10 ring stones, 5-log teepee; separate 3-cone flame shown only while lit | |
| Survivor | pelvis, tapered torso, head with hair, eyes and nose, backpack with bedroll and straps; separate arms and legs | animated (below) |

The original engine shapes stay as **hidden collision / interaction proxies** (`SetHiddenInGame`), so gathering,
focus traces, depletion and all gameplay behave exactly as before. If a model can't be built, the tinted
placeholder is shown instead.

### Survivor animation
Until a skeletal mesh is assigned, a 30 Hz timer (no Tick, every machine) swings the limbs:
- **walk / sprint**: legs and arms swing in opposite phase, amplitude and cadence follow the ground speed, small body bob;
- **swimming**: alternating front-crawl arms and a quick flutter kick;
- **falling**: arms raised to the sides;
- **death**: the animation stops and the body lies down.

Assigning a skeletal mesh in a Blueprint subclass hides all of this automatically.

## Environment (`ALIIslandGenerator`, built with the terrain on every machine)
- **Shallow water**: a flat overlay 2 cm above sea level over the coastal cells: white **foam** where a cell
  touches the shore, **turquoise** down to `ShallowWaterDepth` (160 cm), **teal** to `MidWaterDepth` (420 cm),
  then the darker deep-ocean plane.
- **Ground cover** (`bGenerateDecorations`, `DecorationDensity`, `DecorationCullDistance` = 70 m): instanced grass
  tufts (meadow green, darker in forests, dry on high beach and hills), flowers (white, yellow, pink) in meadows,
  pebbles on beaches, hills and steep slopes, shells on beaches. They sit exactly on the faceted surface
  (`GetMeshHeightAt` follows the mesh triangles), with no collision and no shadows.
- **Clouds** (`CloudCount` = 16): clusters of flattened puffs 120–170 m up, white on top and grey-blue underneath.
  The layer turns `CloudTurnsPerDay` times per in-game day, driven by the clock, so every machine sees the same sky.
- **Fog**: light distance haze (density 0.006, starts at 40 m).
- **Look** (post process, `LookSaturation` 1.15, `LookContrast` 1.06, `LookBloomIntensity` 0.4,
  `LookVignetteIntensity` 0.35, `LookAmbientOcclusion` 0.6): a little more colour, soft bloom, gentle vignette, and
  ambient occlusion so models sit on the ground.

## Performance notes
- Models: roughly 150–700 triangles each, ~250 actors, about 2–3 colour sections each.
- Ground cover: on the order of 20–25k instances of engine cones and spheres, culled beyond 70 m. If FPS is low,
  lower `DecorationDensity` or `DecorationCullDistance` on a Blueprint subclass of the generator.
- Not done yet: the plane wreck, building pieces, raft, chest, sleeping bag and dropped loot still use tinted
  engine shapes, and the HUD is still the canvas prototype.

## Realistic style (Milestone 19, default on)
Console variable **`li.RealisticStyle`** (1 = realistic, default; 0 = the faceted low-poly look). It applies to
objects built after the change, so use `LIRegenerateIsland` or restart PIE to see a switch.

Since the project has no material assets, two materials are **generated in code at startup**
(`Visuals/LIMaterials.cpp`, using the editor's material editing API):
- **`M_LI_VertexColorLit`**: opaque and lit, base colour = vertex colour, roughness 0.82, specular 0.3.
- **`M_LI_Water`**: translucent, lit per pixel. Turquoise and clear over the sand near the shore, deep
  blue-green and almost opaque by ~5 m of depth (depth fade), a white foam line where the water meets the beach,
  glossy, with a Fresnel sheen.

They exist only in **editor builds** (PIE, or Standalone Game launched from the editor). A packaged game falls back to
the low-poly look automatically. When real material assets are made, `LIGetVertexColorMaterial` / `LIGetWaterMaterial`
should load them instead. The first PIE after a build compiles their shaders, so objects can show the default
material for a few seconds.

What changes with it on:
| Area | Realistic style |
|---|---|
| Terrain | one smooth mesh with shared vertices and smooth normals; colours blend per vertex between sand, meadow, forest, hills and rock; the seabed goes from wet sand to dark green-grey with depth; finer grid (192 × 192 cells of 2 m, same 384 m island) |
| Sea | translucent water: you see the sand under the shallows, foam at the waterline, and reflections of the sky, sun and clouds. The opaque shallow-water overlay is not built |
| Models | rounded (smooth normals, one extra subdivision on blobs), colour per vertex with slight shade noise, and darker where trees, rocks and bushes meet the ground |
| Sky | engine volumetric clouds (`/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst`) replace the low-poly clouds |
| Lighting | `DefaultEngine.ini`: Lumen global illumination and reflections, virtual shadow maps, mesh distance fields, TSR |
| Look | neutral grading: saturation 1.05, contrast 1.02, bloom 0.5, vignette 0.25, AO 0.5 |

Performance: Lumen and volumetric clouds need a recent GPU (DX12, ~GTX 1070 / RX 5600 or better). On a weaker PC,
lower Scalability in the editor (Settings → Engine Scalability) or set `li.RealisticStyle 0`.
