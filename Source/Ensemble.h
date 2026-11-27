#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>
#include <vector>

// Solina-style string ensemble: three modulated delay lines 120 degrees apart,
// each moved by a slow swell plus a faster shimmer. Stereo spread comes from
// per-voice panning and an offset between left and right LFO phases.
class StringEnsemble
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        size = (int) (sr * 0.06) + 8;
        for (auto& b : buf) b.assign ((size_t) size, 0.0f);
        pos = 0; slow = 0.0; fast = 0.0;
    }

    void setMix (float m) { mix = juce::jlimit (0.0f, 1.0f, m); }

    void process (juce::AudioBuffer<float>& b)
    {
        constexpr double twoPi = 6.283185307179586;
        const int n = b.getNumSamples();
        const int chans = std::min (2, b.getNumChannels());
        static const float wL[3] = { 1.0f, 0.6f, 0.25f };
        static const float wR[3] = { 0.25f, 0.6f, 1.0f };

        for (int i = 0; i < n; ++i)
        {
            slow += twoPi * 0.55 / sr; if (slow > twoPi) slow -= twoPi;
            fast += twoPi * 5.8 / sr;  if (fast > twoPi) fast -= twoPi;

            for (int ch = 0; ch < chans; ++ch)
            {
                auto& line = buf[(size_t) ch];
                const float in = b.getSample (ch, i);
                line[(size_t) pos] = in;

                float wet = 0.0f;
                for (int v = 0; v < 3; ++v)
                {
                    const double ph = v * twoPi / 3.0 + ch * twoPi / 6.0;
                    const double mod = 0.8 * std::sin (slow + ph) + 0.2 * std::sin (fast + ph);
                    const double d = (9.0 + 4.0 * mod) * sr / 1000.0;
                    double rp = (double) pos - d;
                    if (rp < 0) rp += size;
                    const int i0 = (int) rp;
                    const int i1 = (i0 + 1) % size;
                    const float fr = (float) (rp - i0);
                    const float s = line[(size_t) i0] * (1.0f - fr) + line[(size_t) i1] * fr;
                    wet += s * (ch == 0 ? wL[v] : wR[v]);
                }
                wet *= 1.1f / 1.85f;
                b.setSample (ch, i, in * (1.0f - 0.4f * mix) + wet * mix);
            }
            pos = (pos + 1) % size;
        }
    }

private:
    double sr = 44100.0, slow = 0.0, fast = 0.0;
    int size = 4096, pos = 0;
    float mix = 0.0f;
    std::vector<float> buf[2];
};
