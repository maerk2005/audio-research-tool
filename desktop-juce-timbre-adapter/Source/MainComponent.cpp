#include "MainComponent.h"

namespace
{
    static juce::Colour bgColour()        { return juce::Colour::fromRGB (43, 23, 51); }
    static juce::Colour bg2Colour()       { return juce::Colour::fromRGB (58, 31, 70); }
    static juce::Colour panelColour()     { return juce::Colour::fromRGB (36, 23, 34); }
    static juce::Colour panel2Colour()    { return juce::Colour::fromRGB (25, 19, 23); }
    static juce::Colour lineColour()      { return juce::Colour::fromRGB (77, 58, 70); }
    static juce::Colour accentColour()    { return juce::Colour::fromRGB (246, 165, 27); }
    static juce::Colour textColour()      { return juce::Colour::fromRGB (245, 242, 238); }
    static juce::Colour mutedColour()     { return juce::Colour::fromRGB (203, 188, 196); }

    template <typename T>
    T clampValue (T v, T lo, T hi) { return std::max (lo, std::min (hi, v)); }

    double mapRange (double v, double inMin, double inMax, double outMin, double outMax)
    {
        const auto t = clampValue ((v - inMin) / (inMax - inMin + 1.0e-12), 0.0, 1.0);
        return outMin + t * (outMax - outMin);
    }

    double computeRMS (const juce::AudioBuffer<float>& buffer)
    {
        double sum = 0.0;
        const auto* data = buffer.getReadPointer (0);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            sum += data[i] * data[i];
        return std::sqrt (sum / juce::jmax (1, buffer.getNumSamples()));
    }

    double computeZCR (const juce::AudioBuffer<float>& buffer)
    {
        const auto* data = buffer.getReadPointer (0);
        int changes = 0;
        for (int i = 1; i < buffer.getNumSamples(); ++i)
            if ((data[i - 1] >= 0.0f) != (data[i] >= 0.0f))
                ++changes;
        return (double) changes / juce::jmax (1, buffer.getNumSamples());
    }

    juce::Array<float> computeEnvelope (const juce::AudioBuffer<float>& buffer, int hop)
    {
        juce::Array<float> env;
        const auto* data = buffer.getReadPointer (0);
        for (int start = 0; start < buffer.getNumSamples(); start += hop)
        {
            float peak = 0.0f;
            for (int i = start; i < juce::jmin (start + hop, buffer.getNumSamples()); ++i)
                peak = juce::jmax (peak, std::abs (data[i]));
            env.add (peak);
        }
        float maxV = 1.0e-6f;
        for (auto v : env) maxV = juce::jmax (maxV, v);
        for (int i = 0; i < env.size(); ++i) env.set (i, env[i] / maxV);
        return env;
    }

    double estimateAttack (const juce::Array<float>& env, double sampleRate, int hop)
    {
        int idx10 = -1, idx90 = -1;
        for (int i = 0; i < env.size(); ++i)
        {
            if (idx10 < 0 && env[i] >= 0.1f) idx10 = i;
            if (idx90 < 0 && env[i] >= 0.9f) { idx90 = i; break; }
        }
        if (idx10 < 0 || idx90 < 0) return 0.02;
        return juce::jmax (0.005, (idx90 - idx10) * (double) hop / sampleRate);
    }

    juce::String estimateEnvelopeType (const juce::Array<float>& env)
    {
        if (env.isEmpty()) return "通用";
        const int peakIndex = env.indexOf (*std::max_element (env.begin(), env.end()));
        float sustain = 0.0f;
        const int s0 = (int) (env.size() * 0.45f);
        const int s1 = (int) (env.size() * 0.75f);
        int count = 0;
        for (int i = s0; i < s1 && i < env.size(); ++i) { sustain += env[i]; ++count; }
        sustain = count > 0 ? sustain / count : 0.5f;
        if (peakIndex < env.size() * 0.08 && sustain < 0.35f) return "Pluck / Perc";
        if (peakIndex < env.size() * 0.12 && sustain > 0.55f) return "Lead / Key";
        if (peakIndex > env.size() * 0.18 && sustain > 0.45f) return "Pad / Swell";
        if (sustain > 0.7f) return "Organ / Sustained";
        return "通用";
    }

