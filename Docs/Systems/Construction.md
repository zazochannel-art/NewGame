# Construction (farm valley)

Build a barn and a farmhouse in the yard with the dump truck. Everything is generated in C++; there are no assets.

## Loop

1. Earn coins: harvest grain with the tractor and sell it at the silo (see [Farming.md](Farming.md)). You start with 500.
2. Park the dump truck (empty, bed down, stopped) next to a depot on the east side of the yard. Get out, look at the depot and press E. One load is paid for and drops into the bed.
3. Drive to a staked-out plot. Back the tail of the truck up to it (within about 4.5 m of the tape), stop and press Q to tip the bed. The load slides out once the bed passes 35 degrees, and the building grows.
4. The board by each plot (E) says what the site needs next.

| Depot | Material | Coins per load |
|---|---|---|
| Gravel pit (north-east) | gravel | 40 |
| Timber yard (east) | lumber | 80 |
| Brick yard (south-east) | bricks | 70 |

| Building | Foundation | Walls | Roof | Total |
|---|---|---|---|---|
| Barn (south-west, doors face the yard) | 2 gravel | 3 lumber | 2 lumber | 480 coins |
| Farmhouse (south, door faces the road) | 1 gravel | 3 bricks | 2 lumber | 410 coins |

A finished barn makes grain sell for 25% more (shown next to the coins on the HUD). The walls get collision once the wall stage is done.

## Code

- `Source/LostIsland/Public|Private/Construction/LIConstruction.*`: `ALIMaterialDepot`, `ALIConstructionSite`, material names and prices.
- `ALIDumpTruck` (`Vehicles/LIFarmVehicles.*`): replicated `Cargo`, `CargoMesh` on the bed, `TryUnload()` on the server when the bed passes `UnloadAngle`.
- `ALIGameMode::SpawnConstruction()` places depots and plots in the valley yard (also on Regenerate).
- `ALIGameState::GrainPriceMultiplier` holds the barn bonus.
- Save: `ULISaveGame::Sites` stores loads delivered per site. The truck's cargo is not saved.

## PIE checklist

- Depot prompt without the truck nearby says to park it first. With the truck nearby: the coins go down and a load appears in the bed.
- Not enough coins: a message, nothing is spent.
- Tipping away from a plot: a message, the cargo stays. The wrong material at a plot: a message, the cargo stays.
- Tipping at the barn plot with gravel: the gravel bed rises. The second load gives a concrete slab, then the walls rise in thirds, then the roof is laid from the back.
- Finished barn: a message to every player, and the HUD shows +25%. Selling grain pays more.
- Save, quit, load: building progress is kept and a finished barn keeps the bonus.
- Listen server plus client: the client sees cargo, the tipping bed and the buildings growing.
