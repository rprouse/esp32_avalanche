# Weather icons

Weather, moon-phase and wind-direction bitmaps from Bodmer's
[OpenWeather](https://github.com/Bodmer/OpenWeather) library
(`examples/TFT_eSPI_OpenWeather_LittleFS/data`), used under the FreeBSD license in
[LICENSE.txt](LICENSE.txt).

| Folder | Size | Use |
|---|---|---|
| `icon/` | 100x100 | current conditions |
| `icon50/` | 50x50 | daily forecast rows |
| `moon/` | 60x60 | moon phase (`moonphase_L0..23`) |
| `wind/` | 50x50 | wind direction compass |

These are 24-bit BMPs on a black background. `tools/convert_icons.py` turns them into
LVGL images (`src/weather/weather_icons.c`), deriving transparency from brightness so
they blend onto the dark UI. Re-run it after changing any icon:

    python tools/convert_icons.py
