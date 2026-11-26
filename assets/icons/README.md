# UI icons

| File | Source | License |
|---|---|---|
| `terrain.svg` | Google [Material Icons](https://github.com/google/material-design-icons) "terrain" (filled), `src/maps/terrain/materialicons/24px.svg` | Apache 2.0, see [LICENSE-material-icons.txt](LICENSE-material-icons.txt) |

`tools/make_logo.py` rasterizes `terrain.svg` (mirrored, so the lower peak is on the right) into an
alpha-only LVGL image, `src/ui/logo_icon.c`, which the UI tints white. Re-run it after changing the SVG:

    python tools/make_logo.py
