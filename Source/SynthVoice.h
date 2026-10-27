#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "Wavetable.h"

// Any MIDI note can play on any voice.
struct SynthSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

enum class OscWave  { sine, saw, square, wavetable };
enum class LfoShape { sine, triangle, saw, square };
enum class LfoDest  { cutoff, pitch, wavetablePos, volume };

struct OscParams
{
    OscWave wave = OscWave::saw;
    float position = 0.0f;     // wavetable position, 0..1
    int octave = 0;
    int semitone = 0;
    float level = 0.8f;
};

// Settings shared by all voices; the processor copies parameter values in here once per block.
struct VoiceParams
{
    std::array<OscParams, 2> osc;
    int unison = 1;            // oscillators stacked per note, per osc
    float detune = 0.0f;       // cents between the outermost unison oscillators and the note
    float spread = 0.0f;       // stereo width of the unison stack, 0..1

    juce::ADSR::Parameters ampEnv { 0.01f, 0.2f, 0.8f, 0.3f };
    juce::ADSR::Parameters filterEnv { 0.005f, 0.4f, 0.2f, 0.4f };
    float cutoff = 1500.0f;
    float resonance = 0.7f;
    float filterEnvAmount = 0.5f;   // -1..1, scaled to +/- 6 octaves of cutoff movement

    LfoShape lfoShape = LfoShape::sine;
    LfoDest lfoDest = LfoDest::cutoff;
    float lfoRate = 2.0f;           // Hz
    float lfoAmount = 0.0f;         // 0..1
};

// One voice = two unison oscillator stacks -> stereo low-pass filter -> amp envelope,
// with a filter envelope and a per-voice LFO that restarts on every note.
class SynthVoice : public juce::SynthesiserVoice
{
public:
    static constexpr int maxUnison = 8;
    static constexpr int numOscs = 2;

    explicit SynthVoice (const Wavetable& table) : wavetable (table) {}

    void prepare (double sampleRate, int maxBlockSize)
    {
        filter.prepare ({ sampleRate, (juce::uint32) maxBlockSize, 2 });
        filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        ampEnv.setSampleRate (sampleRate);
        filterEnv.setSampleRate (sampleRate);
    }

    void setParams (const VoiceParams& p)
    {
        params = p;
        params.unison = juce::jlimit (1, maxUnison, p.unison);
        updatePitch();

        ampEnv.setParameters (p.ampEnv);
        filterEnv.setParameters (p.filterEnv);
        filter.setResonance (p.resonance);
    }

    bool canPlaySound (juce::SynthesiserSound* s) override
    {
        return dynamic_cast<SynthSound*> (s) != nullptr;
    }

    void startNote (int midiNote, float vel, juce::SynthesiserSound*, int) override
    {
        velocity = vel;
        noteFrequency = juce::MidiMessage::getMidiNoteInHertz (midiNote);
        lfoPhase = 0.0;

        // Random start phases stop the stacked oscillators cancelling each other out.
        for (auto& oscPhases : phases)
            for (auto& phase : oscPhases)
                phase = random.nextDouble();

        updatePitch();
        filter.reset();
        ampEnv.noteOn();
        filterEnv.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            ampEnv.noteOff();
            filterEnv.noteOff();
        }
        else
        {
            ampEnv.reset();
            filterEnv.reset();
            clearCurrentNote();
        }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) override
    {
        if (! isVoiceActive())
            return;

        const auto sampleRate = getSampleRate();

        for (int i = 0; i < numSamples; ++i)
        {
            if (! ampEnv.isActive())
            {
                clearCurrentNote();
                break;
            }

            // LFO -> chosen destination.
            const auto lfo = nextLfoValue (sampleRate) * params.lfoAmount;
            float cutoffOctaves = 0.0f, positionOffset = 0.0f, ampMod = 1.0f;
            double pitchRatio = 1.0;

            switch (params.lfoDest)
            {
                case LfoDest::cutoff:       cutoffOctaves = lfo * 4.0f; break;
                case LfoDest::pitch:        pitchRatio = std::pow (2.0, (double) lfo); break;   // amount 1 = +/- 1 octave
                case LfoDest::wavetablePos: positionOffset = lfo * 0.5f; break;
                case LfoDest::volume:       ampMod = 1.0f - 0.5f * (params.lfoAmount - lfo); break;
            }

            float left = 0.0f, right = 0.0f;

            for (size_t o = 0; o < numOscs; ++o)
            {
                const auto& osc = params.osc[o];
                if (osc.level <= 0.0f)
                    continue;

                const auto position = juce::jlimit (0.0f, 1.0f, osc.position + positionOffset);

                for (size_t u = 0; u < (size_t) params.unison; ++u)
                {
                    const auto s = oscillator (osc, position, phases[o][u], phaseIncs[o][u] * pitchRatio, mipLevels[o][u]) * osc.level;
                    left  += s * gainsL[u];
                    right += s * gainsR[u];
                }
            }

            // Filter envelope + LFO move the cutoff in octaves around the knob setting.
            const auto envOctaves = params.filterEnvAmount * filterEnv.getNextSample() * 6.0f;
            const auto cutoff = params.cutoff * std::pow (2.0f, envOctaves + cutoffOctaves);
            filter.setCutoffFrequency (juce::jlimit (20.0f, (float) sampleRate * 0.45f, cutoff));

            const auto env = ampEnv.getNextSample() * velocity * unisonLevel * ampMod;
            left  = filter.processSample (0, left) * env;
            right = filter.processSample (1, right) * env;

            if (output.getNumChannels() == 1)
            {
                output.addSample (0, startSample + i, 0.5f * (left + right));
            }
            else
            {
                output.addSample (0, startSample + i, left);
                output.addSample (1, startSample + i, right);
            }
        }
    }

