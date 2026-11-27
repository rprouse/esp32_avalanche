# PlatformIO pre-script: over-the-air upload support (issue #3).
#
#   pio run -t upload --upload-port backcountry.local     (or the device's IP address)
#
# When the upload port is a network address (IP or *.local), switch to the espota
# uploader and pass OTA_PASSWORD from include/config.h, so the password lives only in
# the git-ignored config. A serial port (COM5, /dev/ttyUSB0) or no port is left alone.
Import("env")  # noqa: F821  (provided by PlatformIO/SCons)

import os
import re
import sys

port = env.subst("$UPLOAD_PORT").strip('"')  # noqa: F821
if re.match(r"^((\d{1,3}\.){3}\d{1,3}|[^\\/]+\.local|[0-9a-fA-F:]*:[0-9a-fA-F:]+)$", port):
    config = os.path.join(env.subst("$PROJECT_DIR"), "include", "config.h")  # noqa: F821
    password = ""
    if os.path.exists(config):
        with open(config, encoding="utf-8") as f:
            text = f.read()
        # Last uncommented definition wins, like the preprocessor would see it.
        for m in re.finditer(r'^[ \t]*#\s*define\s+OTA_PASSWORD\s+"((?:[^"\\]|\\.)*)"', text, re.M):
            password = re.sub(r"\\(.)", r"\1", m.group(1))
    if not password:
        sys.stderr.write(
            "Error: OTA upload to %s needs OTA_PASSWORD in include/config.h "
            "(and firmware built with it, flashed once over USB).\n" % port
        )
        env.Exit(1)  # noqa: F821
    env.Replace(UPLOAD_PROTOCOL="espota")  # noqa: F821
    # Prepended to the espota command line by PlatformIO after the platform's builder runs.
    env.Append(UPLOAD_FLAGS=["--auth=" + password, "--timeout=30"])  # noqa: F821
