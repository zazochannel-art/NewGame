# Raft: escaping the island

Milestone 16 gives a run its goal.

- **Raft Kit**: 60 Wood, 40 Fiber, 6 Scrap Metal (crafting page 2). The scrap competes with the metal tools and flasks, so escaping early means fewer comforts.
- **Placement** (`ALIRaft::ComputePlacement`, used by `ULICraftingComponent::PlaceItem`): 3.5 m in front along the camera direction, where the seabed is 30-250 cm deep. The raft floats at sea level (Z = 0).
- **Sail away (E)**: `ALIGameState::SetEscaped` (server) replicates `bEscaped` and the day. The HUD replaces everything with the ending. After `EscapeEndDelay` (15 s) the server calls `ULISaveSubsystem::NewGameAndRestart`, which deletes the save and starts a new random island.
- Saved with the other structures; `LIRegenerateIsland` removes it.
- Debug: `LIGoToShore` (stand at the water's edge facing the sea), `LIEscape` (trigger the ending).

Follow-ups: an actual sailing sequence, a score screen (days, items crafted), and a rescue-signal alternative ending.
