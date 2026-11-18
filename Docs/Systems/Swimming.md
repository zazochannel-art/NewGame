# Swimming

Milestone 10. The island's sea is a flat plane at the island generator's origin, with no water volume. `ALICharacter` therefore computes the water depth itself every tick, on the server and on the owning client.

| Rule | Value (default) | Where |
|---|---|---|
| Start swimming | water deeper than `SwimStartDepth` (130 cm) at the feet | `ALICharacter` |
| Stop swimming | depth below 100 cm (30 cm hysteresis), then normal walking | `ALICharacter` |
| Swim speed | `SwimSpeed` 260 cm/s, no sprint | `ALICharacter` |
| Floating | capsule centre held `FloatDepth` (55 cm) below the surface | `ALICharacter::UpdateSwimming` |
| Stamina | -4/s swimming, -1/s treading water, no regeneration in deep water | `ULISurvivalComponent` |
| Exhausted in deep water | the body sinks slowly | `ALICharacter::UpdateSwimming` |
| Breath | 15 s under water, refills at 5 s per second at the surface | `ULISurvivalComponent` (`Breath`, replicated to the owner) |
| Drowning | -10 HP/s once breath is 0 | `ULISurvivalComponent` |

Movement uses `MOVE_Flying`: free movement without gravity, with the vertical velocity set each tick toward the float (or sink) height. Leaving the water switches to `MOVE_Falling`, which lands into walking.

The HUD shows an **AIR** bar above HP only while breath is below full.

Debug: `LIGoToSea` teleports you into deep water off the start beach.

Follow-ups: a dive key, underwater post-process and fog, swim animations (the AnimBP can read `IsSwimming()`), and a proper water volume once the level is an authored map.
