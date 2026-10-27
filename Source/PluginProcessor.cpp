#include "PluginProcessor.h"
#include "PluginEditor.h"

MoosikSynthProcessor::MoosikSynthProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new SynthVoice (wavetable));

    synth.addSound (new SynthSound());
}

juce::AudioProcessorValueTreeState::ParameterLayout MoosikSynthProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto seconds = [] (float min, float max) { return NormalisableRange<float> (min, max, 0.001f, 0.4f); };
    auto unit = [] { return NormalisableRange<float> (0.0f, 1.0f); };

    auto addFloat = [&] (const char* id, const char* name, NormalisableRange<float> range, float defaultValue)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, defaultValue));
    };
    auto addInt = [&] (const char* id, const char* name, int min, int max, int defaultValue)
    {
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, min, max, defaultValue));
    };
    auto addChoice = [&] (const char* id, const char* name, StringArray choices, int defaultIndex)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, choices, defaultIndex));
    };

    const StringArray oscWaves { "Sine", "Saw", "Square", "Wavetable" };

    addChoice ("osc1_wave", "Osc 1 Wave", oscWaves, 1);
    addFloat  ("osc1_pos", "Osc 1 WT Position", unit(), 0.0f);
    addInt    ("osc1_octave", "Osc 1 Octave", -3, 3, 0);
    addInt    ("osc1_semi", "Osc 1 Semitone", -12, 12, 0);
    addFloat  ("osc1_level", "Osc 1 Level", unit(), 0.8f);

    addChoice ("osc2_wave", "Osc 2 Wave", oscWaves, 3);
    addFloat  ("osc2_pos", "Osc 2 WT Position", unit(), 0.35f);
    addInt    ("osc2_octave", "Osc 2 Octave", -3, 3, -1);
    addInt    ("osc2_semi", "Osc 2 Semitone", -12, 12, 0);
    addFloat  ("osc2_level", "Osc 2 Level", unit(), 0.5f);

    addInt    ("unison", "Unison", 1, SynthVoice::maxUnison, 3);
    addFloat  ("detune", "Detune", NormalisableRange<float> (0.0f, 100.0f, 0.1f, 0.5f), 15.0f);
    addFloat  ("spread", "Spread", unit(), 0.5f);

    addFloat  ("attack", "Attack", seconds (0.001f, 5.0f), 0.01f);
    addFloat  ("decay", "Decay", seconds (0.001f, 5.0f), 0.2f);
    addFloat  ("sustain", "Sustain", unit(), 0.8f);
    addFloat  ("release", "Release", seconds (0.001f, 10.0f), 0.3f);

    addFloat  ("cutoff", "Cutoff", NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.25f), 1500.0f);
    addFloat  ("resonance", "Resonance", NormalisableRange<float> (0.5f, 10.0f, 0.01f, 0.5f), 0.7f);
    addFloat  ("fenv_amount", "Filter Env Amount", NormalisableRange<float> (-1.0f, 1.0f), 0.5f);

    addFloat  ("fenv_attack", "Filter Env Attack", seconds (0.001f, 5.0f), 0.005f);
    addFloat  ("fenv_decay", "Filter Env Decay", seconds (0.001f, 5.0f), 0.4f);
    addFloat  ("fenv_sustain", "Filter Env Sustain", unit(), 0.2f);
    addFloat  ("fenv_release", "Filter Env Release", seconds (0.001f, 10.0f), 0.4f);

    addChoice ("lfo_shape", "LFO Shape", { "Sine", "Triangle", "Saw", "Square" }, 0);
    addChoice ("lfo_dest", "LFO Destination", { "Cutoff", "Pitch", "WT Position", "Volume" }, 0);
    addFloat  ("lfo_rate", "LFO Rate", NormalisableRange<float> (0.05f, 20.0f, 0.01f, 0.4f), 2.0f);
    addFloat  ("lfo_amount", "LFO Amount", unit(), 0.0f);

    addFloat  ("gain", "Gain", NormalisableRange<float> (-48.0f, 6.0f, 0.1f), -12.0f);
    return layout;
}

void MoosikSynthProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<SynthVoice*> (synth.getVoice (i)))
            voice->prepare (sampleRate, samplesPerBlock);
}

bool MoosikSynthProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void MoosikSynthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // Merge notes played on the on-screen keyboard into the incoming MIDI.
    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    VoiceParams params;

    static constexpr const char* oscIds[2][5] { { "osc1_wave", "osc1_pos", "osc1_octave", "osc1_semi", "osc1_level" },
                                                { "osc2_wave", "osc2_pos", "osc2_octave", "osc2_semi", "osc2_level" } };

    for (size_t o = 0; o < params.osc.size(); ++o)
    {
        const auto* ids = oscIds[o];
        auto& osc = params.osc[o];
        osc.wave     = (OscWave) (int) get (ids[0]);
        osc.position = get (ids[1]);
        osc.octave   = (int) get (ids[2]);
        osc.semitone = (int) get (ids[3]);
        osc.level    = get (ids[4]);
    }

    params.unison  = (int) get ("unison");
    params.detune  = get ("detune");
    params.spread  = get ("spread");

    params.ampEnv  = { get ("attack"), get ("decay"), get ("sustain"), get ("release") };
    params.filterEnv = { get ("fenv_attack"), get ("fenv_decay"), get ("fenv_sustain"), get ("fenv_release") };
    params.cutoff  = get ("cutoff");
    params.resonance = get ("resonance");
    params.filterEnvAmount = get ("fenv_amount");

    params.lfoShape  = (LfoShape) (int) get ("lfo_shape");
    params.lfoDest   = (LfoDest) (int) get ("lfo_dest");
    params.lfoRate   = get ("lfo_rate");
    params.lfoAmount = get ("lfo_amount");

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<SynthVoice*> (synth.getVoice (i)))
            voice->setParams (params);

    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    buffer.applyGain (juce::Decibels::decibelsToGain (get ("gain")));
}

juce::AudioProcessorEditor* MoosikSynthProcessor::createEditor()
{
    return new MoosikSynthEditor (*this);
}

void MoosikSynthProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MoosikSynthProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MoosikSynthProcessor();
}
