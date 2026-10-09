# PS5 mod folders

UnleashedRecomp can load supported HMM/UMM file mods from the user-data directory through its existing virtual file overlay. The normal PS5 location is:

```text
/data/UnleashedRecomp/mods/<FolderName>/
```

Each mod folder must contain a supported `mod.ini` and its included game files. Drop the whole mod folder there; do not copy mod files over the base game.

## Enable state

On startup, supported folders are listed in `/data/UnleashedRecomp/mods.json`. Folder names are the IDs, and a new mod defaults to enabled:

```json
{
    "version": 1,
    "mods": {
        "Example Mod": "on",
        "Another Mod": "off"
    }
}
```

Set a value to `"off"` to disable that mod, or `"on"` to enable it again. The loader also accepts JSON booleans (`true`/`false`). Changes take effect the next time the game starts. Existing state is retained for folders that are temporarily absent; unsupported folders are not loaded. The scanner ignores `mods/bak` and does not treat `mods.json` as a mod.

Enabled folder mods are considered alphabetically by folder name. For ordinary file replacements, the first enabled mod that provides a path wins. The folder scan does not require `cpkredir.ini`. If a legacy `cpkredir.ini` and referenced HMM/UMM database are present, those entries continue through the existing loader; folder mods are checked first and their enable states remain controlled by `mods.json`.

## Safety and loading status

The game files are not copied, overwritten, or backed up. Mod files are resolved by the existing virtual overlay; turning a mod off removes it from that overlay on the next launch. The loader shows a translated detected-mod count while the initial game loading screen is active, then dismisses it when that first load completes.

Only file mods understood by UnleashedRecomp's current HMM/UMM `mod.ini` parser are supported. Code mods and other formats are not added by this folder scanner. No PS5 build or console verification has been performed for this change.
