/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>

//==============================================================================
TapSynthAudioProcessor::TapSynthAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), apvts (*this, nullptr, "Parameters", createParams())
#endif
{
    synth.addSound (new SynthSound());
    
    for (int i = 0; i < 5; i++)
    {
        synth.addVoice (new SynthVoice());
    }
}

TapSynthAudioProcessor::~TapSynthAudioProcessor()
{
    
}

//==============================================================================
const juce::String TapSynthAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool TapSynthAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool TapSynthAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool TapSynthAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double TapSynthAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int TapSynthAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int TapSynthAudioProcessor::getCurrentProgram()
{
    return 0;
}

void TapSynthAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String TapSynthAudioProcessor::getProgramName (int index)
{
    return {};
}

void TapSynthAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void TapSynthAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    
    for (int i = 0; i < synth.getNumVoices(); i++)
    {
        if (auto voice = dynamic_cast<SynthVoice*>(synth.getVoice(i)))
        {
            voice->prepareToPlay (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
        }
    }
    
    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlock;
    spec.sampleRate = sampleRate;
    spec.numChannels = getTotalNumOutputChannels();
    
    reverbParams.roomSize = 0.5f;
    reverbParams.width = 1.0f;
    reverbParams.damping = 0.5f;
    reverbParams.freezeMode = 0.0f;
    reverbParams.dryLevel = 1.0f;
    reverbParams.wetLevel = 0.0f;
    
    reverb.setParameters (reverbParams);
}

void TapSynthAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool TapSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void TapSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    setParams();
        
    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());
    juce::dsp::AudioBlock<float> block { buffer };
    reverb.process (juce::dsp::ProcessContextReplacing<float> (block));
    
    meter.processRMS (buffer);
    meter.processPeak (buffer);
}

//==============================================================================
bool TapSynthAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* TapSynthAudioProcessor::createEditor()
{
    return new TapSynthAudioProcessorEditor (*this);
}

//==============================================================================
void TapSynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void TapSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TapSynthAudioProcessor();
}

