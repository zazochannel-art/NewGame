# Resources and gathering

## `ALIResourceNode` (`Resources/LIResourceNode.h`)
A gatherable object that implements `ILIInteractable`. Each interaction (one "hit"):
1. Checks (client for the prompt, server for validation): the node isn't depleted, and the player is alive and has at least `StaminaCostPerHit` stamina.
2. Server: adds `MinYield..MaxYield` of `ResourceItemId` to the gatherer's inventory.
   **If the inventory is full, nothing happens**: no stamina is spent and the node loses no hit.
3. Spends stamina (which also delays regeneration) and decrements `RemainingHits`.
4. At 0 hits the node hides and loses its collision, then respawns after `RespawnTime` (0 means never).

`bDepleted` and `RemainingHits` replicate. The prompt shows the remaining hits: `Chop Tree (4/6)`.
Blueprint events: `On Gathered` (server) and `On Depleted Changed` (all machines, for VFX or a stump mesh).

The actor's origin is at the object's **base**, which makes placement and procedural generation simpler.

## Presets (hard difficulty, no tools yet)
| Class | Resource | Yield/hit | Hits | Stamina/hit | Respawn | Placeholder shape |
|---|---|---|---|---|---|---|
| `ALITreeNode` | Wood | 2–3 | 6 | 6 | 10 min | tall cylinder |
| `ALIRockNode` | Stone | 1–2 | 8 | 8 | 15 min | flattened sphere |
| `ALIFiberBushNode` | Fiber | 2–4 | 3 | 2 | 5 min | cone |

## Pacing
`ULIInteractionComponent::MinInteractInterval` (0.4 s) is enforced on the server, so holding or spamming E can't gather faster than that.

## Test
`ALIGameMode` (when `bSpawnPrototypeTestActors` is on) places 3 of each type in a ring 7–11 m around the PlayerStart, leaving the crates in front free.

## Next
Tools (axe/pickaxe) that multiply yield, gather animations and sounds, and procedural spawning on the island (Milestone 5) with instanced meshes.
