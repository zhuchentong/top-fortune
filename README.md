<p align="right">
  **English** · [简体中文](README.zh_CN.md)
</p>

<h1 align="center">Top Fortune</h1>

<p align="center">
  An I Ching fortune-drawing play for the FoloToy AI Passport.<br>
  Shake the tube, draw one of all 64 hexagrams, read the oracle.
</p>

## What it is

Top Fortune turns the AI Passport into a pocket oracle. Press **OK** and the
hand-drawn bamboo tube sways; press again (or wait a moment) and one of the
sixty-four hexagrams is drawn — the six-line figure, its name, and a colored
fortune badge. Press **OK** once more to open the reading: the excerpted
oracle text plus a short plain-language interpretation.

Everything runs fully offline on the device — no provisioning, no network,
no accounts.

## How to play

| Screen | Keys | Effect |
| --- | --- | --- |
| Idle | OK | Start shaking the tube |
| Shaking | OK | Skip the animation and reveal at once (auto-reveals in ~2.6 s) |
| Result | OK | Open the reading |
| Reading | UP / DOWN | Page through the oracle text and interpretation |
| Reading | OK (long press) | Return to idle and draw again |

A small battery gauge sits in the top-right corner and refreshes every 30 s.

## Features

- All 64 hexagrams in King Wen order, with four fortune levels
  shown as colored badges — gold, cinnabar, tan, slate.
- Every hexagram carries an excerpt of the original oracle text and a two- to
  three-sentence plain-language reading.
- The hexagram figure is drawn as two trigram columns (upper trigram on the
  left, matching the reading order of the name), yin lines rendered as two
  segments.
- Paper-and-ink oriental visual language: rice-paper background, ink text,
  cinnabar and gilded accents; the tube is an anti-aliased illustration.
- Chinese UI text is covered by generated font subsets — the coverage is
  enforced by host tests, so no missing glyphs.

## Repository layout

- `main/` — the application: `fortune_model.c` (hexagram data + draw logic),
  `fortune_ui.c` (four-page LVGL UI), `fortune_text.h` (all UI strings),
  `main.c` (entry).
- `assets/fonts/` — font-subset pipeline (`collect_chars.py`,
  `generate_fonts.sh`) and the generated LVGL subsets; source font is
  Source Han Sans SC Regular 2.004R under the SIL Open Font License 1.1
  (see `assets/fonts/LICENSE.txt`).
- `assets/images/` — the tube artwork generator (`generate_images.py`) and
  the generated RGB565 image.
- `tests/` — host tests: hexagram data integrity, string/font coverage,
  BSP contracts.
- `components/bsp/` — the shared board support package (unchanged upstream).
- `docs/` — the upstream product and development documentation
  ([index](docs/README.md)); [AGENTS.md](AGENTS.md) is the AI-collaboration
  entry point.

## Build and test

Target: ESP32-C3, 8 MB Flash, ESP-IDF 5.5.3.

```bash
source "$HOME/esp/esp-idf-v5.5.3/export.sh"   # activate the toolchain
./tools/validate.sh --static                  # repository checks + host tests
./tools/validate.sh --firmware                # build + merged-image verify
./tools/validate.sh                           # the complete gate
```

The gate produces a content-addressed bundle under
`build/firmware/<full-bin-sha256>/` containing the merged image
(`FoloToy-AI-Passport-full.bin`, flashable from `0x0`), the ELF, the map, and
a manifest. The complete application fits in about 1.0 MB of the 8 MB flash.

After editing any UI text or hexagram data, regenerate the assets and their
tests stay green:

```bash
python3 assets/fonts/collect_chars.py     # refresh codepoint inventories
./assets/fonts/generate_fonts.sh          # rebuild LVGL font subsets (needs node)
python3 assets/images/generate_images.py  # rebuild the tube artwork
```

## License

Code follows the upstream repository license ([MIT](LICENSE)). The bundled
Source Han Sans SC font is distributed under the SIL Open Font License 1.1.
