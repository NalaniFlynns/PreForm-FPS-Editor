# PreForm FPS Editor (C++ / Qt6)

逆向分析 Formlabs PreForm 3.65 打印设置（`.fps`）的工具集，包含一个可读/写/编辑 `.fps`
的桌面编辑器，以及从 `PreForm.exe` 内嵌资源自动提取全部内置材料、并把 Form 4 材料参数
按实测锚点换算到 Form Studio 1 的转换功能。

## 功能

- 打开 / 编辑 / 保存 PreForm `.fps`（JSON 文本，结构校验后再写回）
- **自动定位 PreForm.exe 并提取全部内置材料**（224 份，跨 8 个机型），按内容 sha256 自动去重，
  并重建 `index.json` 素材库
- **任意参数可编辑**，包括尚未解出真名（`param_name` 为空）的混淆键；可一键写入
  `public_fields` 覆盖材料默认值
- **材料对比**：任选两种材料（可跨机型）逐参数对比，差异高亮，可导出 CSV
- **F4 → ST1 参数转换**：基于解码出的真实光强/曝光能量/静置/抬升/温度，换算目标物理参数，
  可导出 CSV 或写入当前 `.fps`
- 内置物理参数库（108 份真实解码记录 + 三锚点标定结果）

## 构建

需要 Qt 6（mingw_64 或任意桌面套件）与 C++17 编译器。

```
qmake PreFormFpsEditor.pro -spec win32-g++
mingw32-make -j4
```

生成 `release/PreFormFpsEditor.exe`。运行时把 `physics-library/` 放在 exe 同目录
（也可用内置的向上查找回退路径）。

## 自检（命令行）

```
PreFormFpsEditor.exe --selftest <src.fps> [out.fps]    # .fps 读写/校验
PreFormFpsEditor.exe --physselftest                    # 物理库 + 三锚点
PreFormFpsEditor.exe --extractselftest [PreForm.exe]  # 从 PreForm.exe 提取全部内置材料
```

## 目录

- `*.cpp` / `*.h` —— 源码
- `physics-library/` —— 解码后的真实物理参数（CSV/JSON），程序运行时读取

## 说明与边界

- `underlying_material` 是 PreForm 内部加密的 knob 数据，本工具不尝试离线解密，仅读写配置层
  与可辨识物理量。
- 除三个实测锚点（Flexible 80A / Tough 1500 V2 / Clear Cast）外，其余材料的能量折减系数为
  按材料族外推，真机使用前需打样校验。
- 本项目仅用于对自有设备与材料配置的研究与互操作，请遵守相关软件许可。