    double computeStereoWidth (const juce::AudioBuffer<float>& original)
    {
        if (original.getNumChannels() < 2) return 0.0;
        const auto* l = original.getReadPointer (0);
        const auto* r = original.getReadPointer (1);
        double sumLR = 0.0, sumL2 = 0.0, sumR2 = 0.0;
        const int step = juce::jmax (1, original.getNumSamples() / 20000);
        for (int i = 0; i < original.getNumSamples(); i += step)
        {
            sumLR += l[i] * r[i];
            sumL2 += l[i] * l[i];
            sumR2 += r[i] * r[i];
        }
        const auto corr = sumLR / std::sqrt (sumL2 * sumR2 + 1.0e-12);
        return clampValue ((1.0 - corr) * 0.5, 0.0, 1.0);
    }

    struct SpectrumSummary
    {
        double centroid = 0.0;
        double rolloff = 0.0;
        double flatness = 0.0;
        double peakiness = 0.0;
        double highRatio = 0.0;
        double motion = 0.0;
    };

    SpectrumSummary analyseSpectrum (const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        constexpr int fftOrder = 11;
        constexpr int fftSize = 1 << fftOrder;
        constexpr int hop = 1024;
        juce::dsp::FFT fft (fftOrder);
        juce::dsp::WindowingFunction<float> window (fftSize, juce::dsp::WindowingFunction<float>::hann, false);
        std::vector<float> fftData ((size_t) fftSize * 2, 0.0f);
        const auto* data = buffer.getReadPointer (0);

        double centroidSum = 0.0, rolloffSum = 0.0, flatSum = 0.0, peakSum = 0.0, motion = 0.0;
        double lowE = 0.0, highE = 0.0;
        double prevCentroid = -1.0;
        int frames = 0;

        for (int start = 0; start + fftSize < buffer.getNumSamples(); start += hop)
        {
            std::fill (fftData.begin(), fftData.end(), 0.0f);
            std::copy (data + start, data + start + fftSize, fftData.begin());
            window.multiplyWithWindowingTable (fftData.data(), fftSize);
            fft.performRealOnlyForwardTransform (fftData.data());

            double magSum = 0.0, weighted = 0.0, totalEnergy = 0.0;
            double maxMag = 1.0e-12, logSum = 0.0;
            int valid = 0;
            std::vector<double> mags (fftSize / 2, 0.0);

            for (int i = 1; i < fftSize / 2; ++i)
            {
                const auto re = fftData[(size_t) i * 2];
                const auto im = fftData[(size_t) i * 2 + 1];
                const auto mag = std::sqrt ((double) re * re + (double) im * im);
                const auto freq = (double) i * sampleRate / fftSize;
                mags[(size_t) i] = mag;
                magSum += mag;
                weighted += mag * freq;
                totalEnergy += mag;
                maxMag = juce::jmax (maxMag, mag);
                if (mag > 1.0e-12) { logSum += std::log (mag); ++valid; }
                if (freq < 800.0) lowE += mag;
                if (freq > 3000.0) highE += mag;
            }

            const auto centroid = magSum > 0.0 ? weighted / magSum : 0.0;
            centroidSum += centroid;
            if (prevCentroid >= 0.0) motion += std::abs (centroid - prevCentroid);
            prevCentroid = centroid;

            double cumulative = 0.0;
            const auto target = totalEnergy * 0.85;
            double rolloff = 0.0;
            for (int i = 1; i < fftSize / 2; ++i)
            {
                cumulative += mags[(size_t) i];
                if (cumulative >= target) { rolloff = (double) i * sampleRate / fftSize; break; }
            }
            rolloffSum += rolloff;

            const auto flatness = valid > 0 && magSum > 0.0 ? std::exp (logSum / valid) / (magSum / valid) : 0.0;
            flatSum += flatness;
            peakSum += maxMag / ((magSum / juce::jmax (1, valid)) + 1.0e-12);
            ++frames;
        }

        SpectrumSummary s;
        s.centroid = frames > 0 ? centroidSum / frames : 0.0;
        s.rolloff  = frames > 0 ? rolloffSum / frames : 0.0;
        s.flatness = frames > 0 ? flatSum / frames : 0.0;
        s.peakiness = frames > 0 ? peakSum / frames : 0.0;
        s.highRatio = highE / (lowE + highE + 1.0e-12);
        s.motion = frames > 1 ? motion / (frames - 1) : 0.0;
        return s;
    }

