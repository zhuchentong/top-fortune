#!/usr/bin/env bash
# 生成上上签的中文子集字库（app_font_48 / app_font_24）。
#
# 输入：assets/fonts/fortune_chars.txt（码点清单，由 collect_chars.py 从
# main/ 源码字符串自动生成——tests/test_fortune_text.c 会校验覆盖）+ ASCII。
# 工具：lv_font_conv 固定版本 1.5.3（经 npx 拉取，需要 node）。
# 字体：Source Han Sans SC Regular 2.004R（OFL 1.1，见本目录 LICENSE.txt）。
#
# 用法：仓库根目录下 ./assets/fonts/generate_fonts.sh
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
font="${repo_root}/assets/fonts/SourceHanSansSC-Regular.otf"
converter="lv_font_conv@1.5.3"

# 码点清单 → --range 参数（ASCII 区段 + 清单中逐个码点）。
make_range() {
    local inventory="$1"
    local range="0x20-0x7E"
    while IFS= read -r line; do
        line="${line%%#*}"
        line="$(echo "$line" | tr -d '[:space:]')"
        [[ -z "$line" ]] && continue
        range="${range},$(echo "$line" | tr 'A-F' 'a-f')"
    done < "$inventory"
    echo "$range"
}

cd "$repo_root"
# 48px 只渲染标题与卦名（小清单 fortune_chars_48.txt）；
# 24px 渲染全部正文（全量清单 fortune_chars.txt）。
npx --yes "$converter" \
    --font "$font" \
    --range "$(make_range "${repo_root}/assets/fonts/fortune_chars_48.txt")" \
    --size 48 --bpp 4 --format lvgl --no-compress \
    --lv-font-name "app_font_48" --lv-include lvgl.h \
    --output "assets/fonts/app_font_48.c"
npx --yes "$converter" \
    --font "$font" \
    --range "$(make_range "${repo_root}/assets/fonts/fortune_chars.txt")" \
    --size 24 --bpp 4 --format lvgl --no-compress \
    --lv-font-name "app_font_24" --lv-include lvgl.h \
    --output "assets/fonts/app_font_24.c"

echo "Generated: assets/fonts/app_font_48.c assets/fonts/app_font_24.c"
