#pragma once

#include <juce_core/juce_core.h>

// A built-in morphing wavetable: sine -> triangle -> saw -> square -> pulse -> formant.
//
// Each frame is built additively from harmonics, once per "mip level". Higher levels keep fewer
// harmonics, so high notes can read a level whose harmonics all stay below Nyquist (no aliasing).
class Wavetable
{
public:
    static constexpr int tableSize = 2048;
    static constexpr int numFrames = 16;
    static constexpr int numLevels = 10;       // level l holds harmonics 1 .. (maxHarmonics >> l)
    static constexpr int maxHarmonics = 512;

    Wavetable()
    {
        data.resize ((size_t) (numLevels * numFrames * tableSize));
        build();
    }

    // The most detailed mip level whose top harmonic stays below Nyquist at this pitch.
    static int levelFor (double phaseInc)
    {
        int level = 0;
        while (level < numLevels - 1 && (double) (maxHarmonics >> level) * phaseInc >= 0.5)
            ++level;
        return level;
    }

    // position 0..1 morphs across the frames; phase 0..1 is the point within the cycle.
    float sample (int level, float position, double phase) const
    {
        const auto framePos = juce::jlimit (0.0f, 1.0f, position) * (float) (numFrames - 1);
        const auto frame = juce::jmin ((int) framePos, numFrames - 2);
        const auto frac = framePos - (float) frame;
        const auto a = read (level, frame, phase);
        const auto b = read (level, frame + 1, phase);
        return a + frac * (b - a);
    }

private:
    float* table (int level, int frame)
    {
        return data.data() + (size_t) ((level * numFrames + frame) * tableSize);
    }

    const float* table (int level, int frame) const
    {
        return data.data() + (size_t) ((level * numFrames + frame) * tableSize);
    }

    float read (int level, int frame, double phase) const
    {
        const auto* t = table (level, frame);
        const auto pos = phase * tableSize;
        const auto i = (int) pos;
        const auto frac = (float) (pos - i);
        const auto a = t[i & (tableSize - 1)];
        const auto b = t[(i + 1) & (tableSize - 1)];
        return a + frac * (b - a);
    }

    // Harmonic amplitudes of the six shapes the table morphs between.
    static float keyframeHarmonic (int key, int n)
    {
        const auto fn = (float) n;
        const bool odd = (n % 2) == 1;

        switch (key)
        {
            case 0:  return n == 1 ? 1.0f : 0.0f;                                                   // sine
            case 1:  return odd ? (((n - 1) / 2) % 2 == 0 ? 1.0f : -1.0f) / (fn * fn) : 0.0f;       // triangle
            case 2:  return 1.0f / fn;                                                              // saw
            case 3:  return odd ? 1.0f / fn : 0.0f;                                                 // square
            case 4:  return std::sin (juce::MathConstants<float>::pi * fn * 0.2f) / fn;            // 20% pulse
            default: return (n == 1 ? 0.5f : 0.0f) + std::exp (-(fn - 12.0f) * (fn - 12.0f) / 18.0f); // formant bump
        }
    }

    void build()
    {
        constexpr int numKeyframes = 6;

        std::vector<float> sine ((size_t) tableSize);
        for (int i = 0; i < tableSize; ++i)
            sine[(size_t) i] = (float) std::sin (juce::MathConstants<double>::twoPi * i / tableSize);

        std::vector<float> amps ((size_t) maxHarmonics + 1);

        for (int frame = 0; frame < numFrames; ++frame)
        {
            const auto keyPos = (float) frame / (float) (numFrames - 1) * (float) (numKeyframes - 1);
            const auto lo = juce::jmin ((int) keyPos, numKeyframes - 2);
            const auto frac = keyPos - (float) lo;

            for (int n = 1; n <= maxHarmonics; ++n)
                amps[(size_t) n] = juce::jmap (frac, keyframeHarmonic (lo, n), keyframeHarmonic (lo + 1, n));

            float normalise = 1.0f;

            for (int level = 0; level < numLevels; ++level)
            {
                auto* t = table (level, frame);
                const int topHarmonic = maxHarmonics >> level;

                for (int n = 1; n <= topHarmonic; ++n)
                {
                    const auto amp = amps[(size_t) n];
                    if (std::abs (amp) < 1.0e-6f)
                        continue;

                    for (int i = 0; i < tableSize; ++i)
                        t[i] += amp * sine[(size_t) ((n * i) & (tableSize - 1))];
                }

                // Normalise every level by the full-bandwidth peak so loudness doesn't jump between levels.
                if (level == 0)
                {
                    float peak = 0.0f;
                    for (int i = 0; i < tableSize; ++i)
                        peak = juce::jmax (peak, std::abs (t[i]));
                    normalise = peak > 0.0f ? 1.0f / peak : 1.0f;
                }

                juce::FloatVectorOperations::multiply (t, normalise, tableSize);
            }
        }
    }

    std::vector<float> data;
};
