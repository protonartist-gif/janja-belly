#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>

// 0 Dusty Keys  - FM Rhodes: velocity-sensitive tine, tremolo, drift, warm low-pass, sub
// 1 Wet Pad     - 5 detuned saws + PWM square + sub, 2-pole drifting filter with jittery S&H
// 2 Glass Twinkle - two FM bell stacks plus beating carrier pair for shimmer

struct SynthSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

struct VoiceParams
{
    std::atomic<float>* sound = nullptr;
    std::atomic<float>* bright = nullptr;
    std::atomic<float>* attack = nullptr;
    std::atomic<float>* wobble = nullptr;
};

class SynthVoice : public juce::SynthesiserVoice
{
public:
    explicit SynthVoice (const VoiceParams& p) : vp (p) {}

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<SynthSound*> (s) != nullptr; }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        const double sr = getSampleRate();
        patch = juce::jlimit (0, 2, (int) vp.sound->load());
        bright = vp.bright->load();
        const float atk = vp.attack->load();
        wob = vp.wobble->load();
        level = velocity;
        freq = juce::MidiMessage::getMidiNoteInHertz (midiNote) * (1.0 + (rnd.nextDouble() - 0.5) * 0.0035);

        phC = phM = phM2 = phB = phSub = wobPh = tremPh = 0.0;
        slowPh = rnd.nextDouble() * twoPi;
        pwmPh = rnd.nextDouble() * twoPi;
        for (auto& p : phS) p = rnd.nextDouble();
        lp1 = lp2 = 0.0f;
        modEnv = modEnv2 = bellEnv = 1.0;
        shCount = 0; shTarget = shCur = 0.5f;
        shSmooth = (float) (1.0 - std::exp (-1.0 / (0.03 * sr)));

        juce::ADSR::Parameters ap;
        if (patch == 0)
        {
            ap = { 0.003f + atk * 0.04f, 1.8f, 0.25f, 0.35f };
            dMod = std::exp (-1.0 / (0.6 * sr));
        }
        else if (patch == 1)
        {
            ap = { 0.15f + atk * 1.2f, 1.0f, 0.8f, 1.4f };
        }
        else
        {
            ap = { 0.002f + atk * 0.02f, 1.2f, 0.05f, 0.5f };
            dMod = std::exp (-1.0 / (0.35 * sr));
            dMod2 = std::exp (-1.0 / (0.14 * sr));
        }
        dBell = std::exp (-1.0 / (0.08 * sr));

        env.setSampleRate (sr);
        env.setParameters (ap);
        env.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff) env.noteOff();
        else { env.reset(); clearCurrentNote(); }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& out, int startSample, int numSamples) override
    {
        if (! env.isActive()) { clearCurrentNote(); return; }

        const double sr = getSampleRate();
        const float amp = level * 0.22f;
        const double maxCut = sr * 0.42;

        for (int i = 0; i < numSamples; ++i)
        {
            adv (wobPh, 0.8, sr);
            const double f = freq * (1.0 + (double) wob * 0.004 * std::sin (wobPh));
            float s = 0.0f;
            float tremolo = 1.0f;

            if (patch == 0) // ---- Dusty Keys
            {
                modEnv *= dMod; bellEnv *= dBell;
                adv (phC, f, sr); adv (phM, f, sr); adv (phB, f * 4.0, sr); adv (phSub, f * 0.5, sr);
                adv (tremPh, 4.2, sr);
                tremolo = 1.0f + 0.15f * wob * (float) std::sin (tremPh);

                const float idx = (0.5f + 2.3f * bright) * (float) modEnv * (0.4f + 1.1f * level);
                s = std::sin ((float) phC + idx * std::sin ((float) phM)) * 0.85f
                    + std::sin ((float) phB) * 0.12f * bright * level * (float) bellEnv
                    + std::sin ((float) phSub) * 0.10f;

                const double cutoff = std::min (maxCut, 1800.0 + 9000.0 * (double) bright);
                const float a = (float) (1.0 - std::exp (-twoPi * cutoff / sr));
                lp1 += a * (s - lp1);
                s = lp1 * (1.0f + level * 0.5f);
            }
            else if (patch == 1) // ---- Wet Pad
            {
                const double d = 0.003 + (double) wob * 0.004;
                const double rat[5] = { 1.0 - 2 * d, 1.0 - d, 1.0, 1.0 + d, 1.0 + 2 * d };
                for (int k = 0; k < 5; ++k)
                {
                    phS[k] += f * rat[k] / sr;
                    if (phS[k] >= 1.0) phS[k] -= 1.0;
                    s += (float) (2.0 * phS[k] - 1.0);
                }
                s *= 0.14f;

                adv (pwmPh, 0.23, sr);
                const double pw = 0.5 + 0.35 * std::sin (pwmPh);
                phSq += f * 0.5 / sr; if (phSq >= 1.0) phSq -= 1.0;
                s += (phSq < pw ? 0.12f : -0.12f);

                adv (phSub, f * 0.5, sr);
                s += std::sin ((float) phSub) * 0.12f;

                adv (slowPh, 0.15, sr);
                if (--shCount <= 0)
                {
                    shCount = (int) (sr * (0.06 + rnd.nextDouble() * 0.18));
                    shTarget = rnd.nextFloat();
                }
                shCur += shSmooth * (shTarget - shCur);

                const double cutoff = std::min (maxCut, 300.0 + 3600.0 * (double) (bright * bright)
                                                         + 450.0 * std::sin (slowPh)
                                                         + 1200.0 * (double) (wob * shCur));
                const float a = (float) (1.0 - std::exp (-twoPi * std::max (80.0, cutoff) / sr));
                lp1 += a * (s - lp1);
                lp2 += a * (lp1 - lp2);
                s = lp2 * 1.4f;
            }
            else // ---- Glass Twinkle
            {
                modEnv *= dMod; modEnv2 *= dMod2;
                adv (phC, f, sr); adv (phM, f * 3.5, sr); adv (phM2, f * 7.01, sr); adv (phB, f * 1.004, sr);
                const float v = 0.4f + 1.1f * level;
                const float idx1 = (1.0f + 3.0f * bright) * (float) modEnv * v;
                const float idx2 = (0.5f + 1.5f * bright) * (float) modEnv2 * v;
                const float mod = idx1 * std::sin ((float) phM) + idx2 * std::sin ((float) phM2);
                s = (std::sin ((float) phC + mod) + std::sin ((float) phB + mod)) * 0.42f
                    + std::sin (2.0f * (float) phC) * 0.08f * (float) modEnv;
            }

            s = std::tanh (s * 1.15f) * 0.9f;
            s *= env.getNextSample() * amp * tremolo;
            for (int ch = 0; ch < out.getNumChannels(); ++ch)
                out.addSample (ch, startSample + i, s);

            if (! env.isActive()) { clearCurrentNote(); break; }
        }
    }

private:
    static constexpr double twoPi = 6.283185307179586;

    static void adv (double& ph, double f, double sr)
    {
        ph += twoPi * f / sr;
        if (ph > twoPi) ph -= twoPi;
    }

    VoiceParams vp;
    juce::ADSR env;
    juce::Random rnd;
    int patch = 0, shCount = 0;
    float bright = 0.5f, wob = 0.3f, level = 0.8f, lp1 = 0.0f, lp2 = 0.0f;
    float shTarget = 0.5f, shCur = 0.5f, shSmooth = 0.001f;
    double freq = 440.0, phC = 0, phM = 0, phM2 = 0, phB = 0, phSub = 0, phSq = 0;
    double wobPh = 0, slowPh = 0, pwmPh = 0, tremPh = 0;
    double phS[5] = { 0, 0, 0, 0, 0 };
    double modEnv = 1.0, modEnv2 = 1.0, bellEnv = 1.0, dMod = 0.9999, dMod2 = 0.999, dBell = 0.999;
};
