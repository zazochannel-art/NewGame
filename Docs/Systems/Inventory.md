# Items and inventory

## Items
- **`ULIItemDefinition`** (`Items/LIItemDefinition.h`) is a `UPrimaryDataAsset` with `ItemId`, `DisplayName`, `Description`, `Category`, `MaxStackSize` and `Icon`.
  Its primary asset type is `LIItem`. The Asset Manager scans `/Game/Items` (see `Config/DefaultGame.ini`).
- **`ULIItemRegistrySubsystem`** (GameInstance subsystem, on server and clients) maps `ItemId` to its definition.
  At startup it registers 3 built-in items so the prototype runs with no assets: **Wood** (stack 50), **Stone** (40), **Fiber** (80).
  A Data Asset with the same `ItemId` overrides the built-in item.
- Item IDs are `FName`. Constants: `LIItems::Wood / Stone / Fiber` (`Items/LIItemTypes.h`).

To add an item without code: Content Browser → `/Game/Items` → Miscellaneous → Data Asset → `LIItemDefinition`, then fill in `ItemId`.

## `ULIInventoryComponent` (`Inventory/LIInventoryComponent.h`)
A fixed-size inventory (`DefaultSlotCount` = 20). Each slot is an `FLIInventorySlot { ItemId, Quantity }`, limited to the item's `MaxStackSize`.

| Function | Where | Description |
|---|---|---|
| `GetSlots()`, `GetItemCount(Id)`, `HasItem(Id, Qty)`, `GetRoomFor(Id)` | server + owner | Queries |
| `AddItem(Id, Qty)` | server | Tops up existing stacks first, then fills empty slots. **Returns how many were added** (the rest didn't fit) |
| `RemoveItem(Id, Qty)` | server | All or nothing; takes from the last stacks first |
| `ClearInventory()` | server | |
| `SetSlots(Array)` | server | For SaveGame. Unknown items are dropped and stacks are clamped |
| `SetSlotCount(N)` | server | For future expansion (backpacks) |

- `Slots` replicates only to the owner (`COND_OwnerOnly`). `OnInventoryChanged` fires on the server and on the owning client.
- Pickup and "Inventory full" messages: the server sends an unreliable Client RPC, and the HUD shows them for 2.5 s.

## Basic UI
- **Tab** (gamepad: Menu) toggles the inventory panel, a 5x4 grid with name and quantity per slot (`ALIHUD`, canvas).
- Gathering messages (`+3 Wood`, `Inventory full`) appear under the crosshair.
- A UMG version (drag & drop, icons) will replace the canvas when crafting arrives.

## Debug commands
```
LIGive Wood 25
LIClearInventory
```