    std::pair<int, double> detectDominantPitch (const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        constexpr int fftOrder = 12;
        constexpr int fftSize = 1 << fftOrder;
        juce::dsp::FFT fft (fftOrder);
        juce::dsp::WindowingFunction<float> window (fftSize, juce::dsp::WindowingFunction<float>::hann, false);
        std::vector<float> fftData ((size_t) fftSize * 2, 0.0f);
        const int start = juce::jmax (0, (buffer.getNumSamples() - fftSize) / 2);
        const auto* data = buffer.getReadPointer (0);
        std::copy (data + start, data + start + juce::jmin (fftSize, buffer.getNumSamples() - start), fftData.begin());
        window.multiplyWithWindowingTable (fftData.data(), fftSize);
        fft.performRealOnlyForwardTransform (fftData.data());

        int bestIdx = 1;
        double bestMag = 0.0;
        for (int i = 1; i < fftSize / 2; ++i)
        {
            const auto freq = (double) i * sampleRate / fftSize;
            if (freq < 50.0 || freq > 3000.0) continue;
            const auto re = fftData[(size_t) i * 2];
            const auto im = fftData[(size_t) i * 2 + 1];
            const auto mag = std::sqrt ((double) re * re + (double) im * im);
            if (mag > bestMag) { bestMag = mag; bestIdx = i; }
        }

        const auto freq = (double) bestIdx * sampleRate / fftSize;
        const auto midi = (int) std::round (69.0 + 12.0 * std::log2 (freq / 440.0));
        return { midi, freq };
    }
}

RotaryParam::RotaryParam (const juce::String& name, const juce::String& suffixToUse) : suffix (suffixToUse)
{
    title.setText (name, juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centred);
    title.setColour (juce::Label::textColourId, textColour());
    addAndMakeVisible (title);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange (0.0, 100.0, 1.0);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accentColour());
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, lineColour());
    slider.setColour (juce::Slider::thumbColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (slider);

    valueLabel.setJustificationType (juce::Justification::centred);
    valueLabel.setColour (juce::Label::textColourId, juce::Colour::fromRGB (255, 227, 155));
    addAndMakeVisible (valueLabel);
}

void RotaryParam::resized()
{
    auto area = getLocalBounds();
    title.setBounds (area.removeFromTop (18));
    valueLabel.setBounds (area.removeFromBottom (18));
    slider.setBounds (area.reduced (2));
}

void RotaryParam::paint (juce::Graphics& g)
{
    g.setColour (lineColour());
    g.drawRoundedRectangle (getLocalBounds().toFloat(), 8.0f, 1.0f);
}

void RotaryParam::setValue (double value)
{
    slider.setValue (value, juce::dontSendNotification);
    valueLabel.setText (juce::String ((int) std::round (value)) + suffix, juce::dontSendNotification);
}

double RotaryParam::getValue() const { return slider.getValue(); }
juce::Slider& RotaryParam::getSlider() { return slider; }

FaderParam::FaderParam (const juce::String& name)
{
    title.setText (name, juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centred);
    title.setColour (juce::Label::textColourId, textColour());
    addAndMakeVisible (title);

    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange (0.0, 100.0, 1.0);
    slider.setColour (juce::Slider::trackColourId, lineColour());
    slider.setColour (juce::Slider::thumbColourId, accentColour());
    addAndMakeVisible (slider);
}

