# Death, respawn and the sleeping bag

Milestone 11.

## Death

When health reaches 0, `ALICharacter::HandleDeath` tells `ALIGameMode::HandlePlayerDeath` (server):

1. The whole inventory goes into an `ALIDroppedLoot` bundle at the feet, and the inventory is cleared. `[E] Take Dropped Items` moves everything that fits; tools keep their durability (`AddItem` has a durability override). The bundle disappears when empty or after 30 minutes. Bundles are **not** saved.
2. After `RespawnDelay` (6 s) the dead pawn is destroyed and a new one spawns with fresh stats: at the player's sleeping bag if they have one (`RestartPlayerAtTransform`), otherwise at the beach start.

`LIRevive` respawns immediately the same way. The delayed respawn skips a player who was already revived.

## Sleeping bag

| | |
|---|---|
| Recipe | 20 Fiber + 4 Wood (sort order 6, after the Campfire) |
| Place | G, same placement rules as the campfire (flat ground above sea level, free space) |
| Owner | placing a bag or sleeping in it makes it your respawn point; your previous bag is released |
| Sleep (E) | always sets the respawn point. At night in single player it also skips to `NightEndHours + 0.5` (≈ 06:00) and costs 15 food and 15 water |
| Save | stored with the campfires in `Structures`, including `OwnerKey` (player key from `ULISaveSubsystem::GetPlayerKey`) |

Debug: `LIPlace` does the same as G.

Follow-ups: choose which placeable G places when you carry several, co-op sleeping (everyone in a bag), and a ragdoll on death.
