import os
import unreal

project = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
task = unreal.AssetImportTask()
task.set_editor_property("filename", os.path.join(project, "SourceAudio", "window_screamer.mp3"))
task.set_editor_property("destination_path", "/Game/Audio")
task.set_editor_property("destination_name", "WindowScreamer")
task.set_editor_property("automated", True)
task.set_editor_property("replace_existing", True)
task.set_editor_property("save", True)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
sound = unreal.load_asset("/Game/Audio/WindowScreamer")
if not isinstance(sound, unreal.SoundWave):
    raise RuntimeError("The supplied MP3 could not be imported as a SoundWave")
sound.set_editor_property("looping", False)
sound.set_sound_asset_compression_type(unreal.SoundAssetCompressionType.PCM)
sound.set_editor_property("loading_behavior", unreal.SoundWaveLoadingBehavior.FORCE_INLINE)
unreal.EditorAssetLibrary.save_loaded_asset(sound)
unreal.log("METAL_AUDIO_IMPORTED: duration=" + str(sound.get_editor_property("duration")))
