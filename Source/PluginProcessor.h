#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <vector>
#include "ChordEngine.h"
#include "SynthVoice.h"
#include "Ensemble.h"

class JanjaBellyProcessor : public juce::AudioProcessor
{
public:
    JanjaBellyProcessor();
    ~JanjaBellyProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Janja Belly"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // UI actions
    void generatePhrase();                       // GENERATE button
    void generateFromSeed (juce::int64 seed);
    void spawnSound();                           // SPAWN button
    bool writeMidiFile (const juce::File& f) const;
    std::vector<janja::Note> getPhraseCopy() const;
    int getPhraseBars() const;
    juce::int64 getSeed() const { return currentSeed.load(); }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<bool> previewing { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setParam (const juce::String& id, float plainValue);

    juce::Synthesiser synth;
    juce::dsp::Chorus<float> chorus;
    StringEnsemble ensemble;
    juce::dsp::Reverb reverb;

    mutable juce::SpinLock phraseLock;
    std::vector<janja::Note> phrase;
    int phraseBars = 8;
    std::atomic<juce::int64> currentSeed { 0 };
    std::atomic<bool> panic { false };
    std::atomic<double> lastBpm { 140.0 };

    double internalBeat = 0.0;
    bool wasRunning = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JanjaBellyProcessor)
};