void FaderParam::resized()
{
    auto area = getLocalBounds();
    title.setBounds (area.removeFromBottom (18));
    slider.setBounds (area.reduced (10, 4));
}

void FaderParam::paint (juce::Graphics& g)
{
    g.setColour (lineColour());
    g.drawRoundedRectangle (getLocalBounds().toFloat(), 8.0f, 1.0f);
}

void FaderParam::setValue (double value) { slider.setValue (value, juce::dontSendNotification); }
double FaderParam::getValue() const { return slider.getValue(); }
juce::Slider& FaderParam::getSlider() { return slider; }

void PianoPreview::setNotes (int dominant, juce::Array<int> harmonics)
{
    dominantMidi = dominant;
    harmonicMidis = std::move (harmonics);
    repaint();
}

void PianoPreview::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.fillAll (juce::Colour::fromRGB (239, 239, 239));

    auto isBlack = [] (int midi) { return juce::Array<int> { 1, 3, 6, 8, 10 }.contains (midi % 12); };
    const int whiteW = 20, blackW = 12, whiteH = area.getHeight() - 8, blackH = (int) (whiteH * 0.63f);
    juce::HashMap<int, int> whiteX;
    int whiteIndex = 0;

    for (int midi = 48; midi <= 96; ++midi)
    {
        if (! isBlack (midi))
        {
            const int x = whiteIndex * whiteW;
            const bool active = midi == dominantMidi;
            const bool harmonic = harmonicMidis.contains (midi) && ! active;
            g.setColour (active ? juce::Colour::fromRGB (125, 211, 252)
                                : harmonic ? juce::Colour::fromRGB (255, 227, 155)
                                           : juce::Colours::white);
            g.fillRect (x, 0, whiteW, whiteH);
            g.setColour (juce::Colour::fromRGB (72, 72, 72));
            g.drawRect (x, 0, whiteW, whiteH);
            whiteX.set (midi, x);
            ++whiteIndex;
        }
    }

    for (int midi = 48; midi <= 96; ++midi)
    {
        if (isBlack (midi))
        {
            const int* left = whiteX[midi - 1];
            if (left == nullptr) continue;
            const int x = *left + whiteW - blackW / 2;
            const bool active = midi == dominantMidi;
            const bool harmonic = harmonicMidis.contains (midi) && ! active;
            g.setColour (active ? juce::Colour::fromRGB (56, 189, 248)
                                : harmonic ? juce::Colour::fromRGB (246, 182, 60)
                                           : juce::Colours::black);
            g.fillRect (x, 0, blackW, blackH);
            g.setColour (juce::Colours::black);
            g.drawRect (x, 0, blackW, blackH);
        }
    }
}

