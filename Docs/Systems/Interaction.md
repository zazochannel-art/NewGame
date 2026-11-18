# Interaction system

## Flow

```
[Owning client]                                   [Server]
Timer (0.1 s) -> ScanForInteractable()
  sphere sweep along the camera ray, starting
  at the point nearest the pawn
  -> actor implements ILIInteractable,
     is in reach and CanInteract()
  -> SetFocusedActor: OnFocusEnd/OnFocusBegin,
     OnFocusChanged broadcast (HUD)
Press E -> TryInteract()
  -> ServerInteract(Target) ------------------->  PerformInteract(Target)
                                                    re-check: implements, reach (+tolerance), CanInteract
                                                    ILIInteractable::Interact(Target, Pawn)
                                                    OnInteracted broadcast
```

On a listen server or in standalone play, `TryInteract` calls `PerformInteract` directly.

## `ILIInteractable`
| Function | Runs on | Default |
|---|---|---|
| `CanInteract(Pawn)` | Client (focus) and server (validation) | `true` |
| `Interact(Pawn)` | Server only | no-op |
| `GetInteractionPrompt(Pawn)` | Client | "Interact" |
| `OnFocusBegin/End(Pawn)` | Local client only | no-op |

Always call these through the `ILIInteractable::Execute_*` wrappers so Blueprint implementations work.

To make something interactable: in C++, inherit `ILIInteractable` and override the `_Implementation` functions. In Blueprint, open Class Settings, add the `LIInteractable` interface, and implement the events.

## `ULIInteractionComponent` tunables
| Property | Default | Meaning |
|---|---|---|
| `InteractionDistance` | 250 cm | Reach, measured from the pawn's centre to the target's bounds |
| `TraceRadius` | 20 cm | Sweep radius |
| `ScanInterval` | 0.1 s | Focus update rate |
| `TraceChannel` | Visibility | Channel the sweep uses |
| `ServerDistanceTolerance` | 100 cm | Extra reach the server allows to absorb latency |
| `bDrawDebugTrace` | false | Draws the sweep (development builds only) |

## `ALIInteractableActor`
A test and base actor: static mesh (engine cube by default), `PromptText`, and `MaxUses` (0 means unlimited). `UseCount`, `PromptText` and `MaxUses` replicate. The Blueprint event `On Interacted` fires on the server. Focus toggles custom depth, ready for a future outline post-process.

The resource nodes in Milestone 3 will implement `ILIInteractable` directly. Gathering will also need tool checks, hit counts and respawn timers, so they won't derive from this actor.
