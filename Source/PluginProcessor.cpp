#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const juce::StringArray kBars   { "4 bars", "8 bars", "12 bars", "16 bars" };
const juce::StringArray kMoods  { "Auto", "Stabby", "Twinkle", "Neo-soul" };
const juce::StringArray kSounds { "Dusty Keys", "Wet Pad", "Glass Twinkle" };
const juce::StringArray kKeys   { "Key: Any", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

juce::AudioProcessorValueTreeState::ParameterLayout JanjaBellyProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto f01 = [] (const char* id, const char* name, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                      NormalisableRange<float> (0.0f, 1.0f, 0.001f), def);
    };

    layout.add (f01 ("reverb",   "Reverb",   0.25f));
    layout.add (f01 ("chorus",   "Chorus",   0.30f));
    layout.add (f01 ("ensemble", "Ensemble", 0.20f));
    layout.add (f01 ("jitter",   "Jitter",   0.30f));
    layout.add (f01 ("bright",   "Tone",     0.50f));
    layout.add (f01 ("attack",   "Attack",   0.30f));
    layout.add (f01 ("wobble",   "Wobble",   0.35f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "swing", 1 }, "Swing",
                                                       NormalisableRange<float> (0.5f, 0.7f, 0.001f), 0.60f));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "bars", 1 },  "Bars",  kBars, 1));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mood", 1 },  "Mood",  kMoods, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "sound", 1 }, "Sound", kSounds, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 },   "Key",   kKeys, 0));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "halftime", 1 },  "Half-time", false));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "locksound", 1 }, "Lock sound", false));
    return layout;
}

JanjaBellyProcessor::JanjaBellyProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    synth.addSound (new SynthSound());
    VoiceParams vp { apvts.getRawParameterValue ("sound"), apvts.getRawParameterValue ("bright"),
                     apvts.getRawParameterValue ("attack"), apvts.getRawParameterValue ("wobble") };
    for (int i = 0; i < 12; ++i)
        synth.addVoice (new SynthVoice (vp));

    generatePhrase();
}

bool JanjaBellyProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void JanjaBellyProcessor::prepareToPlay (double sr, int bs)
{
    synth.setCurrentPlaybackSampleRate (sr);
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };

    chorus.prepare (spec);
    chorus.setCentreDelay (7.0f);
    chorus.setRate (0.9f);
    chorus.setDepth (0.25f);
    chorus.setFeedback (0.0f);

    ensemble.prepare (sr);   // 3-phase string-machine ensemble

    reverb.prepare (spec);
    internalBeat = 0.0;
}

void JanjaBellyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int n = buffer.getNumSamples();
    const double sr = getSampleRate();

    // --- transport ---
    double bpm = lastBpm.load(), ppq = 0.0;
    bool hostPlaying = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) { bpm = *b; lastBpm = bpm; }
            if (auto p = pos->getPpqPosition()) ppq = *p;
            hostPlaying = pos->getIsPlaying();
        }

    const bool running = hostPlaying || previewing.load();
    const double ratio = apvts.getRawParameterValue ("halftime")->load() > 0.5f ? 0.5 : 1.0;
    const double beatsPerSample = bpm * ratio / (60.0 * sr);
    const double blockBeats = beatsPerSample * n;

    if (panic.exchange (false) || (wasRunning && ! running))
        synth.allNotesOff (0, true);
    wasRunning = running;

    juce::MidiBuffer events;

    if (running)
    {
        const double startBeat = hostPlaying ? ppq * ratio : internalBeat;

        juce::SpinLock::ScopedTryLockType lock (phraseLock);
        if (lock.isLocked() && ! phrase.empty())
        {
            const double L = phraseBars * 4.0;
            double b0 = std::fmod (startBeat, L);
            if (b0 < 0) b0 += L;

            auto offsetFor = [&] (double t) -> int
            {
                for (double wrap : { 0.0, L })
                {
                    const double tt = t + wrap;
                    if (tt >= b0 && tt < b0 + blockBeats)
                        return juce::jlimit (0, n - 1, (int) ((tt - b0) / beatsPerSample));
                }
                return -1;
            };

            for (const auto& note : phrase)   // note-offs first so retriggers are clean
            {
                const int o = offsetFor (std::fmod (note.start + note.length, L));
                if (o >= 0) events.addEvent (juce::MidiMessage::noteOff (1, note.pitch), o);
            }
            for (const auto& note : phrase)
            {
                const int o = offsetFor (note.start);
                if (o >= 0) events.addEvent (juce::MidiMessage::noteOn (1, note.pitch, note.velocity), o);
            }
        }
        if (! hostPlaying)
            internalBeat += blockBeats;
    }
    else
        internalBeat = 0.0;

    synth.renderNextBlock (buffer, events, 0, n);

    // --- FX: chorus -> ensemble -> reverb ---
    chorus.setMix (apvts.getRawParameterValue ("chorus")->load());
    ensemble.setMix (apvts.getRawParameterValue ("ensemble")->load());
    const float rev = apvts.getRawParameterValue ("reverb")->load();
    juce::dsp::Reverb::Parameters rp;
    rp.roomSize = 0.78f; rp.damping = 0.55f; rp.width = 1.0f; rp.freezeMode = 0.0f;
    rp.wetLevel = rev * 0.45f; rp.dryLevel = 1.0f - rev * 0.2f;
    reverb.setParameters (rp);

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    chorus.process (ctx);
    ensemble.process (buffer);
    reverb.process (ctx);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)   // gentle master soft-clip
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i) d[i] = std::tanh (d[i] * 1.1f) * 0.95f;
    }
}