MainComponent::MainComponent()
{
    titleLabel.setText ("音色适配器 | JUCE Desktop Prototype", juce::dontSendNotification);
    titleLabel.setColour (juce::Label::textColourId, textColour());
    titleLabel.setFont (juce::FontOptions().withHeight (24.0f).withStyle ("Bold"));
    addAndMakeVisible (titleLabel);

    infoLabel.setText ("加载音频后，本地快速分析音色并映射到 FL 风格参数面板。", juce::dontSendNotification);
    infoLabel.setColour (juce::Label::textColourId, mutedColour());
    addAndMakeVisible (infoLabel);

    for (auto* b : { &openButton, &analyseButton, &playButton, &stopButton, &exportButton })
    {
        addAndMakeVisible (*b);
        b->addListener (this);
        b->setColour (juce::TextButton::buttonColourId, panel2Colour());
        b->setColour (juce::TextButton::textColourOffId, textColour());
    }
    analyseButton.setEnabled (false);
    playButton.setEnabled (false);
    stopButton.setEnabled (false);
    exportButton.setEnabled (false);

    for (auto* l : { &brightnessStat, &noiseStat, &attackStat, &widthStat, &pitchStat, &envStat })
    {
        addAndMakeVisible (*l);
        l->setColour (juce::Label::backgroundColourId, panel2Colour());
        l->setColour (juce::Label::outlineColourId, lineColour());
        l->setColour (juce::Label::textColourId, textColour());
        l->setJustificationType (juce::Justification::centredLeft);
    }

    snapshotBox.setMultiLine (true);
    snapshotBox.setReadOnly (true);
    snapshotBox.setColour (juce::TextEditor::backgroundColourId, panel2Colour());
    snapshotBox.setColour (juce::TextEditor::outlineColourId, lineColour());
    snapshotBox.setColour (juce::TextEditor::textColourId, textColour());
    addAndMakeVisible (snapshotBox);

    analysisHintBox.setMultiLine (true);
    analysisHintBox.setReadOnly (true);
    analysisHintBox.setColour (juce::TextEditor::backgroundColourId, panel2Colour());
    analysisHintBox.setColour (juce::TextEditor::outlineColourId, lineColour());
    analysisHintBox.setColour (juce::TextEditor::textColourId, textColour());
    addAndMakeVisible (analysisHintBox);

    for (auto* k : { &cutoff, &resonance, &brightness, &drive, &unison, &detune, &harmonics, &noise, &stereo, &fmDepth, &tone, &filterMix })
    {
        mainKnobs.add (k);
        addAndMakeVisible (*k);
        k->getSlider().addListener (this);
    }

    for (auto* f : { &attackF, &decayF, &sustainF, &releaseF })
    {
        envFaders.add (f);
        addAndMakeVisible (*f);
        f->getSlider().addListener (this);
    }

    addAndMakeVisible (pianoPreview);

    formatManager = std::make_unique<juce::AudioFormatManager>();
    formatManager->registerBasicFormats();
    deviceManager.initialise (0, 2, nullptr, true);
    deviceManager.addAudioCallback (&player);
    player.setSource (&transport);

    setSize (1400, 900);
}

MainComponent::~MainComponent()
{
    transport.stop();
    transport.setSource (nullptr);
    player.setSource (nullptr);
    deviceManager.removeAudioCallback (&player);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (bgColour());
    auto r = getLocalBounds().toFloat();
    juce::ColourGradient grad (bg2Colour(), 0, 0, bgColour(), 0, (float) getHeight(), false);
    g.setGradientFill (grad);
    g.fillRect (r);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (16);
    auto top = area.removeFromTop (56);
    titleLabel.setBounds (top.removeFromTop (28));
    infoLabel.setBounds (top);

    auto left = area.removeFromLeft (380);
    auto right = area;

    auto buttonRow = left.removeFromTop (36);
    openButton.setBounds (buttonRow.removeFromLeft (80));
    analyseButton.setBounds (buttonRow.removeFromLeft (90));
    playButton.setBounds (buttonRow.removeFromLeft (70));
    stopButton.setBounds (buttonRow.removeFromLeft (70));
    exportButton.setBounds (buttonRow.removeFromLeft (90));
    left.removeFromTop (10);

    auto stats = left.removeFromTop (210);
    const int statH = 32;
    brightnessStat.setBounds (stats.removeFromTop (statH)); stats.removeFromTop (4);
    noiseStat.setBounds (stats.removeFromTop (statH)); stats.removeFromTop (4);
    attackStat.setBounds (stats.removeFromTop (statH)); stats.removeFromTop (4);
    widthStat.setBounds (stats.removeFromTop (statH)); stats.removeFromTop (4);
    pitchStat.setBounds (stats.removeFromTop (statH)); stats.removeFromTop (4);
    envStat.setBounds (stats.removeFromTop (statH));

    left.removeFromTop (10);
    snapshotBox.setBounds (left.removeFromTop (240));
    left.removeFromTop (10);
    analysisHintBox.setBounds (left);

    auto topRight = right.removeFromTop (420);
    auto knobArea = topRight.removeFromLeft (760);
    auto envArea = topRight;

    const int knobW = 95, knobH = 112, cols = 4;
    for (int i = 0; i < mainKnobs.size(); ++i)
    {
        auto row = i / cols;
        auto col = i % cols;
        mainKnobs[i]->setBounds (knobArea.getX() + col * (knobW + 8), knobArea.getY() + row * (knobH + 8), knobW, knobH);
    }

    for (int i = 0; i < envFaders.size(); ++i)
        envFaders[i]->setBounds (envArea.getX() + i * 74, envArea.getY(), 64, 170);

    right.removeFromTop (10);
    pianoPreview.setBounds (right.removeFromTop (140));
}

