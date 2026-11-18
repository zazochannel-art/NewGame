# Character system

## `ALICharacter` (`Source/LostIsland/Public/Character/LICharacter.h`)

Third-person survivor pawn. It does not tick: all behaviour runs from input events or replication callbacks.

### Components
| Component | Notes |
|---|---|
| `CameraBoom` (SpringArm) | Follows the controller rotation, uses camera lag, length clamped to Min/MaxArmLength |
| `FollowCamera` | Attached to the boom socket |
| `InteractionComponent` | See [Interaction.md](Interaction.md) |
| `PlaceholderBody` | Engine cylinder. Hidden at BeginPlay if a skeletal mesh is assigned |

### Tunables (EditDefaultsOnly)
| Property | Default | Meaning |
|---|---|---|
| `WalkSpeed` | 380 cm/s | Normal movement speed |
| `SprintSpeed` | 650 cm/s | Speed while sprinting |
| `DefaultArmLength` | 400 cm | Starting camera distance |
| `MinArmLength` / `MaxArmLength` | 150 / 700 cm | Zoom limits |
| `ZoomStep` | 50 cm | Distance per wheel notch |

### Input
Optional assets: `DefaultMappingContext`, `MoveAction`, `LookAction`, `JumpAction`, `SprintAction`, `InteractAction`, `ZoomAction`.
`EnsureDefaultInput()` creates a transient `UInputAction` for each one that is left empty. If no mapping context is assigned, it also builds one with the default bindings listed in the README. An assigned mapping context always takes precedence.

### Sprint and networking
- `StartSprint`/`StopSprint` set the speed locally (prediction), and `ServerSetSprinting` makes the server confirm it.
- `bIsSprinting` is replicated. `OnRep_IsSprinting` applies the speed on other machines.
- `CanSprint()` (BlueprintNativeEvent) is where stamina will hook in. Currently it only checks "not crouching".
- `ForceStopSprinting()` is for stamina depletion, stuns and similar effects.
- `OnSprintStateChanged(bool)` fires on every machine. The survival system will use it to start and stop stamina drain timers instead of polling.

## `ULIAnimInstance` (`Character/LIAnimInstance.h`)
The C++ parent for the Anim Blueprint. It exposes `Velocity`, `GroundSpeed`, `Direction`, `bShouldMove`, `bIsFalling` and `bIsSprinting`.
Recommended AnimGraph: a locomotion state machine with Idle, Walk/Run (a blendspace on GroundSpeed from 0 to 650), Jump, Fall and Land.
