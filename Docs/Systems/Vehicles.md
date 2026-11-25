# Vehicles and the farm valley

The game is moving from island survival to **building and farming with vehicles**. This is the first step.

## The farm valley (default world)

`ALIIslandGenerator` has a valley mode (`SetValleyMode(true)`, chosen by `ALIGameMode::bValleyWorld`, on by default):

- A wide valley floor (~650 cm above sea level, gentle +-220 cm rolls) ringed by mountains from 62% of the radius. The sea is only beyond the mountains.
- A flat **farmyard** (44 x 44 m) in the middle: gravel colour, no grass, no trees.
- A **dirt road**: a wandering loop around the valley plus a straight road (with an S-bend) through the yard. Roads follow the smooth floor, so they are easy to drive.
- Trees, rocks, bushes, berries and springs as before, kept off the roads and the yard. No wreck, crabs or boars.
- The player starts in the yard; a **pickup**, a **tractor** and a **dump truck** are parked side by side in its north half.

URL option: `?World=Island` plays the original survival island, `?World=Valley` the valley. Each world has its own save slot (`LostIsland_Slot0`, `Valley_Slot0`), so old island saves are kept.

## ALIVehicle

A drivable pawn built entirely in code, so it needs no skeletal mesh, physics asset or Chaos Vehicle setup:

- **Chassis**: a physics-simulated box (`UBoxComponent`, mass override, low centre of mass).
- **Wheels**: one line trace per wheel. Spring + damper (stiffness from the weight and `RestCompression`), sideways grip and engine / brake force along the wheel's heading, limited together by a friction circle (`TyreGrip` x load).
- **Steering** shrinks with speed (`HighSpeedSteerFactor`). **Handbrake** locks the rear wheels and lowers their grip.
- **Camera**: spring arm behind the vehicle; the mouse looks around and the camera drifts back behind after 1.5 s. It looks backwards while reversing.
- **Getting in / out**: the vehicle is an `ILIInteractable` ("Drive Pickup"). The server possesses it with the character's controller and seats the character (`ALICharacter::SetSeatedVehicle`: hidden, no collision, attached; replicated, so the MetaHuman is hidden on every machine). E gets out on the driver's side, the other side, behind or in front, wherever the capsule fits.
- If the driver dies at the wheel they are put outside and the normal respawn runs.
- Networking: the server simulates; the driver sends throttle / steer / handbrake with an unreliable RPC (repeated every 0.25 s). Other machines receive the input and run the same suspension, with replicated physics correcting them.

`ALIPickupTruck` is the first vehicle: 1700 kg, 4x4, 95 km/h, low-poly body with a cargo bed (original design and colours).

### ALITractor

3800 kg, 32 km/h, big driven rear wheels and small steering front wheels (tight 42 degree lock, extra grip). Open cab under a sun roof, original teal and cream livery. Plough, seeder and harvester on its hitch: see [Farming.md](Farming.md).

### ALIDumpTruck

9000 kg, six wheels (steering front axle, two driven rear axles), 70 km/h, heavy and slow to stop. **Q** tips the bed up to 48 degrees on its rear hinge and back down; it only starts tipping below 5 km/h. The bed is cosmetic for now: cargo comes with the building step.

Each vehicle can have a tool on Q (`HasTool`, `ToggleTool`, replicated `bToolActive`); the HUD adds its hint.

Saving: a player at the wheel is saved as the seated character. Vehicles are not saved yet; they are parked in the yard again on load.

## Controls in a vehicle

| Action | Keyboard / mouse | Gamepad |
|---|---|---|
| Drive / brake, reverse when stopped | W / S (or arrows) | RT / LT |
| Steer | A / D (or arrows) | Left stick |
| Handbrake | Space | B / Circle |
| Get out | E | X / Square |
| Put back on the wheels | R | Y / Triangle |
| Tool: tip the bed (dump truck) | Q | RB / R1 |
| Look around / camera distance | Mouse / wheel | Right stick |

## Test checklist (PIE)