void MainComponent::buttonClicked (juce::Button* b)
{
    if (b == &openButton) openFile();
    else if (b == &analyseButton) analyseLoadedFile();
    else if (b == &playButton) playFile();
    else if (b == &stopButton) stopFile();
    else if (b == &exportButton) exportSnapshot();
}

void MainComponent::sliderValueChanged (juce::Slider*)
{
    snapshotBox.setText (buildSnapshotText(), false);
}

void MainComponent::openFile()
{
    juce::FileChooser chooser ("选择 1–15 秒音频", {}, "*.wav;*.mp3;*.aif;*.aiff;*.flac;*.m4a");
    if (! chooser.browseForFileToOpen())
        return;

    loadedFile = chooser.getResult();
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager->createReaderFor (loadedFile));
    if (reader == nullptr)
    {
        infoLabel.setText ("无法读取该音频文件。", juce::dontSendNotification);
        return;
    }

    readerSource.reset (new juce::AudioFormatReaderSource (reader.release(), true));
    transport.setSource (readerSource.get(), 0, nullptr, readerSource->sampleRate);
    analyseButton.setEnabled (true);
    playButton.setEnabled (true);
    stopButton.setEnabled (true);
    infoLabel.setText ("已加载：" + loadedFile.getFileName(), juce::dontSendNotification);
}

void MainComponent::playFile() { transport.start(); }
void MainComponent::stopFile() { transport.stop(); transport.setPosition (0.0); }

void MainComponent::exportSnapshot()
{
    juce::FileChooser chooser ("导出快照", juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile ("timbre-adapter-snapshot.json"), "*.json");
    if (! chooser.browseForFileToSave (true))
        return;

    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty ("source", loadedFile.getFileName());
    root->setProperty ("cutoff", cutoff.getValue());
    root->setProperty ("resonance", resonance.getValue());
    root->setProperty ("brightness", brightness.getValue());
    root->setProperty ("drive", drive.getValue());
    root->setProperty ("unison", unison.getValue());
    root->setProperty ("detune", detune.getValue());
    root->setProperty ("harmonics", harmonics.getValue());
    root->setProperty ("noise", noise.getValue());
    root->setProperty ("stereo", stereo.getValue());
    root->setProperty ("fmDepth", fmDepth.getValue());
    root->setProperty ("attack", attackF.getValue());
    root->setProperty ("decay", decayF.getValue());
    root->setProperty ("sustain", sustainF.getValue());
    root->setProperty ("release", releaseF.getValue());

    chooser.getResult().replaceWithText (juce::JSON::toString (juce::var (root.get()), true));
}

MainComponent::AnalysisResult MainComponent::runAnalysis (const juce::AudioBuffer<float>& monoBuffer, double sampleRate)
{
    AnalysisResult result;
    result.durationSec = monoBuffer.getNumSamples() / sampleRate;
    result.rms = computeRMS (monoBuffer);
    result.zcr = computeZCR (monoBuffer);

    const auto env = computeEnvelope (monoBuffer, 512);
    result.attackTime = estimateAttack (env, sampleRate, 512);
    result.envelopeType = estimateEnvelopeType (env);

    const auto spectrum = analyseSpectrum (monoBuffer, sampleRate);
    result.centroid = spectrum.centroid;
    result.rolloff = spectrum.rolloff;
    result.flatness = spectrum.flatness;
    result.peakiness = spectrum.peakiness;
    result.highRatio = spectrum.highRatio;
    result.motion = spectrum.motion;

    const auto pitch = detectDominantPitch (monoBuffer, sampleRate);
    result.dominantMidi = pitch.first;
    result.dominantFreq = pitch.second;
    result.dominantNote = midiToNoteName (result.dominantMidi);

    for (auto offset : { 0, 12, 19, 24 })
    {
        const auto m = result.dominantMidi + offset;
        if (m >= 48 && m <= 96)
            result.harmonicMidis.addIfNotAlreadyThere (m);
    }

    return result;
}

