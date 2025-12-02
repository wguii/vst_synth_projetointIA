/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "SynthVoice.h"
#include "SynthSound.h"
#include "Data/MeterData.h"

//==============================================================================
/**
*/
class TapSynthAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    TapSynthAudioProcessor();
    ~TapSynthAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    const std::atomic<float>& getRMS() { return meter.getRMS(); }
    const std::atomic<float>& getPeak() { return meter.getPeak(); }
    juce::AudioProcessorValueTreeState apvts;

    void sendPromptToAI(const juce::String& prompt);
    void updateParametersFromJson(const juce::String& jsonString);

    std::function<void(bool success, const juce::String& message)> onStatusChange;


private:
    static constexpr int numChannelsToProcess { 2 };
    juce::Synthesiser synth;
    
    juce::AudioProcessorValueTreeState::ParameterLayout createParams();
    void setParams();
    void setVoiceParams();
    void setFilterParams();
    void setReverbParams();
    
    static constexpr int numVoices { 5 };
    juce::dsp::Reverb reverb;
    juce::Reverb::Parameters reverbParams;
    MeterData meter;

    juce::String getSystemPrompt();
    juce::String extractJsonFromResponse(const juce::String& response);

    juce::ThreadPool pool{ 1 };

    const juce::String apiKey = "test";
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapSynthAudioProcessor)
};