1. Play: you start in the yard and see the pickup in front of you. Roads and the yard are bare earth, the valley is ringed by mountains.
2. Look at the pickup: "E Drive Pickup". Press E: the camera moves behind the pickup, the character disappears, the HUD shows km/h.
3. W accelerates to ~95 km/h, S brakes then reverses (~22 km/h), A/D steer, Space slides the rear.
4. Drive the loop road; the suspension should soak up the rolls without bouncing.
5. E: you get out next to the driver's door. The pickup stays parked (does not roll away on a slope).
6. Tractor: slow (~32 km/h) but turns tightly and climbs the rolls without slipping.
7. Dump truck: heavy, ~70 km/h, long braking. Stopped, Q raises the bed, Q again lowers it; while driving Q does not raise it.
8. Roll a vehicle over on a mountain slope: the HUD suggests R; R puts it back on its wheels.
9. `?World=Island` still plays the old island with its own save.

## Next

- Building: buy materials, haul them with the truck, build a barn or a house.

## Vehicle dealer

Four bays on the west side of the farmyard, each with a blue price board. Look at a board and press E to buy that vehicle; it is delivered onto the bay, facing the yard. The bay has to be clear, and the farm can have at most 12 vehicles.

| Vehicle | Coins |
|---|---|
| Pickup | 300 |
| Tractor | 700 |
| Dump truck | 1000 |
| Grain trailer | 250 |

## Saving

In the valley, every vehicle is saved with its position, the tractor's implement, the dump truck's load and the trailer's grain. A trailer that was hitched is hitched again to the nearest tractor on load. A player saved at the wheel comes back standing next to the vehicle. Saves from before this change start with the default vehicles.

PIE: buy a tractor (coins go down, it appears on the bay); buy again with the bay occupied (refused); drive vehicles around, load the dump truck, fill the trailer, F5, F9: everything is where it was.

## Physics and effects

- Anti-roll bars (`AntiRoll`, a fraction of the spring rate): wheels across an axle share their compression difference, so bodies lean less in corners and roll over far less.
- Air resistance (`DragArea`, Cd x area in m^2): the dump truck (5.0) tops out earlier than the pickup (0.9).
- Air control (`AirControl`): with all wheels off the ground the vehicle gently levels itself to land on its tyres.
- Headlights (spot light, no shadows) come on from 19:30 to 6:30 while someone drives. A red tail glow is dim at night and bright while braking or on the handbrake. Trailers only get the tail glow.
- Dust: tan puffs (world-space instanced spheres) rise from grounded wheels above 6 km/h, more with speed. They are cosmetic and simulated on every machine, but not on a dedicated server.
- The camera's field of view widens from 88 to 102 degrees with speed.

PIE: corner hard with the pickup (it leans, but much less than before); drive at night (lights on); brake (tail glow); jump off a slope (it lands level); drive on dirt (dust).

## Fuel

Every vehicle with an engine burns diesel while someone sits at the wheel: a little at idle, more with the throttle down. The trailer has no engine.

| Vehicle | Tank | Full throttle lasts about |
|---|---|---|
| Pickup | 60 L | 9 min |
| Tractor | 150 L | 14 min |
| Dump truck | 200 L | 14 min |

- The HUD shows a fuel gauge above the speedometer (green, amber below 35%, red below 15%). A message warns at 15% and when the tank is empty.
- **Empty tank:** the engine limps along at a quarter of its power and no faster than 12 km/h, so you can always crawl back to the pump.
- **Fuel pump** (`ALIFuelPump`): the red pump under the canopy on the north side of the yard road, east of the parked vehicles. Park within about 8 m, get out, look at the pump and press E: "Refuel the Tractor: 120 L (180 coins)". 1.5 coins per litre; with too few coins it fills as much as you can pay for.
- New vehicles (start of a game, the dealer) come with a full tank. The tank level is saved with the vehicle; older saves load with full tanks.

Test: drive the pickup until the gauge drops (or watch it at full throttle), park by the pump, E: coins go down, gauge full. Drive one until empty: the warning appears and it only crawls.
