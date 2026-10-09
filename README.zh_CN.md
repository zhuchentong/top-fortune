<p align="right">
  [English](README.md) · **简体中文**
</p>

<h1 align="center">上上签 · Top Fortune</h1>

<p align="center">
  FoloToy AI Passport 上的易经六十四卦灵签玩法。<br>
  摇动签筒，六十四卦中摇出一卦，静读卦辞。
</p>

## 这是什么

上上签把 AI Passport 变成一台掌上求签机。按 **OK**，手绘风签筒轻轻晃动；
再按一下（或稍候片刻），六十四卦中便摇出一卦——卦画、卦名与吉凶徽章
一目了然。再按 **OK** 展开解签：卦辞原文节引，配两三句白话解读。

全部功能在设备上离线运行——无需配网、无需联网账号。

## 怎么玩

| 界面 | 按键 | 效果 |
| --- | --- | --- |
| 待机页 | OK | 开始摇签 |
| 摇签中 | OK | 跳过动画立即出签（约 2.6 秒后也会自动揭晓） |
| 结果页 | OK | 进入解签 |
| 解签页 | UP / DOWN | 翻页阅读卦辞与白话解签 |
| 解签页 | OK（长按） | 返回待机，重新摇签 |

屏幕右上角有电量角标，每 30 秒刷新一次。

## 特色

- 六十四卦（文王卦序）全覆盖，吉凶四级（上上 / 上 / 中 / 下）以彩色徽章
  区分——鎏金、朱砂、棕、灰。
- 每卦配有卦辞原文节引与两到三句白话解签。
- 卦画按上下卦两柱并排绘制（上卦在左，与卦名读序一致），阴爻画为两段。
- 宣纸底、墨字、朱砂与鎏金的东方视觉；签筒为抗锯齿插画。
- 中文界面由生成的字库子集完整覆盖——宿主测试强制校验码点覆盖，
  不会出现缺字。

## 仓库结构

- `main/` —— 应用本体：`fortune_model.c`（六十四卦数据与抽取逻辑）、
  `fortune_ui.c`（四页 LVGL 界面）、`fortune_text.h`（全部界面文案）、
  `main.c`（入口）。
- `assets/fonts/` —— 字库子集流水线（`collect_chars.py`、
  `generate_fonts.sh`）与生成的 LVGL 子集；源字体为思源黑体
  Source Han Sans SC Regular 2.004R，SIL Open Font License 1.1 授权
  （见 `assets/fonts/LICENSE.txt`）。
- `assets/images/` —— 签筒插画生成器（`generate_images.py`）与生成的
  RGB565 图像。
- `tests/` —— 宿主测试：卦数据完整性、文案与字库覆盖、BSP 契约。
- `components/bsp/` —— 共享板级支持包（沿用上游，未改动）。
- `docs/` —— 上游产品与开发文档（[索引](docs/README.zh_CN.md)）；
  [AGENTS.md](AGENTS.zh_CN.md) 是 AI 协作入口。

## 构建与测试

目标硬件：ESP32-C3，8 MB Flash，ESP-IDF 5.5.3。

```bash
source "$HOME/esp/esp-idf-v5.5.3/export.sh"   # 激活工具链
./tools/validate.sh --static                  # 仓库检查 + 宿主测试
./tools/validate.sh --firmware                # 构建 + 合并镜像校验
./tools/validate.sh                           # 完整门禁
```

门禁会在 `build/firmware/<合并镜像哈希>/` 下生成内容寻址的交付包，内含
从 `0x0` 刷写的合并镜像（`FoloToy-AI-Passport-full.bin`）、ELF、MAP 与
清单。整个应用约占 8 MB Flash 中的 1.0 MB。

修改界面文案或卦签数据后，重新生成素材即可保持测试全绿：

```bash
python3 assets/fonts/collect_chars.py     # 刷新码点清单
./assets/fonts/generate_fonts.sh          # 重建 LVGL 字库子集（需要 node）
python3 assets/images/generate_images.py  # 重建签筒插画
```

## 许可

代码遵循上游仓库许可（[MIT](LICENSE)）。随仓库分发的思源黑体字体基于
SIL Open Font License 1.1 授权。