private:
    // Works out each oscillator's pitch (octave/semitone + unison detune) and stereo position.
    void updatePitch()
    {
        const auto sampleRate = getSampleRate();
        if (sampleRate <= 0.0)
            return;

        const auto unison = params.unison;

        for (size_t o = 0; o < numOscs; ++o)
        {
            const auto& osc = params.osc[o];
            const auto oscFrequency = noteFrequency * std::pow (2.0, osc.octave + osc.semitone / 12.0);

            for (int u = 0; u < unison; ++u)
            {
                const auto pos = unison > 1 ? 2.0f * (float) u / (float) (unison - 1) - 1.0f : 0.0f;
                const auto inc = oscFrequency * std::pow (2.0, pos * params.detune / 1200.0) / sampleRate;
                phaseIncs[o][(size_t) u] = inc;
                mipLevels[o][(size_t) u] = Wavetable::levelFor (inc);
            }
        }

        for (int u = 0; u < unison; ++u)
        {
            const auto pos = unison > 1 ? 2.0f * (float) u / (float) (unison - 1) - 1.0f : 0.0f;
            const auto pan = pos * params.spread;
            gainsL[(size_t) u] = juce::jmin (1.0f, 1.0f - pan);
            gainsR[(size_t) u] = juce::jmin (1.0f, 1.0f + pan);
        }

        unisonLevel = 1.0f / std::sqrt ((float) unison);   // keeps loudness roughly constant as voices are added
    }

    float nextLfoValue (double sampleRate)
    {
        const auto p = lfoPhase;
        lfoPhase += params.lfoRate / sampleRate;
        if (lfoPhase >= 1.0)
            lfoPhase -= 1.0;

        switch (params.lfoShape)
        {
            case LfoShape::sine:     return (float) std::sin (juce::MathConstants<double>::twoPi * p);
            case LfoShape::triangle: return (float) (1.0 - 4.0 * std::abs (p - 0.5));
            case LfoShape::saw:      return (float) (2.0 * p - 1.0);
            case LfoShape::square:   return p < 0.5 ? 1.0f : -1.0f;
        }

        return 0.0f;
    }

    float oscillator (const OscParams& osc, float position, double& phase, double phaseInc, int mipLevel) const
    {
        const auto t = phase;
        float out = 0.0f;

        switch (osc.wave)
        {
            case OscWave::sine:      out = (float) std::sin (juce::MathConstants<double>::twoPi * t); break;
            case OscWave::saw:       out = (float) (2.0 * t - 1.0 - polyBlep (t, phaseInc)); break;
            case OscWave::square:    out = (float) ((t < 0.5 ? 1.0 : -1.0) + polyBlep (t, phaseInc) - polyBlep (std::fmod (t + 0.5, 1.0), phaseInc)); break;
            case OscWave::wavetable: out = wavetable.sample (mipLevel, position, t); break;
        }

        phase += phaseInc;
        if (phase >= 1.0)
            phase -= 1.0;

        return out;
    }

    // Smooths the hard edges of saw/square waves to reduce aliasing (harsh high-frequency artefacts).
    static double polyBlep (double t, double dt)
    {
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0;
        }

        if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }

        return 0.0;
    }

    const Wavetable& wavetable;
    VoiceParams params;

    juce::ADSR ampEnv, filterEnv;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::Random random;

    std::array<std::array<double, maxUnison>, numOscs> phases {}, phaseIncs {};
    std::array<std::array<int, maxUnison>, numOscs> mipLevels {};
    std::array<float, maxUnison> gainsL {}, gainsR {};
    double noteFrequency = 0.0, lfoPhase = 0.0;
    float velocity = 0.0f, unisonLevel = 1.0f;
};