juce::AudioProcessorValueTreeState::ParameterLayout TapSynthAudioProcessor::createParams()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    // OSC select
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("OSC1", "Oscillator 1", juce::StringArray { "Sine", "Saw", "Square" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("OSC2", "Oscillator 2", juce::StringArray { "Sine", "Saw", "Square" }, 0));
    
    // OSC Gain
    params.push_back (std::make_unique<juce::AudioParameterFloat>("OSC1GAIN", "Oscillator 1 Gain", juce::NormalisableRange<float> { -40.0f, 0.2f, 0.1f }, 0.1f, "dB"));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("OSC2GAIN", "Oscillator 2 Gain", juce::NormalisableRange<float> { -40.0f, 0.2f, 0.1f }, 0.1f, "dB"));
    
    // OSC Pitch val
    params.push_back (std::make_unique<juce::AudioParameterInt>("OSC1PITCH", "Oscillator 1 Pitch", -48, 48, 0));
    params.push_back (std::make_unique<juce::AudioParameterInt>("OSC2PITCH", "Oscillator 2 Pitch", -48, 48, 0));
    
    // FM Osc Freq
    params.push_back (std::make_unique<juce::AudioParameterFloat>("OSC1FMFREQ", "Oscillator 1 FM Frequency", juce::NormalisableRange<float> { 0.0f, 1000.0f, 0.1f }, 0.0f, "Hz"));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("OSC2FMFREQ", "Oscillator 1 FM Frequency", juce::NormalisableRange<float> { 0.0f, 1000.0f, 0.1f }, 0.0f, "Hz"));
    
    // FM Osc Depth
    params.push_back (std::make_unique<juce::AudioParameterFloat>("OSC1FMDEPTH", "Oscillator 1 FM Depth", juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 0.0f, ""));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("OSC2FMDEPTH", "Oscillator 2 FM Depth", juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 0.0f, ""));
    
    // LFO
    params.push_back (std::make_unique<juce::AudioParameterFloat>("LFO1FREQ", "LFO1 Frequency", juce::NormalisableRange<float> { 0.0f, 20.0f, 0.1f }, 0.0f, "Hz"));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("LFO1DEPTH", "LFO1 Depth", juce::NormalisableRange<float> { 0.0f, 10000.0f, 0.1f, 0.3f }, 0.0f, ""));
    
    //Filter
    params.push_back (std::make_unique<juce::AudioParameterChoice>("FILTERTYPE", "Filter Type", juce::StringArray { "Low Pass", "Band Pass", "High Pass" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("FILTERCUTOFF", "Filter Cutoff", juce::NormalisableRange<float> { 20.0f, 20000.0f, 0.1f, 0.6f }, 20000.0f, "Hz"));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("FILTERRESONANCE", "Filter Resonance", juce::NormalisableRange<float> { 0.1f, 2.0f, 0.1f }, 0.1f, ""));
    
    // ADSR
    params.push_back (std::make_unique<juce::AudioParameterFloat>("ATTACK", "Attack", juce::NormalisableRange<float> { 0.1f, 1.0f, 0.1f }, 0.1f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("DECAY", "Decay", juce::NormalisableRange<float> { 0.1f, 1.0f, 0.1f }, 0.1f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("SUSTAIN", "Sustain", juce::NormalisableRange<float> { 0.1f, 1.0f, 0.1f }, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("RELEASE", "Release", juce::NormalisableRange<float> { 0.1f, 3.0f, 0.1f }, 0.4f));
    
    params.push_back (std::make_unique<juce::AudioParameterFloat>("FILTERADSRDEPTH", "Filter ADSR Depth", juce::NormalisableRange<float> { 0.0f, 10000.0f, 0.1f, 0.3f }, 10000.0f, ""));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("FILTERATTACK", "Filter Attack", juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, 0.01f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("FILTERDECAY", "Filter Decay", juce::NormalisableRange<float> { 0.0f, 1.0f, 0.1f }, 0.1f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("FILTERSUSTAIN", "Filter Sustain", juce::NormalisableRange<float> { 0.0f, 1.0f, 0.1f }, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("FILTERRELEASE", "Filter Release", juce::NormalisableRange<float> { 0.0f, 3.0f, 0.1f }, 0.1f));
    
    // Reverb
    params.push_back (std::make_unique<juce::AudioParameterFloat>("REVERBSIZE", "Reverb Size", juce::NormalisableRange<float> { 0.0f, 1.0f, 0.1f }, 0.0f, ""));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("REVERBWIDTH", "Reverb Width", juce::NormalisableRange<float> { 0.0f, 1.0f, 0.1f }, 1.0f, ""));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("REVERBDAMPING", "Reverb Damping", juce::NormalisableRange<float> { 0.0f, 1.0f, 0.1f }, 0.5f, ""));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("REVERBDRY", "Reverb Dry", juce::NormalisableRange<float> { 0.0f, 1.0f, 0.1f }, 1.0f, ""));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("REVERBWET", "Reverb Wet", juce::NormalisableRange<float> { 0.0f, 1.0f, 0.1f }, 0.0f, ""));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("REVERBFREEZE", "Reverb Freeze", juce::NormalisableRange<float> { 0.0f, 1.0f, 0.1f }, 0.0f, ""));
    
    return { params.begin(), params.end() };
}

void TapSynthAudioProcessor::setParams()
{
    setVoiceParams();
    setFilterParams();
    setReverbParams();
}

void TapSynthAudioProcessor::setVoiceParams()
{
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto voice = dynamic_cast<SynthVoice*>(synth.getVoice(i)))
        {
            auto& attack = *apvts.getRawParameterValue ("ATTACK");
            auto& decay = *apvts.getRawParameterValue ("DECAY");
            auto& sustain = *apvts.getRawParameterValue ("SUSTAIN");
            auto& release = *apvts.getRawParameterValue ("RELEASE");
            
            auto& osc1Choice = *apvts.getRawParameterValue ("OSC1");
            auto& osc2Choice = *apvts.getRawParameterValue ("OSC2");
            auto& osc1Gain = *apvts.getRawParameterValue ("OSC1GAIN");
            auto& osc2Gain = *apvts.getRawParameterValue ("OSC2GAIN");
            auto& osc1Pitch = *apvts.getRawParameterValue ("OSC1PITCH");
            auto& osc2Pitch = *apvts.getRawParameterValue ("OSC2PITCH");
            auto& osc1FmFreq = *apvts.getRawParameterValue ("OSC1FMFREQ");
            auto& osc2FmFreq = *apvts.getRawParameterValue ("OSC2FMFREQ");
            auto& osc1FmDepth = *apvts.getRawParameterValue ("OSC1FMDEPTH");
            auto& osc2FmDepth = *apvts.getRawParameterValue ("OSC2FMDEPTH");
            
            auto& filterAttack = *apvts.getRawParameterValue ("FILTERATTACK");
            auto& filterDecay = *apvts.getRawParameterValue ("FILTERDECAY");
            auto& filterSustain = *apvts.getRawParameterValue ("FILTERSUSTAIN");
            auto& filterRelease = *apvts.getRawParameterValue ("FILTERRELEASE");

            auto& osc1 = voice->getOscillator1();
            auto& osc2 = voice->getOscillator2();
            
            auto& adsr = voice->getAdsr();
            auto& filterAdsr = voice->getFilterAdsr();
           
            for (int i = 0; i < getTotalNumOutputChannels(); i++)
            {
                osc1[i].setParams (osc1Choice, osc1Gain, osc1Pitch, osc1FmFreq, osc1FmDepth);
                osc2[i].setParams (osc2Choice, osc2Gain, osc2Pitch, osc2FmFreq, osc2FmDepth);
            }
            
            adsr.update (attack.load(), decay.load(), sustain.load(), release.load());
            filterAdsr.update (filterAttack, filterDecay, filterSustain, filterRelease);
        }
    }
}