// ---------------------------------------------------------------- generation

void JanjaBellyProcessor::generatePhrase()
{
    generateFromSeed (juce::Random::getSystemRandom().nextInt64());
}

void JanjaBellyProcessor::generateFromSeed (juce::int64 seed)
{
    janja::GenParams gp;
    gp.seed = seed;
    gp.bars = 4 * ((int) apvts.getRawParameterValue ("bars")->load() + 1);
    gp.mood = (int) apvts.getRawParameterValue ("mood")->load() - 1;
    gp.swing = apvts.getRawParameterValue ("swing")->load();
    gp.jitter = apvts.getRawParameterValue ("jitter")->load();
    gp.key = (int) apvts.getRawParameterValue ("key")->load() - 1;

    auto notes = janja::ChordEngine::generate (gp);
    {
        juce::SpinLock::ScopedLockType l (phraseLock);
        phrase = std::move (notes);
        phraseBars = gp.bars;
    }
    currentSeed = seed;
    panic = true;
}

void JanjaBellyProcessor::setParam (const juce::String& id, float plain)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
}

void JanjaBellyProcessor::spawnSound()
{
    auto& r = juce::Random::getSystemRandom();
    if (apvts.getRawParameterValue ("locksound")->load() < 0.5f)
        setParam ("sound", (float) r.nextInt (kSounds.size()));
    setParam ("bright", 0.2f + 0.1f * (float) r.nextInt (8));   // 8 steps
    setParam ("attack", 0.1f * (float) r.nextInt (9));          // 9 steps
    setParam ("wobble", 0.1f * (float) r.nextInt (8));          // 8 steps
}

std::vector<janja::Note> JanjaBellyProcessor::getPhraseCopy() const
{
    juce::SpinLock::ScopedLockType l (phraseLock);
    return phrase;
}

int JanjaBellyProcessor::getPhraseBars() const
{
    juce::SpinLock::ScopedLockType l (phraseLock);
    return phraseBars;
}

bool JanjaBellyProcessor::writeMidiFile (const juce::File& file) const
{
    constexpr int ppq = 960;
    juce::MidiMessageSequence seq;
    const int bpm = juce::jlimit (60, 200, (int) lastBpm.load());
    seq.addEvent (juce::MidiMessage::tempoMetaEvent (60000000 / bpm), 0.0);

    for (const auto& n : getPhraseCopy())
    {
        seq.addEvent (juce::MidiMessage::noteOn (1, n.pitch, n.velocity).withTimeStamp (n.start * ppq));
        seq.addEvent (juce::MidiMessage::noteOff (1, n.pitch).withTimeStamp ((n.start + n.length) * ppq));
    }
    seq.updateMatchedPairs();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (ppq);
    mf.addTrack (seq);

    file.deleteFile();
    juce::FileOutputStream os (file);
    return os.openedOk() && mf.writeTo (os);
}

// ---------------------------------------------------------------- state / editor

void JanjaBellyProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("seed", currentSeed.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void JanjaBellyProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (vt);
            if (vt.hasProperty ("seed"))
                generateFromSeed ((juce::int64) vt.getProperty ("seed"));
        }
}

juce::AudioProcessorEditor* JanjaBellyProcessor::createEditor()
{
    return new JanjaBellyEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JanjaBellyProcessor();
}
