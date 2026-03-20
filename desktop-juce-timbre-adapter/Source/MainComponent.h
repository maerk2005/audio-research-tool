#pragma once

#include <JuceHeader.h>

class RotaryParam : public juce::Component
{
public:
    RotaryParam (const juce::String& name, const juce::String& suffix = "%");
    void resized() override;
    void paint (juce::Graphics&) override;

    void setValue (double value);
    double getValue() const;
    juce::Slider& getSlider();

private:
    juce::Label title;
    juce::Label valueLabel;
    juce::Slider slider;
    juce::String suffix;
};

class FaderParam : public juce::Component
{
public:
    explicit FaderParam (const juce::String& name);
    void resized() override;
    void paint (juce::Graphics&) override;

    void setValue (double value);
    double getValue() const;
    juce::Slider& getSlider();

private:
    juce::Label title;
    juce::Slider slider;
};

class PianoPreview : public juce::Component
{
public:
    void setNotes (int dominantMidi, juce::Array<int> harmonicMidis);
    void paint (juce::Graphics&) override;

private:
    int dominantMidi = -1;
    juce::Array<int> harmonicMidis;
};

class MainComponent : public juce::Component,
                      private juce::Button::Listener,
                      private juce::Slider::Listener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void buttonClicked (juce::Button*) override;
    void sliderValueChanged (juce::Slider*) override;

    void openFile();
    void playFile();
    void stopFile();
    void exportSnapshot();
    void analyseLoadedFile();

    struct AnalysisResult
    {
        double durationSec = 0.0;
        double rms = 0.0;
        double zcr = 0.0;
        double attackTime = 0.01;
        double stereoWidth = 0.0;
        double centroid = 0.0;
        double rolloff = 0.0;
        double flatness = 0.0;
        double peakiness = 0.0;
        double highRatio = 0.0;
        double motion = 0.0;
        int dominantMidi = -1;
        juce::String dominantNote = "—";
        double dominantFreq = 0.0;
        juce::String envelopeType = "通用";
        juce::Array<int> harmonicMidis;
    };

    AnalysisResult runAnalysis (const juce::AudioBuffer<float>& buffer, double sampleRate);
    void applyResultToUi (const AnalysisResult& result);
    juce::String buildSnapshotText() const;
    juce::String midiToNoteName (int midi) const;

    juce::TextButton openButton { "加载音频" };
    juce::TextButton analyseButton { "分析音色" };
    juce::TextButton playButton { "播放" };
    juce::TextButton stopButton { "停止" };
    juce::TextButton exportButton { "导出快照" };

    juce::Label titleLabel;
    juce::Label infoLabel;
    juce::TextEditor snapshotBox;
    juce::TextEditor analysisHintBox;

    juce::Label brightnessStat;
    juce::Label noiseStat;
    juce::Label attackStat;
    juce::Label widthStat;
    juce::Label pitchStat;
    juce::Label envStat;

    RotaryParam cutoff { "Cutoff" };
    RotaryParam resonance { "Resonance" };
    RotaryParam brightness { "Brightness" };
    RotaryParam drive { "Drive" };
    RotaryParam unison { "Unison" };
    RotaryParam detune { "Detune" };
    RotaryParam harmonics { "Harmonics" };
    RotaryParam noise { "Noise" };
    RotaryParam stereo { "Stereo" };
    RotaryParam fmDepth { "FMDepth" };
    RotaryParam tone { "Tone" };
    RotaryParam filterMix { "FilterMix" };

    FaderParam attackF { "A" };
    FaderParam decayF { "D" };
    FaderParam sustainF { "S" };
    FaderParam releaseF { "R" };

    PianoPreview pianoPreview;

    juce::Array<RotaryParam*> mainKnobs;
    juce::Array<FaderParam*> envFaders;

    std::unique_ptr<juce::AudioFormatManager> formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transport;
    juce::AudioSourcePlayer player;
    juce::AudioDeviceManager deviceManager;

    juce::File loadedFile;
    AnalysisResult lastResult;
    bool hasResult = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
