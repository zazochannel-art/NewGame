# Copyright LOST ISLAND Team. All Rights Reserved.
#
# Creates and assembles the "Castaway" MetaHuman used as the survivor's body.
# Run (editor closed):
#   UnrealEditor-Cmd.exe C:\Dev\NewGame\LostIsland.uproject -run=pythonscript
#       -script="C:\Dev\NewGame\Tools\MetaHuman\make_castaway.py" -unattended -AllowCommandletRendering
#
# Auto-rigging and texture download are MetaHuman cloud services; they need an Epic account that has
# accepted the MetaHuman terms. Output: /Game/MetaHumans/Castaway/BP_Castaway (picked up by ALICharacter).

import unreal

PACKAGE_PATH = "/Game/MetaHumans"
ASSET_NAME = "Castaway"

subsystem = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
registry = unreal.AssetRegistryHelpers.get_asset_registry()


def log(message):
    unreal.log(f"[Castaway] {message}")


def find_or_create_character():
    path = f"{PACKAGE_PATH}/{ASSET_NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        log("reusing existing character asset")
        return unreal.load_asset(path)
    return asset_tools.create_asset(asset_name=ASSET_NAME, package_path=PACKAGE_PATH,
                                    asset_class=unreal.MetaHumanCharacter,
                                    factory=unreal.new_object(type=unreal.MetaHumanCharacterFactoryNew))


def add_wardrobe(character, slot, package_path, prefer=None):
    """Adds one wardrobe item from a folder to a slot and selects it (prefer = substring of the asset name)."""
    items = registry.get_assets(filter=unreal.ARFilter(
        package_paths=[package_path], recursive_paths=True,
        class_paths=[unreal.MetaHumanWardrobeItem.static_class().get_class_path_name()]))
    if not items:
        log(f"no wardrobe items in {package_path} for slot {slot}")
        return
    items = sorted(items, key=lambda a: str(a.asset_name))
    chosen = items[0]
    if prefer:
        for item in items:
            if prefer.lower() in str(item.asset_name).lower():
                chosen = item
                break
    key = character.internal_collection.try_add_item_from_wardrobe_item(slot_name=slot, wardrobe_item=chosen.get_asset())
    if key is None:
        log(f"could not add {chosen.asset_name} to {slot}")
        return
    character.internal_collection.default_instance.try_add_slot_selection(
        selection=unreal.MetaHumanPipelineSlotSelection(slot_name=slot, selected_item=key))
    log(f"{slot}: {chosen.asset_name}")


def style(character):
    # Sun-tanned skin with a few freckles.
    skin = unreal.MetaHumanCharacterSkinProperties()
    skin.u = 0.55
    skin.v = 0.55
    skin.roughness = 0.8
    freckles = unreal.MetaHumanCharacterFrecklesProperties()
    freckles.density = 0.25
    freckles.strength = 0.4
    settings = unreal.MetaHumanCharacterSkinSettings()
    settings.skin = skin
    settings.freckles = freckles
    subsystem.commit_skin_settings(character=character, skin_settings=settings)

    # Simple clothes and a scruffy castaway look.
    add_wardrobe(character, "Outfits", "/MetaHumanCharacter/Optional/Clothing")
    add_wardrobe(character, "Hair", "/MetaHumanCharacter/Optional/Grooms/Bindings/Hair", prefer="short")
    add_wardrobe(character, "Beard", "/MetaHumanCharacter/Optional/Grooms/Bindings/Beards", prefer="stubble")
    add_wardrobe(character, "Eyebrows", "/MetaHumanCharacter/Optional/Grooms/Bindings/Eyebrows")
    add_wardrobe(character, "Eyelashes", "/MetaHumanCharacter/Optional/Grooms/Bindings/Eyelashes")


def main():
    character = find_or_create_character()
    if character is None:
        unreal.log_error("[Castaway] could not create the MetaHuman Character asset")
        return
    if not subsystem.try_add_object_to_edit(character=character):
        unreal.log_error("[Castaway] could not open the character for editing")
        return
    try:
        style(character)

        rig = unreal.MetaHumanCharacterAutoRiggingRequestParams()
        rig.blocking = True
        rig.report_progress = False
        rig.rig_type = unreal.MetaHumanRigType.JOINTS_ONLY
        log("requesting auto-rigging (cloud)...")
        subsystem.request_auto_rigging(character=character, params=rig)

        textures = unreal.MetaHumanCharacterTextureRequestParams()
        textures.blocking = True
        textures.report_progress = False
        log("requesting textures (cloud)...")
        subsystem.request_texture_sources(character=character, params=textures)

        if not subsystem.can_build_meta_human(character=character):
            unreal.log_error("[Castaway] cannot build yet: auto-rigging or textures are missing "
                             "(sign in to the MetaHuman cloud services in the editor and accept the terms)")
            return

        build = unreal.MetaHumanCharacterEditorBuildParameters()
        build.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
        build.pipeline_quality = unreal.MetaHumanQualityLevel.MEDIUM
        build.absolute_build_path = PACKAGE_PATH
        build.common_folder_path = f"{PACKAGE_PATH}/Common"
        log("assembling...")
        subsystem.build_meta_human(character=character, params=build)
        unreal.EditorAssetLibrary.save_directory(PACKAGE_PATH, only_if_is_dirty=False, recursive=True)
        log("done")
    finally:
        subsystem.remove_object_to_edit(character=character)


main()
