#pragma once
#include <juce_core/juce_core.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

// Janja Belly - procedural chord progression engine.
// Pure melodic output: no drums, no percussive layers.

namespace janja
{
struct Note
{
    double start = 0.0;   // in beats from phrase start
    double length = 0.0;  // in beats
    int pitch = 60;
    float velocity = 0.7f;
};

struct GenParams
{
    juce::int64 seed = 1;
    int bars = 8;
    int mood = -1;        // -1 auto, 0 stabby, 1 twinkle, 2 neo-soul
    float swing = 0.60f;  // 0.5 straight .. 0.7 heavy
    float jitter = 0.3f;  // 0..1 nervous timing/velocity/dropouts
    int key = -1;         // -1 random, 0..11 = C..B
};

class ChordEngine
{
public:
    enum Mood { Stabby = 0, Twinkle = 1, NeoSoul = 2 };

    static std::vector<Note> generate (const GenParams& gp)
    {
        juce::Random rnd (gp.seed);
        std::vector<Note> out;

        const int bars = juce::jlimit (1, 32, gp.bars);
        const double total = bars * 4.0;
        const int mood = gp.mood >= 0 ? gp.mood : pickMood (rnd);
        const int randomKey = rnd.nextInt (12);
        const int key = gp.key >= 0 ? gp.key : randomKey;
        const auto& prog = progressions()[(size_t) rnd.nextInt ((int) progressions().size())];
        const int moodB = (mood + 1 + rnd.nextInt (2)) % 3;
        const bool hybrid = gp.mood < 0 && rnd.nextFloat() < 0.35f; // auto mode can switch feel in the last quarter
        const int chordBars = (mood == NeoSoul && rnd.nextFloat() < 0.45f) ? 2 : 1;
        const int stabTemplate = rnd.nextInt (4);
        const int arpMode = rnd.nextInt (3);
        const bool arp16 = rnd.nextBool();
        const float jit = gp.jitter;
        const double swingOff = (gp.swing - 0.5) * 0.5; // delay on odd 16ths, in beats

        auto add = [&] (int pitch, double start, double len, float vel, int step)
        {
            double t = start + ((step & 1) ? swingOff : 0.0);
            t += (double) (rnd.nextFloat() - 0.5f) * (0.008 + (double) jit * 0.03);
            t = std::max (0.0, t);
            if (t >= total) return;
            const float v = vel + (rnd.nextFloat() - 0.5f) * jit * 0.3f;
            out.push_back ({ t, std::min (len, total - t), pitch, juce::jlimit (0.15f, 1.0f, v) });
        };

        std::vector<int> prev;
        int arpIdx = 0;

        for (int bar = 0; bar < bars; ++bar)
        {
            const int slot = bar / chordBars;
            const bool lastBlock = (bar + chordBars >= bars);
            Chord c = prog[(size_t) (slot % 4)];
            if (lastBlock && slot >= 4 && rnd.nextFloat() < 0.6f)
                c = { 7, rnd.nextBool() ? 8 : 3 }; // turnaround on the dominant
            else if (slot >= 4 && (c.type == 3 || c.type == 8) && rnd.nextFloat() < 0.25f)
                c = { (c.root + 6) % 12, 3 };      // tritone sub on repeats

            const Voicing v = voice ((key + c.root) % 12, types()[(size_t) c.type], prev,
                                     mood == NeoSoul ? 5 : 4, rnd);
            prev = v.upper;

            const double bs = bar * 4.0;
            const int barMood = (hybrid && bar >= bars - std::max (1, bars / 4)) ? moodB : mood;
            const bool chordStart = (bar % chordBars) == 0;

            if (barMood == Stabby)
            {
                static const int tpl[4][5] = { { 0, 3, 6, 10, 14 }, { 0, 3, 7, 10, 13 },
                                               { 2, 5, 8, 11, 14 }, { 0, 3, 6, 9, 12 } };
                if (rnd.nextFloat() < 0.5f)
                    add (v.bass, bs, 0.4, 0.6f, 0);

                for (int k = 0; k < 5; ++k)
                {
                    if (k > 0 && rnd.nextFloat() < 0.12f + jit * 0.2f) continue;
                    const int step = tpl[stabTemplate][k];
                    const double len = 0.16 + (double) rnd.nextFloat() * 0.14;
                    const float vel = k == 0 ? 0.85f : 0.62f;
                    for (size_t i = 0; i < v.upper.size(); ++i)
                        add (v.upper[i], bs + step * 0.25 + (double) i * 0.006, len, vel, step);
                }
            }
            else if (barMood == Twinkle)
            {
                if (chordStart)
                    add (v.bass, bs, 3.0, 0.5f, -2);

                for (int step = 0; step < 16; step += (arp16 ? 1 : 2))
                {
                    if (rnd.nextFloat() < 0.12f + jit * 0.15f) { ++arpIdx; continue; }
                    const int n = (int) v.upper.size();
                    int idx;
                    if (arpMode == 0) idx = arpIdx % n;
                    else if (arpMode == 1) { const int period = 2 * n - 2; const int k = arpIdx % period; idx = k < n ? k : period - k; }
                    else idx = rnd.nextInt (n);
                    ++arpIdx;
                    int pitch = v.upper[(size_t) idx] + 12;
                    while (pitch > 96) pitch -= 12;
                    add (pitch, bs + step * 0.25, 0.55, 0.45f + rnd.nextFloat() * 0.25f, step);
                }
            }
            else // Neo-soul: long, smooth, lazy
            {
                if (! chordStart) continue;
                const bool hit2 = (chordBars == 1) && rnd.nextFloat() < 0.5f;
                const double hold = hit2 ? 2.7 : 3.9 * chordBars;
                add (v.bass, bs, hold, 0.55f, -2);
                for (size_t i = 0; i < v.upper.size(); ++i)
                {
                    const float vel = 0.62f - 0.04f * (float) i + rnd.nextFloat() * 0.08f;
                    add (v.upper[i], bs + (double) i * 0.02, hold, vel, -2);
                    if (hit2)
                        add (v.upper[i], bs + 2.75 + (double) i * 0.02, 1.1, vel * 0.85f, 11);
                }
            }
        }

        std::sort (out.begin(), out.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
        return out;
    }

private:
    struct Chord { int root; int type; };
    struct Voicing { int bass; std::vector<int> upper; };

    static int pickMood (juce::Random& r)
    {
        const float x = r.nextFloat();
        return x < 0.35f ? Stabby : (x < 0.65f ? Twinkle : NeoSoul);
    }

    // 0 m9, 1 m11, 2 maj9, 3 dom13, 4 m7, 5 maj7#11, 6 m7sus, 7 m7b5, 8 dom7alt, 9 m6/9,
    // 10 maj7, 11 minMaj9, 12 dim7, 13 maj13
    static const std::vector<std::vector<int>>& types()
    {
        static const std::vector<std::vector<int>> t = {
            { 0, 3, 7, 10, 14 }, { 0, 3, 7, 10, 14, 17 }, { 0, 4, 7, 11, 14 },
            { 0, 4, 10, 14, 21 }, { 0, 3, 7, 10 },        { 0, 4, 7, 11, 18 },
            { 0, 5, 7, 10, 14 },  { 0, 3, 6, 10 },        { 0, 4, 10, 13, 20 },
            { 0, 3, 7, 9, 14 },   { 0, 4, 7, 11 },     { 0, 3, 7, 11, 14 },
            { 0, 3, 6, 9 },       { 0, 4, 11, 14, 21 } };
        return t;
    }

    static const std::vector<std::array<Chord, 4>>& progressions()
    {
        static const std::vector<std::array<Chord, 4>> p = {
            std::array<Chord, 4> {{ { 2, 0 }, { 7, 3 }, { 0, 2 }, { 9, 1 } }},   // ii V I vi
            std::array<Chord, 4> {{ { 0, 0 }, { 10, 3 }, { 8, 2 }, { 7, 8 } }},  // i bVII bVI V
            std::array<Chord, 4> {{ { 0, 2 }, { 4, 4 }, { 9, 0 }, { 2, 1 } }},   // I iii vi ii
            std::array<Chord, 4> {{ { 0, 0 }, { 5, 1 }, { 8, 5 }, { 7, 8 } }},   // i iv bVI V
            std::array<Chord, 4> {{ { 5, 2 }, { 4, 4 }, { 9, 0 }, { 2, 6 } }},   // IV iii vi ii
            std::array<Chord, 4> {{ { 0, 1 }, { 3, 2 }, { 5, 1 }, { 7, 3 } }},   // i bIII iv V
            std::array<Chord, 4> {{ { 2, 7 }, { 7, 8 }, { 0, 0 }, { 5, 1 } }},   // iiø V i iv
            std::array<Chord, 4> {{ { 0, 9 }, { 8, 5 }, { 5, 0 }, { 10, 3 } }},  // i6/9 bVI iv bVII
            std::array<Chord, 4> {{ { 0, 2 }, { 9, 0 }, { 2, 0 }, { 7, 3 } }},   // I vi ii V
            std::array<Chord, 4> {{ { 9, 0 }, { 2, 0 }, { 7, 8 }, { 0, 13 } }},  // vi ii V I13
            std::array<Chord, 4> {{ { 0, 11 }, { 8, 2 }, { 5, 0 }, { 7, 8 } }},  // i(maj9) bVI iv V
            std::array<Chord, 4> {{ { 3, 2 }, { 8, 2 }, { 10, 3 }, { 0, 0 } }},  // bIII bVI bVII i
            std::array<Chord, 4> {{ { 0, 0 }, { 9, 7 }, { 2, 0 }, { 7, 8 } }},   // i vi-o ii V
            std::array<Chord, 4> {{ { 5, 5 }, { 0, 2 }, { 5, 1 }, { 4, 4 } }},   // IV#11 I iv iii
            std::array<Chord, 4> {{ { 0, 10 }, { 1, 12 }, { 2, 4 }, { 7, 3 } }}, // Imaj7 #I-o ii V13
            std::array<Chord, 4> {{ { 0, 9 }, { 5, 1 }, { 7, 6 }, { 0, 0 } }}    // i6/9 iv V7sus i
        };
        return p;
    }

    // Tight voice-leading: pick the inversion closest to the previous voicing.
    static Voicing voice (int rootPc, const std::vector<int>& iv, const std::vector<int>& prev,
                          int maxNotes, juce::Random& rnd)
    {
        std::vector<int> tones;
        for (int t : iv)
            if (! (iv.size() > 4 && t == 7)) tones.push_back (t); // drop 5th when extensions exist
        if ((int) tones.size() > maxNotes) tones.erase (tones.begin()); // drop root (bass covers it)
        while ((int) tones.size() > maxNotes) tones.pop_back();

        std::vector<int> base;
        for (int t : tones)
        {
            int p = (rootPc + t) % 12;
            while (p < 55) p += 12;
            base.push_back (p);
        }
        std::sort (base.begin(), base.end());

        double prevCentroid = 66.0;
        if (! prev.empty())
        {
            prevCentroid = 0;
            for (int p : prev) prevCentroid += p;
            prevCentroid /= (double) prev.size();
        }

        std::vector<int> best = base;
        double bestScore = 1e9;
        std::vector<int> cand = base;
        for (size_t k = 0; k < base.size(); ++k)
        {
            double c = 0;
            for (int p : cand) c += p;
            c /= (double) cand.size();
            double score = std::abs (c - prevCentroid);
            if (cand.size() == prev.size())
                for (size_t i = 0; i < cand.size(); ++i)
                    score += 0.3 * std::abs (cand[i] - prev[i]);
            score += (double) rnd.nextFloat() * 1.5;
            if (cand.back() > 84) score += 50;
            if (score < bestScore) { bestScore = score; best = cand; }

            cand.front() += 12; // next inversion
            std::sort (cand.begin(), cand.end());
        }

        int bass = rootPc;
        while (bass < 38) bass += 12;
        return { bass, best };
    }
};
} // namespace janja
