# JUCE Desktop Timbre Adapter

这是一个基于 **JUCE + C++** 的桌面软件原型，用来把 1–15 秒音频的音色特征快速映射成一组可在 FL Studio / Sytrus 风格界面里继续微调的参数。

## 已实现功能

- 加载本地音频文件（建议 1–15 秒）
- 播放 / 停止试听
- 本地快速分析音色特征：
  - RMS
  - Zero Crossing Rate
  - Attack Time
  - Stereo Width
  - Spectral Centroid
  - Spectral Rolloff
  - Spectral Flatness
  - Peakiness
  - Dominant Pitch
- 把分析结果映射到一组合成器参数：
  - Cutoff
  - Resonance
  - Brightness
  - Drive
  - Unison
  - Detune
  - Harmonics
  - Noise
  - Stereo
  - FMDepth
  - ADSR
- 显示一个 JUCE 版的合成器风格界面
- 键盘预览主峰音高及其泛音示意
- 导出参数快照 JSON

## 构建方式

### 方式 1：使用 CMake + FetchContent

```bash
cmake -S . -B build
cmake --build build --config Release
```

### 方式 2：已安装 JUCE

如果你已经本地安装或维护了 JUCE，也可以把 `CMakeLists.txt` 改成 `add_subdirectory(JUCE)` 的方式。

## 运行说明

- 推荐上传 **单音色片段**，例如 lead、pad、pluck、bass、单独采样
- 音频越干净，映射结果越稳定
- 当前版本是 **启发式音色映射器**，不是 1:1 的预设反编译器

## 目录结构

- `CMakeLists.txt`
- `Source/Main.cpp`
- `Source/MainComponent.h`
- `Source/MainComponent.cpp`

## 后续建议

下一步最值得继续做的是：

1. 加入更细的波形类型建议（Saw / Square / Sine / Noise / FM）
2. 分离 OP1 / OP2 / Filter / FX 页面
3. 增加频谱和包络可视化
4. 增加小模型分类器，辅助判断更像 lead / pad / pluck / bass
