MzPCD @VERSION@ - a pattern-controlled distortion for Windows (64-bit)
https://github.com/simozzer/MzPCD

The easiest way to install is MzPCD-@VERSION@-win64-Setup.exe from the releases page,
which also adds an uninstaller. This zip is for installing by hand.

VST3
  Copy the MzPCD.vst3 folder into either
    %LOCALAPPDATA%\Programs\Common\VST3        (just you - no admin needed)
    C:\Program Files\Common Files\VST3         (everyone - needs admin)
  then rescan plugins in your DAW. If it does not appear, add the first folder
  to the DAW's VST3 plugin paths.

Windows may warn that the plugin is from an unknown publisher: the binaries are
not code-signed. Choose "More info" then "Run anyway", or build from source.

Copyright (C) 2026 Simon Moscrop.
Free software under the GNU AGPL v3 - see LICENSE. Source code:
https://github.com/simozzer/MzPCD/tree/v@VERSION@
