#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
juce::AudioParameterFloatAttributes pct()
{
    return juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; });
}
}

juce::AudioProcessorValueTreeState::ParameterLayout VentriloquistProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::mode, 1 }, "Puppet",
                                                        StringArray { "Robot", "Monster", "Ghost", "Angel", "Radio", "Creature" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::size, 1 }, "Puppet size",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                       {
                                                           const float st = vq::formantFromSize (v);
                                                           return String (st >= 0 ? "+" : "") + String (st, 1) + " st";
                                                       })));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::carrier, 1 }, "Carrier", StringArray { "synth", "noise", "sidechain" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::pitch, 1 }, "Pitch", StringArray { "free", "major", "minor" }, 2));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::key, 1 }, "Key",
                                                        StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::linger, 1 }, "Linger",  NormalisableRange<float> (0.0f, 100.0f, 0.1f), 35.0f, pct()));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::whisper, 1 }, "Whisper", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, pct()));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::chorus, 1 }, "Chorus",  NormalisableRange<float> (0.0f, 100.0f, 0.1f), 20.0f, pct()));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::gate, 1 }, "Gate",      NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f, pct()));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::mix, 1 }, "Mix",        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pct()));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::output, 1 }, "Output",  NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int)
                                                       { return String (v >= 0 ? "+" : "") + String (v, 1) + " dB"; })));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::freeze, 1 }, "Freeze", false));
    return layout;
}

VentriloquistProcessor::VentriloquistProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VENTRILOQUIST", createLayout())
{
    pMode    = apvts.getRawParameterValue (ParamIDs::mode);
    pSize    = apvts.getRawParameterValue (ParamIDs::size);
    pCarrier = apvts.getRawParameterValue (ParamIDs::carrier);
    pPitch   = apvts.getRawParameterValue (ParamIDs::pitch);
    pKey     = apvts.getRawParameterValue (ParamIDs::key);
    pLinger  = apvts.getRawParameterValue (ParamIDs::linger);
    pWhisper = apvts.getRawParameterValue (ParamIDs::whisper);
    pChorus  = apvts.getRawParameterValue (ParamIDs::chorus);
    pGate    = apvts.getRawParameterValue (ParamIDs::gate);
    pMix     = apvts.getRawParameterValue (ParamIDs::mix);
    pOutput  = apvts.getRawParameterValue (ParamIDs::output);
    pFreeze  = apvts.getRawParameterValue (ParamIDs::freeze);
}

bool VentriloquistProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mono = juce::AudioChannelSet::mono(), stereo = juce::AudioChannelSet::stereo();
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != mono && mainOut != stereo) return false;
    if (layouts.getMainInputChannelSet() != mainOut) return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != mono && sc != stereo) return false;
    }
    return true;
}

void VentriloquistProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    scratch.assign ((size_t) juce::jmax (512, samplesPerBlock), 0.0f);
    vq::Params p; p.mode = (int) pMode->load();
    engine.setParams (p);
    engine.prepare (sampleRate, samplesPerBlock);
}

void VentriloquistProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())                         engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())                   engine.noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
}

void VentriloquistProcessor::render (const float* inL, const float* inR, const float* scL, const float* scR,
                                     float* outL, float* outR, int numSamples)
{
    if (numSamples <= 0) return;
    if (outR != nullptr) { engine.process (inL, inR, scL, scR, outL, outR, numSamples); return; }

    // mono host: render in chunks, second channel goes to scratch
    const int chunk = (int) scratch.size();
    for (int pos = 0; pos < numSamples; pos += chunk)
    {
        const int n = juce::jmin (chunk, numSamples - pos);
        engine.process (inL + pos, nullptr, scL != nullptr ? scL + pos : nullptr, scR != nullptr ? scR + pos : nullptr,
                        outL + pos, scratch.data(), n);
    }
}

void VentriloquistProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    vq::Params p;
    p.mode      = (int) pMode->load();
    p.sizePct   = pSize->load();
    p.carrier   = (int) pCarrier->load();
    p.pitchMode = (int) pPitch->load();
    p.key       = (int) pKey->load();
    p.linger    = pLinger->load() * 0.01f;
    p.whisper   = pWhisper->load() * 0.01f;
    p.chorus    = pChorus->load() * 0.01f;
    p.gate      = pGate->load() * 0.01f;
    p.mix       = pMix->load() * 0.01f;
    p.outputDb  = pOutput->load();
    p.freeze    = pFreeze->load() > 0.5f;
    engine.setParams (p);

    auto mainIn  = getBusBuffer (buffer, true, 0);
    auto mainOut = getBusBuffer (buffer, false, 0);
    const int n = buffer.getNumSamples();
    if (mainIn.getNumChannels() == 0 || mainOut.getNumChannels() == 0) return;

    const float* scL = nullptr; const float* scR = nullptr;
    if (getBusCount (true) > 1)
        if (auto* bus = getBus (true, 1); bus != nullptr && bus->isEnabled())
        {
            auto sc = getBusBuffer (buffer, true, 1);
            if (sc.getNumChannels() > 0)
            {
                scL = sc.getReadPointer (0);
                scR = sc.getNumChannels() > 1 ? sc.getReadPointer (1) : scL;
            }
        }

    const float* inL = mainIn.getReadPointer (0);
    const float* inR = mainIn.getNumChannels() > 1 ? mainIn.getReadPointer (1) : nullptr;
    float* outL = mainOut.getWritePointer (0);
    float* outR = mainOut.getNumChannels() > 1 ? mainOut.getWritePointer (1) : nullptr;

    // sample-accurate MIDI: render between events
    int pos = 0;
    for (const auto meta : midi)
    {
        const int evPos = juce::jlimit (0, n, meta.samplePosition);
        render (inL + pos, inR != nullptr ? inR + pos : nullptr, scL != nullptr ? scL + pos : nullptr, scR != nullptr ? scR + pos : nullptr,
                outL + pos, outR != nullptr ? outR + pos : nullptr, evPos - pos);
        handleMidi (meta.getMessage());
        pos = evPos;
    }
    render (inL + pos, inR != nullptr ? inR + pos : nullptr, scL != nullptr ? scL + pos : nullptr, scR != nullptr ? scR + pos : nullptr,
            outL + pos, outR != nullptr ? outR + pos : nullptr, n - pos);

    for (int ch = mainOut.getNumChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);
}

//==============================================================================
int VentriloquistProcessor::getNumPrograms() { return vq::numFactoryPresets; }

const juce::String VentriloquistProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, vq::numFactoryPresets) ? juce::String (vq::factoryPresets[index].name) : juce::String();
}

void VentriloquistProcessor::setParam (const char* id, float plainValue)
{
    if (auto* param = apvts.getParameter (id))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (plainValue));
        param->endChangeGesture();
    }
}

void VentriloquistProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, vq::numFactoryPresets)) return;
    currentProgram = index;
    const auto& pr = vq::factoryPresets[index];
    setParam (ParamIDs::mode, (float) pr.mode);
    setParam (ParamIDs::size, pr.size);
    setParam (ParamIDs::carrier, (float) pr.carrier);
    setParam (ParamIDs::pitch, (float) pr.pitch);
    setParam (ParamIDs::linger, pr.linger);
    setParam (ParamIDs::whisper, pr.whisper);
    setParam (ParamIDs::chorus, pr.chorus);
    setParam (ParamIDs::gate, pr.gate);
    setParam (ParamIDs::mix, pr.mix);
    setParam (ParamIDs::freeze, 0.0f);
}

void VentriloquistProcessor::shakeTheBox()
{
    auto& r = juce::Random::getSystemRandom();
    setParam (ParamIDs::mode, (float) r.nextInt (vq::kNumModes));
    setParam (ParamIDs::size, r.nextFloat() * 100.0f);
    setParam (ParamIDs::linger, r.nextFloat() * 100.0f);
    setParam (ParamIDs::whisper, 20.0f + r.nextFloat() * 70.0f);
    setParam (ParamIDs::chorus, r.nextFloat() * 70.0f);
    setParam (ParamIDs::pitch, (float) r.nextInt (3));
}

void VentriloquistProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VentriloquistProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) state.getProperty ("program", 0);
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* VentriloquistProcessor::createEditor() { return new VentriloquistEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VentriloquistProcessor(); }