void TapSynthAudioProcessor::setFilterParams()
{
    auto& filterType = *apvts.getRawParameterValue ("FILTERTYPE");
    auto& filterCutoff = *apvts.getRawParameterValue ("FILTERCUTOFF");
    auto& filterResonance = *apvts.getRawParameterValue ("FILTERRESONANCE");
    auto& adsrDepth = *apvts.getRawParameterValue ("FILTERADSRDEPTH");
    auto& lfoFreq = *apvts.getRawParameterValue ("LFO1FREQ");
    auto& lfoDepth = *apvts.getRawParameterValue ("LFO1DEPTH");
        
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto voice = dynamic_cast<SynthVoice*>(synth.getVoice(i)))
        {
            voice->updateModParams (filterType, filterCutoff, filterResonance, adsrDepth, lfoFreq, lfoDepth);
        }
    }
}

void TapSynthAudioProcessor::setReverbParams()
{
    reverbParams.roomSize = *apvts.getRawParameterValue ("REVERBSIZE");
    reverbParams.width = *apvts.getRawParameterValue ("REVERBWIDTH");
    reverbParams.damping = *apvts.getRawParameterValue ("REVERBDAMPING");
    reverbParams.dryLevel = *apvts.getRawParameterValue ("REVERBDRY");
    reverbParams.wetLevel = *apvts.getRawParameterValue ("REVERBWET");
    reverbParams.freezeMode = *apvts.getRawParameterValue ("REVERBFREEZE");
    
    reverb.setParameters (reverbParams);
}

juce::String TapSynthAudioProcessor::getSystemPrompt()
{
    // We explain the synth architecture to Gemini so it generates valid values.
    return R"(You are a synthesizer patch programmer for a Dual Oscillator Subtractive Synth.
    
    Available Parameters and their ranges:
    - "OSC1", "OSC2" (Choice): 0=Sine, 1=Saw, 2=Square
    - "OSC1GAIN", "OSC2GAIN" (Float): -40.0 to 0.2
    - "OSC1PITCH", "OSC2PITCH" (Int): -48 to 48 (semitones)
    - "OSC1FMFREQ", "OSC2FMFREQ" (Float): 0.0 to 1000.0 (Hz)
    - "OSC1FMDEPTH", "OSC2FMDEPTH" (Float): 0.0 to 100.0
    
    - "FILTERTYPE" (Choice): 0=LowPass, 1=BandPass, 2=HighPass
    - "FILTERCUTOFF" (Float): 20.0 to 20000.0 (Hz)
    - "FILTERRESONANCE" (Float): 0.1 to 2.0
    
    - "ATTACK", "DECAY", "SUSTAIN", "RELEASE" (Amp Envelope): A/D/R 0.1-1.0/3.0, S 0.1-1.0
    
    - "FILTERADSRDEPTH" (Float): 0.0 to 10000.0
    - "FILTERATTACK", "FILTERDECAY", "FILTERSUSTAIN", "FILTERRELEASE" (Filter Env)
    
    - "LFO1FREQ" (Float): 0.0 to 20.0 Hz
    - "LFO1DEPTH" (Float): 0.0 to 10000.0
    
    - "REVERBSIZE", "REVERBWIDTH", "REVERBDAMPING", "REVERBFREEZE" (Float): 0.0 to 1.0
    - "REVERBDRY", "REVERBWET" (Float): 0.0 to 1.0

    INSTRUCTIONS:
    1. Analyze the user's description of a sound (e.g., "warm pad", "laser zap").
    2. Generate a JSON object where keys are the parameter names listed above and values are the settings to achieve that sound.
    3. ONLY return the JSON. No markdown formatting, no explanations.
    )";
}