void MainComponent::analyseLoadedFile()
{
    if (! loadedFile.existsAsFile())
        return;

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager->createReaderFor (loadedFile));
    if (reader == nullptr)
    {
        infoLabel.setText ("读取失败。", juce::dontSendNotification);
        return;
    }

    const auto duration = reader->lengthInSamples / reader->sampleRate;
    if (duration < 1.0 || duration > 15.0)
    {
        infoLabel.setText ("音频长度需要在 1–15 秒之间。", juce::dontSendNotification);
        return;
    }

    juce::AudioBuffer<float> original ((int) reader->numChannels, (int) reader->lengthInSamples);
    reader->read (&original, 0, (int) reader->lengthInSamples, 0, true, true);

    juce::AudioBuffer<float> mono (1, original.getNumSamples());
    mono.clear();
    for (int ch = 0; ch < original.getNumChannels(); ++ch)
        mono.addFrom (0, 0, original, ch, 0, original.getNumSamples(), 1.0f / original.getNumChannels());

    auto result = runAnalysis (mono, reader->sampleRate);
    result.stereoWidth = computeStereoWidth (original);
    applyResultToUi (result);
    hasResult = true;
    infoLabel.setText ("分析完成：" + loadedFile.getFileName(), juce::dontSendNotification);
}

void MainComponent::applyResultToUi (const AnalysisResult& r)
{
    lastResult = r;

    auto brightnessV = mapRange (r.centroid, 250.0, 3800.0, 8.0, 100.0);
    auto cutoffV = mapRange (r.rolloff, 900.0, 7000.0, 18.0, 100.0);
    auto resonanceV = mapRange (r.peakiness, 2.0, 20.0, 20.0, 90.0);
    auto noiseV = mapRange ((r.flatness * 0.8 + r.zcr * 2.5), 0.02, 0.45, 0.0, 100.0);
    auto stereoV = mapRange (r.stereoWidth, 0.0, 1.0, 0.0, 100.0);
    auto harmonicsV = mapRange (r.highRatio, 0.04, 0.65, 8.0, 100.0);
    auto attackV = mapRange (r.attackTime, 0.005, 0.35, 2.0, 100.0);
    auto driveV = mapRange (r.rms, 0.03, 0.42, 5.0, 95.0);
    auto unisonV = mapRange ((r.stereoWidth + r.motion / 900.0) * 0.5, 0.0, 1.0, 0.0, 100.0);
    auto detuneV = mapRange (r.motion, 5.0, 550.0, 0.0, 70.0);
    auto toneV = juce::jlimit (0.0, 100.0, brightnessV * 0.6 + harmonicsV * 0.4);
    auto filterMixV = juce::jlimit (0.0, 100.0, cutoffV * 0.6 + resonanceV * 0.4);
    auto fmDepthV = mapRange ((r.peakiness * 0.45 + r.motion * 0.002), 0.5, 10.0, 0.0, 88.0);

    brightness.setValue (brightnessV);
    cutoff.setValue (cutoffV);
    resonance.setValue (resonanceV);
    noise.setValue (noiseV);
    stereo.setValue (stereoV);
    harmonics.setValue (harmonicsV);
    drive.setValue (driveV);
    unison.setValue (unisonV);
    detune.setValue (detuneV);
    tone.setValue (toneV);
    filterMix.setValue (filterMixV);
    fmDepth.setValue (fmDepthV);

    attackF.setValue (attackV);
    decayF.setValue (mapRange (r.flatness, 0.01, 0.65, 20.0, 85.0));
    sustainF.setValue (mapRange (1.0 - r.flatness, 0.2, 1.0, 25.0, 90.0));
    releaseF.setValue (mapRange (r.stereoWidth + r.motion / 600.0, 0.0, 1.0, 12.0, 88.0));

    brightnessStat.setText ("亮度 Brightness: " + juce::String ((int) brightnessV) + "%", juce::dontSendNotification);
    noiseStat.setText ("噪声感 Noise: " + juce::String ((int) noiseV) + "%", juce::dontSendNotification);
    attackStat.setText ("起音 Attack: " + juce::String (r.attackTime, 3) + " s", juce::dontSendNotification);
    widthStat.setText ("立体声宽度 Width: " + juce::String ((int) stereoV) + "%", juce::dontSendNotification);
    pitchStat.setText ("主峰音高: " + r.dominantNote + " / " + juce::String (r.dominantFreq, 1) + " Hz", juce::dontSendNotification);
    envStat.setText ("包络类型: " + r.envelopeType, juce::dontSendNotification);

    snapshotBox.setText (buildSnapshotText(), false);

    juce::String hint;
    if (r.envelopeType.containsIgnoreCase ("Pluck")) hint << "更像 pluck / mallet，建议短 Attack、短 Decay、低 Sustain。\n";
    if (r.envelopeType.containsIgnoreCase ("Lead")) hint << "更像 lead / key，建议中低 Attack、适中 Sustain。\n";
    if (r.envelopeType.containsIgnoreCase ("Pad")) hint << "更像 pad / swell，建议提高 Attack 和 Release。\n";
    if (noiseV > 55.0) hint << "噪声感偏高，可加入 noise / breath / unison blur。\n";
    if (harmonicsV > 60.0) hint << "高频谐波较多，适合 brighter wave / FM / saturation。\n";
    if (stereoV > 55.0) hint << "立体声较宽，可提高 unison、stereo spread、chorus。\n";
    if (hint.isEmpty()) hint = "这是一个比较均衡的音色，适合从 saw / sine mix + 中等滤波与包络开始复刻。";
    analysisHintBox.setText (hint, false);

    pianoPreview.setNotes (r.dominantMidi, r.harmonicMidis);
    exportButton.setEnabled (true);
}

