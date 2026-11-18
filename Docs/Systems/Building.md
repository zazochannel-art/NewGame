# Building

Milestone 12. Wooden shelter pieces on a 300 cm grid (`ALIBuildingPiece`, `World/LIBuildingPiece.h`).

| Piece | Recipe | Size | Snaps to |
|---|---|---|---|
| Wooden Floor | 8 Wood | 300 x 300 x 20 cm | the ground (first floor: flat-ish, above sea level), or the grid of the nearest floor |
| Wooden Wall | 6 Wood + 2 Fiber | 20 x 300 x 300 cm | the floor edge nearest to the target point, on top of the floor |
| Wooden Roof | 6 Wood + 4 Fiber | 320 x 320 x 15 cm | one wall height (300 cm) above a floor cell |

Placement (`ALIBuildingPiece::ComputePlacement`, server): the target point is 2.4 m in front of the player along the **camera** direction. The nearest floor within 6 m anchors the grid; the target is rounded to that grid. Duplicate pieces on the same spot are refused. Walls and roofs need a floor.

Shelter: standing under a roof (within two wall heights below it) means no night cold (`ALIBuildingPiece::IsSheltered`, used by `ULISurvivalComponent`).

G now places the most recently crafted placeable first (`ULICraftingComponent::PreferredPlaceable`), so building walls doesn't place the campfire.

Saved as structures (class + transform). `LIRegenerateIsland` removes them.

Lighting change in this milestone: the island sky light intensity is 8, so shadowed sides of buildings are not black.

Follow-ups: placement preview (ghost), doorways and windows, deconstruct/pick up, structural support (roofs currently float if the walls are missing), stone tier.
