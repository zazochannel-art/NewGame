# Weather

Rain showers in the farm valley. The island keeps its clear sky.

## Cycle (`ALIGameState`)

- Clear sky for 6 to 14 real minutes (`ClearSeconds`), then a shower of 2 to 5 minutes (`RainSeconds`) with a random strength between 55% and 100%, then clear again.
- The sky takes about 40 s (`RainFadeSeconds`) to cloud over or clear up.
- Networking works like the clock: the server replicates only where the current fade starts, where it ends and when it started. Every machine computes `GetRainAmount()` (0 = clear, 1 = downpour) from the shared server time, so all players see the same shower with no constant traffic.
- Weather is not saved; a loaded game starts clear.

## What the rain changes

| | Effect at full rain |
|---|---|
| Light | sun 70% weaker and cooler, darker sky, colours less saturated, no golden hour |
| Fog | thicker haze that starts at 5 m instead of 40 m |
| Rain | up to 900 falling streaks in an 18 m column around your camera (`ALIRainEffect`, local only, never on a dedicated server) |
| Crops | grow 50% faster (`RainGrowthBonus`) |
| Vehicles | tyres lose 20% grip (`RainGripLoss`), wheels hardly raise dust, headlights come on in heavy rain |
| HUD | the clock line says "Rain (crops grow faster)" or "Heavy rain (...roads slippery)" |

## Debug

- `LIRain 1` starts a downpour (fades in over 8 s), `LIRain 0.5` a light shower, `LIRain 0` clears the sky. The normal cycle continues from there.

## Test checklist (PIE)

1. Valley game, console `LIRain 1`: within a few seconds it gets darker and hazier, streaks fall around the camera, the HUD says "Heavy rain".
2. Drive the pickup hard into a corner: it slides earlier than in the dry. Little or no dust behind the wheels. Headlights are on during the day.
3. Sow a field and watch: the crops reach "ripe" noticeably sooner than 8 minutes while it rains.
4. `LIRain 0`: the sky clears, the rain stops, the headlights go off within a second.
5. Two players (listen server): both see the rain start and stop at the same moment.
6. `?World=Island`: `LIRain 1` works for testing, but no showers come on their own.
