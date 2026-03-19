<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>AI音乐频谱后处理实验工具（毕业设计）</title>
  <style>
    body { font-family: Arial, sans-serif; max-width: 700px; margin: 40px auto; padding: 20px; background: #f8f9fa; }
    h1 { text-align: center; color: #333; }
    .container { background: white; padding: 30px; border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.1); }
    input, button { display: block; margin: 20px auto; padding: 12px 24px; font-size: 16px; }
    button { background: #007bff; color: white; border: none; border-radius: 6px; cursor: pointer; }
    button:hover { background: #0056b3; }
    #status { text-align: center; color: #555; min-height: 24px; }
    #controls { text-align: center; margin-top: 30px; }
    audio { display: block; margin: 20px auto; width: 100%; }
  </style>
</head>
<body>
  <div class="container">
    <h1>AI生成音乐频谱后处理实验工具</h1>
    <p style="text-align:center; color:#666;">纯前端本地处理，仅用于学术研究对比实验</p>

    <input type="file" id="fileInput" accept="audio/wav,audio/mp3">
    <div id="status">请上传音频文件（WAV 或 MP3）</div>

    <div id="controls" style="display:none;">
      <button id="processBtn">开始处理</button>
      <audio id="preview" controls></audio>
      <button id="downloadBtn" style="margin-top:15px;">下载处理后 WAV</button>
    </div>
  </div>

  <script>
    // ========================
    // 核心处理函数（Web Audio API + OfflineAudioContext）
    // ========================
    const fileInput = document.getElementById('fileInput');
    const status = document.getElementById('status');
    const processBtn = document.getElementById('processBtn');
    const downloadBtn = document.getElementById('downloadBtn');
    const preview = document.getElementById('preview');
    let processedBuffer = null;

    fileInput.addEventListener('change', async (e) => {
      const file = e.target.files[0];
      if (!file) return;
      status.textContent = `已选择: ${file.name}，点击“开始处理”`;

      // 显示控件
      document.getElementById('controls').style.display = 'block';
    });

    processBtn.addEventListener('click', async () => {
      const file = fileInput.files[0];
      if (!file) {
        status.textContent = '请先上传音频文件';
        return;
      }

      status.textContent = '处理中...（请稍等，取决于文件长度）';

      try {
        // 1. 读取文件为 ArrayBuffer
        const arrayBuffer = await file.arrayBuffer();

        // 2. 创建 AudioContext（用于解码）
        const audioCtx = new (window.AudioContext || window.webkitAudioContext)();

        // 3. 解码成 AudioBuffer
        const originalBuffer = await audioCtx.decodeAudioData(arrayBuffer);

        // 4. 创建 OfflineAudioContext（离线渲染，速度快）
        const offlineCtx = new OfflineAudioContext(
          originalBuffer.numberOfChannels,
          originalBuffer.length,
          originalBuffer.sampleRate
        );

        // 5. 创建源节点
        const source = offlineCtx.createBufferSource();
        source.buffer = originalBuffer;

        // 6. 构建滤波链（所有处理在这里串联）
        let lastNode = source;

        // 6.1 250 Hz 峰值滤波 +0.6 dB, Q=1.0
        const lowPeak = offlineCtx.createBiquadFilter();
        lowPeak.type = 'peaking';
        lowPeak.frequency.value = 250;
        lowPeak.Q.value = 1.0;
        lowPeak.gain.value = 0.6;
        lastNode.connect(lowPeak);
        lastNode = lowPeak;

        // 6.2 3.2 kHz 峰值滤波 -1.2 dB, Q=0.9
        const midPeak = offlineCtx.createBiquadFilter();
        midPeak.type = 'peaking';
        midPeak.frequency.value = 3200;
        midPeak.Q.value = 0.9;
        midPeak.gain.value = -1.2;
        lastNode.connect(midPeak);
        lastNode = midPeak;

        // 6.3 10 kHz 以上 -0.8 dB 高搁架衰减
        const highShelf = offlineCtx.createBiquadFilter();
        highShelf.type = 'highshelf';
        highShelf.frequency.value = 10000;
        highShelf.gain.value = -0.8;
        lastNode.connect(highShelf);
        lastNode = highShelf;

        // 6.4 18 kHz 低通滤波（串联两个二阶 lowpass 近似 -24 dB/oct）
        const lp1 = offlineCtx.createBiquadFilter();
        lp1.type = 'lowpass';
        lp1.frequency.value = 18000;
        lp1.Q.value = 0.707; // Butterworth 风格
        lastNode.connect(lp1);

        const lp2 = offlineCtx.createBiquadFilter();
        lp2.type = 'lowpass';
        lp2.frequency.value = 18000;
        lp2.Q.value = 0.707;
        lp1.connect(lp2);
        lastNode = lp2;

        // 6.5 加极低白噪声 (-60 dB FS ≈ 0.001 振幅)
        const noiseGain = offlineCtx.createGain();
        noiseGain.gain.value = 0.001; // ≈ -60 dBFS

        // 生成白噪声缓冲（循环）
        const noiseBuffer = offlineCtx.createBuffer(1, offlineCtx.sampleRate * 2, offlineCtx.sampleRate);
        const noiseData = noiseBuffer.getChannelData(0);
        for (let i = 0; i < noiseData.length; i++) {
          noiseData[i] = Math.random() * 2 - 1;
        }
        const noiseSource = offlineCtx.createBufferSource();
        noiseSource.buffer = noiseBuffer;
        noiseSource.loop = true;
        noiseSource.connect(noiseGain);
        noiseGain.connect(offlineCtx.destination); // 噪声直接到输出（混入主链）
        noiseSource.start(0);

        // 6.6 主链连接到 destination
        lastNode.connect(offlineCtx.destination);

        // 7. 开始离线渲染
        source.start(0);
        const renderedBuffer = await offlineCtx.startRendering();

        // 8. 保存处理后 Buffer
        processedBuffer = renderedBuffer;

        // 9. 预览
        const blob = await bufferToWaveBlob(renderedBuffer);
        preview.src = URL.createObjectURL(blob);
        status.textContent = '处理完成！可试听或下载';

        // 10. 下载按钮
        downloadBtn.onclick = () => {
          const url = URL.createObjectURL(blob);
          const a = document.createElement('a');
          a.href = url;
          a.download = 'processed_' + file.name.replace(/\.[^/.]+$/, "") + '.wav';
          a.click();
          URL.revokeObjectURL(url);
        };
      } catch (err) {
        status.textContent = '处理失败：' + err.message;
        console.error(err);
      }
    });

    // ========================
    // WAV 编码函数（AudioBuffer → WAV Blob）
    // ========================
    async function bufferToWaveBlob(buffer) {
      const numChannels = buffer.numberOfChannels;
      const sampleRate = buffer.sampleRate;
      const length = buffer.length * numChannels * 2 + 44; // 44字节头
      const arrayBuffer = new ArrayBuffer(length);
      const view = new DataView(arrayBuffer);

      // RIFF 头
      writeString(view, 0, 'RIFF');
      view.setUint32(4, 36 + buffer.length * numChannels * 2, true);
      writeString(view, 8, 'WAVE');
      // fmt 子块
      writeString(view, 12, 'fmt ');
      view.setUint32(16, 16, true);
      view.setUint16(20, 1, true); // PCM
      view.setUint16(22, numChannels, true);
      view.setUint32(24, sampleRate, true);
      view.setUint32(28, sampleRate * numChannels * 2, true);
      view.setUint16(32, numChannels * 2, true);
      view.setUint16(34, 16, true); // 16位
      // data 子块
      writeString(view, 36, 'data');
      view.setUint32(40, buffer.length * numChannels * 2, true);

      // 写 PCM 数据（交错）
      let offset = 44;
      for (let i = 0; i < buffer.length; i++) {
        for (let channel = 0; channel < numChannels; channel++) {
          const sample = Math.max(-1, Math.min(1, buffer.getChannelData(channel)[i]));
          view.setInt16(offset, sample < 0 ? sample * 0x8000 : sample * 0x7FFF, true);
          offset += 2;
        }
      }

      return new Blob([arrayBuffer], { type: 'audio/wav' });
    }

    function writeString(view, offset, string) {
      for (let i = 0; i < string.length; i++) {
        view.setUint8(offset + i, string.charCodeAt(i));
      }
    }
  </script>
</body>
</html>
