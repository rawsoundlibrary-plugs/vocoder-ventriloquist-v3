#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/VentriloquistEngine.h"

namespace ParamIDs
{
    inline constexpr const char* mode    = "mode";
    inline constexpr const char* size    = "size";
    inline constexpr const char* carrier = "carrier";
    inline constexpr const char* pitch   = "pitch";
    inline constexpr const char* key     = "key";
    inline constexpr const char* linger  = "linger";
    inline constexpr const char* whisper = "whisper";
    inline constexpr const char* chorus  = "chorus";
    inline constexpr const char* gate    = "gate";
    inline constexpr const char* mix     = "mix";
    inline constexpr const char* output  = "output";
    inline constexpr const char* freeze  = "freeze";
}

class VentriloquistProcessor : public juce::AudioProcessor
{
public:
    VentriloquistProcessor();
    ~VentriloquistProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /** Called by the interface ("SHAKE THE BOX"): random puppet and settings. Message thread only. */
    void shakeTheBox();
    /** Sets a parameter from the interface as one complete gesture. Message thread only. */
    void setParam (const char* id, float plainValue);

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;
    vq::Engine engine;

private:
    void render (const float* inL, const float* inR, const float* scL, const float* scR,
                 float* outL, float* outR, int numSamples);
    void handleMidi (const juce::MidiMessage& m);

    std::atomic<float>* pMode = nullptr; std::atomic<float>* pSize = nullptr; std::atomic<float>* pCarrier = nullptr;
    std::atomic<float>* pPitch = nullptr; std::atomic<float>* pKey = nullptr; std::atomic<float>* pLinger = nullptr;
    std::atomic<float>* pWhisper = nullptr; std::atomic<float>* pChorus = nullptr; std::atomic<float>* pGate = nullptr;
    std::atomic<float>* pMix = nullptr; std::atomic<float>* pOutput = nullptr; std::atomic<float>* pFreeze = nullptr;

    std::vector<float> scratch;   // second channel when the host runs us in mono
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VentriloquistProcessor)
};