juce::String MainComponent::buildSnapshotText() const
{
    return "Patch Snapshot\n"
           "------------------------------------------------\n"
         + "Source: " + loadedFile.getFileName() + "\n"
         + "Dominant pitch: " + lastResult.dominantNote + " (" + juce::String (lastResult.dominantFreq, 1) + " Hz)\n"
         + "Envelope type: " + lastResult.envelopeType + "\n\n"
         + "Cutoff: " + juce::String ((int) cutoff.getValue()) + "%\n"
         + "Resonance: " + juce::String ((int) resonance.getValue()) + "%\n"
         + "Brightness: " + juce::String ((int) brightness.getValue()) + "%\n"
         + "Drive: " + juce::String ((int) drive.getValue()) + "%\n"
         + "Unison: " + juce::String ((int) unison.getValue()) + "%\n"
         + "Detune: " + juce::String ((int) detune.getValue()) + "%\n"
         + "Harmonics: " + juce::String ((int) harmonics.getValue()) + "%\n"
         + "Noise: " + juce::String ((int) noise.getValue()) + "%\n"
         + "Stereo: " + juce::String ((int) stereo.getValue()) + "%\n"
         + "FMDepth: " + juce::String ((int) fmDepth.getValue()) + "%\n"
         + "Attack: " + juce::String ((int) attackF.getValue()) + "%\n"
         + "Decay: " + juce::String ((int) decayF.getValue()) + "%\n"
         + "Sustain: " + juce::String ((int) sustainF.getValue()) + "%\n"
         + "Release: " + juce::String ((int) releaseF.getValue()) + "%\n";
}

juce::String MainComponent::midiToNoteName (int midi) const
{
    static const juce::StringArray names { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const auto octave = midi / 12 - 1;
    return names[midi % 12] + juce::String (octave);
}
