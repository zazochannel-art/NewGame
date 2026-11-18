# Survival system

## `ULISurvivalComponent` (`Source/LostIsland/Public/Survival/LISurvivalComponent.h`)

Owns **Health, Hunger, Thirst and Stamina** for a pawn. It is attached to `ALICharacter` as `SurvivalComponent`.

### Authority and performance
- **No Tick.** The server runs one looping timer (`UpdateInterval`, default 0.25 s).
- The values replicate. Health and death go to everyone, so co-op can show team health bars. Hunger, thirst, stamina and exhaustion go only to the owning player.
- Clients get `OnStatChanged` from the OnRep callbacks, so UI can be fully event-driven.
- Every function that changes a value runs on the server only (`BlueprintAuthorityOnly`, with an early return on clients).

### Interface
| Function | Where | Description |
|---|---|---|
| `GetStatValue / GetStatMax / GetStatPercent(Stat)` | any | Reads a stat |
| `IsDead()`, `IsExhausted()`, `CanSprint()` | any | State; `CanSprint` is also used for client-side prediction |
| `ModifyStat(Stat, Delta)` | server | +/- a value (eating, drinking...); negative health is routed through `ApplyDamage` |
| `SetStatValue(Stat, Value)` | server | Absolute value (SaveGame, debug) |
| `ApplyDamage(Amount, Instigator)` | server | Damage; returns the damage actually applied |
| `ResetStats()` | server | Restores start values, clears death |
| `SetSprinting(bool)` | owner | The character reports its sprint state |

Events: `OnStatChanged(Stat, Value, Max)`, `OnExhaustedChanged(bool)`, `OnDeath(Killer)`.

### Rules (defaults tuned for HARD difficulty)
| Rule | Default |
|---|---|
| Hunger decays | -0.042/s (full to empty in ~40 min), starts at 80% |
| Thirst decays | -0.067/s (full to empty in ~25 min), starts at 80% |
| Sprinting multiplies hunger/thirst decay | x2 |
| Starvation damage (hunger = 0) | 0.5 HP/s |
| Dehydration damage (thirst = 0) | 0.8 HP/s |
| Health regenerates only when hunger AND thirst are >= 60% | 0.1 HP/s |
| Sprint drains stamina (only while actually moving) | 18/s (~5.5 s of sprint) |
| Stamina regenerates after 1.5 s without drain | 12/s, halved when hungry or thirsty |
| At 0 stamina: **Exhausted** | Sprint locked until stamina is back to 30% |

All values are `EditDefaultsOnly` and can be changed in a Blueprint subclass of the character (Survival component → Details).

### Integration with the character
- `ALICharacter::CanSprint` also requires `SurvivalComponent->CanSprint()`.
- `OnExhaustedChanged(true)` triggers `ForceStopSprinting()`.
- `OnDeath` turns off movement, input and interaction, then calls the Blueprint event `On Died` for ragdoll and UI.
- `TakeDamage` (the engine's damage system) is routed into `ApplyDamage`.

### Debug commands (console `~`, not in Shipping builds)
```
LISetStat Hunger 0      // Health | Hunger | Thirst | Stamina
LIDamage 30
LIRevive                // respawns a fresh character
```

### HUD
`ALIHUD` draws 4 bars bottom-left (HP, FOOD, WATER, STAMINA; the stamina bar turns grey while exhausted) and "YOU DIED" on death.