void TapSynthAudioProcessor::sendPromptToAI(const juce::String& prompt)
{
    if (apiKey.isEmpty() || apiKey == "YOUR_GEMINI_API_KEY_HERE")
    {
        if (onStatusChange) onStatusChange(false, "API Key not set in Processor!");
        return;
    }

    pool.addJob([this, prompt]()
        {
            // 1. Construct JSON Payload for Gemini API
            juce::var systemPart;
            systemPart.getDynamicObject()->setProperty("text", getSystemPrompt());

            juce::var userPart;
            userPart.getDynamicObject()->setProperty("text", "Create this sound: " + prompt);

            // Gemini API structure: { contents: [{ parts: [{ text: ... }] }] }
            // Note: For system instructions + user prompt, structure varies by model version.
            // For gemini-1.5-flash, we can often just put system instructions in the prompt or use the system_instruction field.
            // Simplest way for valid JSON: Combine them into one user message for zero-shot prompting.

            juce::var textObj;
            textObj.getDynamicObject()->setProperty("text", getSystemPrompt() + "\n\nUser Request: " + prompt);

            juce::var partObj;
            partObj.getDynamicObject()->setProperty("parts", juce::Array<juce::var>{ textObj });
            partObj.getDynamicObject()->setProperty("role", "user");

            juce::var contentArray;
            contentArray.add(partObj);

            juce::var mainObj;
            mainObj.getDynamicObject()->setProperty("contents", contentArray);

            // Safety settings (optional, but good to prevent blocks)
            // Generation Config (Force JSON response mime type if supported, otherwise rely on prompt)

            juce::String jsonPayload = juce::JSON::toString(mainObj);

            // 2. Perform Network Request
            juce::URL url("https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=" + apiKey);

            // Headers
            juce::StringArray headers;
            headers.add("Content-Type: application/json");

            // Execute
            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                .withExtraHeaders(headers.joinIntoString("\r\n"));

            std::unique_ptr<juce::InputStream> stream = url.withPOSTData(jsonPayload).createInputStream(options);

            if (stream != nullptr)
            {
                auto response = stream->readEntireStreamAsString();
                auto jsonResponse = extractJsonFromResponse(response);

                // Pass back to Message Thread
                juce::MessageManager::callAsync([this, jsonResponse]()
                    {
                        updateParametersFromJson(jsonResponse);
                    });
            }
            else
            {
                juce::MessageManager::callAsync([this]()
                    {
                        if (onStatusChange) onStatusChange(false, "Network Error");
                    });
            }
        });
}

juce::String TapSynthAudioProcessor::extractJsonFromResponse(const juce::String& response)
{
    // Gemini returns a complex JSON. We need: candidates[0].content.parts[0].text
    juce::var parsed = juce::JSON::parse(response);

    if (parsed.hasProperty("candidates"))
    {
        auto candidates = parsed["candidates"];
        if (candidates.isArray() && candidates.size() > 0)
        {
            auto content = candidates[0]["content"];
            auto parts = content["parts"];
            if (parts.isArray() && parts.size() > 0)
            {
                juce::String rawText = parts[0]["text"].toString();

                // Strip Markdown code fences if present (e.g. ```json ... ```)
                rawText = rawText.replace("```json", "", true);
                rawText = rawText.replace("```", "");
                return rawText.trim();
            }
        }
    }
    return "{}";
}

void TapSynthAudioProcessor::updateParametersFromJson(const juce::String& jsonString)
{
    juce::var parsedJson = juce::JSON::parse(jsonString);

    if (!parsedJson.isObject())
    {
        if (onStatusChange) onStatusChange(false, "Failed to parse AI response");
        return;
    }

    auto* obj = parsedJson.getDynamicObject();
    auto properties = obj->getProperties();

    for (auto& prop : properties)
    {
        juce::String paramID = prop.name.toString();
        float value = (float)prop.value;

        // Find parameter in APVTS
        auto* param = apvts.getParameter(paramID);

        if (param != nullptr)
        {
            if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*>(param))
            {
                // IMPORTANT: setValueNotifyingHost expects a normalized value (0.0 - 1.0)
                // But the AI returns the real value (e.g., 500Hz).
                // We must convert Real -> Normalized.
                float normalizedValue = rangedParam->convertTo0to1(value);
                rangedParam->setValueNotifyingHost(normalizedValue);
            }
        }
    }

    if (onStatusChange) onStatusChange(true, "Patch generated!");