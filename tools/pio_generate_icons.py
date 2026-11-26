# PlatformIO pre-build script: regenerates src/weather/weather_icons.{c,h} from
# assets/weather-icons/*.bmp when the icons or the converter are newer than the output.
# The generated files are git-ignored (they are ~4.6 MB of C source).
Import("env")  # noqa: F821  (provided by PlatformIO/SCons)

import glob
import os
import subprocess

root = env.subst("$PROJECT_DIR")  # noqa: F821
converter = os.path.join(root, "tools", "convert_icons.py")
outputs = [os.path.join(root, "src", "weather", n) for n in ("weather_icons.c", "weather_icons.h")]
inputs = glob.glob(os.path.join(root, "assets", "weather-icons", "*", "*.bmp")) + [converter]

newest_input = max(os.path.getmtime(p) for p in inputs)
if not all(os.path.exists(p) for p in outputs) or min(os.path.getmtime(p) for p in outputs) < newest_input:
    print("Generating weather icons...")
    subprocess.check_call([env.subst("$PYTHONEXE"), converter])  # noqa: F821
