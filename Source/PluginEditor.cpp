#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
TapSynthAudioProcessorEditor::TapSynthAudioProcessorEditor (TapSynthAudioProcessor& p)
: AudioProcessorEditor (&p)
, audioProcessor (p)
, osc1 (audioProcessor.apvts, "OSC1", "OSC1GAIN", "OSC1PITCH", "OSC1FMFREQ", "OSC1FMDEPTH")
, osc2 (audioProcessor.apvts, "OSC2", "OSC2GAIN", "OSC2PITCH", "OSC2FMFREQ", "OSC2FMDEPTH")
, filter (audioProcessor.apvts, "FILTERTYPE", "FILTERCUTOFF", "FILTERRESONANCE")
, adsr (audioProcessor.apvts, "ATTACK", "DECAY", "SUSTAIN", "RELEASE")
, lfo1 (audioProcessor.apvts, "LFO1FREQ", "LFO1DEPTH")
, filterAdsr (audioProcessor.apvts, "FILTERATTACK", "FILTERDECAY", "FILTERSUSTAIN", "FILTERRELEASE")
, reverb (audioProcessor.apvts, "REVERBSIZE", "REVERBDAMPING", "REVERBWIDTH", "REVERBDRY", "REVERBWET", "REVERBFREEZE")
, meter (audioProcessor)
{
    
    addAndMakeVisible (osc1);
    addAndMakeVisible (osc2);
    addAndMakeVisible (filter);
    addAndMakeVisible (adsr);
    addAndMakeVisible (lfo1);
    addAndMakeVisible (filterAdsr);
    addAndMakeVisible (reverb);
    addAndMakeVisible (meter);
    addAndMakeVisible(promptBox);
    addAndMakeVisible(sendButton);
    

    promptBox.setMultiLine(false);
    promptBox.setReturnKeyStartsNewLine(false);
    promptBox.setTextToShowWhenEmpty("Descreva um timbre...", juce::Colours::grey);
    promptBox.setColour(juce::TextEditor::backgroundColourId, juce::Colours::darkgrey.withAlpha(0.2f));
    promptBox.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    promptBox.setColour(juce::TextEditor::outlineColourId, juce::Colours::darkgrey);

    osc1.setName ("Oscillator 1");
    osc2.setName ("Oscillator 2");
    filter.setName ("Filtro");
    lfo1.setName ("Filtro LFO");
    filterAdsr.setName ("Filtro ADSR");
    adsr.setName ("ADSR");
    meter.setName ("Meter");
    
    auto oscColour = juce::Colour::fromRGB (247, 190, 67);
    auto filterColour = juce::Colour::fromRGB (246, 87, 64);
    
    osc1.setBoundsColour (oscColour);
    osc2.setBoundsColour (oscColour);
    
    filterAdsr.setBoundsColour (filterColour);
    filter.setBoundsColour (filterColour);
    lfo1.setBoundsColour (filterColour);
        
    // hook up the button
    sendButton.onClick = [this]() { sendPrompt(); };

    startTimerHz (30);
    setSize (1066, 600);
}

TapSynthAudioProcessorEditor::~TapSynthAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void TapSynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void TapSynthAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    auto bottomArea = bounds.removeFromBottom(80);
    bottomArea = bottomArea.reduced(0, 9);

    promptBox.setBounds(bottomArea.removeFromLeft(bottomArea.getWidth() - 80).reduced(5));
    sendButton.setBounds(bottomArea.reduced(5));

    const auto oscWidth = 420;
    const auto oscHeight = 180;

    osc1.setBounds (0, 0, oscWidth, oscHeight);
    osc2.setBounds (0, osc1.getBottom(), oscWidth, oscHeight);
    filter.setBounds (osc1.getRight(), 0, 180, 200);
    lfo1.setBounds (osc2.getRight(), filter.getBottom(), 180, 160);
    filterAdsr.setBounds (filter.getRight(), 0, 230, 360);
    adsr.setBounds (filterAdsr.getRight(), 0, 230, 360);
    reverb.setBounds (0, osc2.getBottom(), oscWidth, 150);
    meter.setBounds (reverb.getRight(), osc2.getBottom(), filterAdsr.getWidth() + lfo1.getWidth(), 150);
}

void TapSynthAudioProcessorEditor::timerCallback()
{
    repaint();
}


// sendPrompt implementation
void TapSynthAudioProcessorEditor::sendPrompt()
{
    const juce::String promptText = promptBox.getText().trim();

    if (promptText.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Prompt empty",
            "Enter a text description of the sound before sending."
        );
        return;
    }

    const juce::String apiKey   = "";
    const juce::String modelId  = "gemini-flash-latest"; // or "gemini-2.5-flash"
    const juce::String endpoint = "https://generativelanguage.googleapis.com/v1beta/models/"
                                  + modelId
                                  + ":generateContent?key="
                                  + apiKey;


    sendButton.setEnabled(false);
    sendButton.setButtonText("Gerando...");

    TapSynthAudioProcessor& processor = audioProcessor;
    const juce::String promptCopy = promptText;
    const juce::String endpointCopy = endpoint;

    std::thread([promptCopy, endpointCopy, &processor, this]()
    {
        juce::String responseBody;
        juce::String errorMsg;

        // --- Build Gemini JSON ---
        juce::DynamicObject* textObj = new juce::DynamicObject();
        textObj->setProperty("text", promptCopy);

        juce::DynamicObject* partObj = new juce::DynamicObject();
        partObj->setProperty("parts", juce::Array<juce::var>{ juce::var(textObj) });

        juce::Array<juce::var> contentsArr;
        contentsArr.add(juce::var(partObj));

        juce::DynamicObject* rootObj = new juce::DynamicObject();
        rootObj->setProperty("contents", contentsArr);

        const juce::String body = juce::JSON::toString(juce::var(rootObj));

        try
        {
            juce::URL url(endpointCopy);

            juce::String headers = "Content-Type: application/json\r\n";

            // ❗ FIX: Create InputStreamOptions in ONE expression — no assignment!
            std::unique_ptr<juce::InputStream> stream =
                url.withPOSTData(body)
                   .createInputStream(
                       juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                           .withHttpRequestCmd("POST")
                           .withExtraHeaders(headers)
                           .withConnectionTimeoutMs(15000)
                           .withNumRedirectsToFollow(2)
                   );

            if (stream == nullptr)
                errorMsg = "Could not open network stream.";
            else
                responseBody = stream->readEntireStreamAsString();
        }
        catch (const std::exception& e)
        {
            errorMsg = e.what();
        }
        catch (...)
        {
            errorMsg = "Unknown network error.";
        }

        // --- Back to UI thread ---
        juce::MessageManager::callAsync([this, &processor, responseBody, errorMsg]()
        {
            sendButton.setEnabled(true);
            sendButton.setButtonText("Enviar");

            if (! errorMsg.isEmpty())
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "Network Error",
                    errorMsg
                );
                return;
            }

            juce::var parsed = juce::JSON::parse(responseBody);
            if (! parsed.isObject())
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "JSON Error",
                    "Invalid JSON:\n" + responseBody
                );
                return;
            }

            // Extract Gemini return text
            auto* root = parsed.getDynamicObject();
            auto candidates = root->getProperty("candidates");

            if (! candidates.isArray() || candidates.getArray()->isEmpty())
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "API Error",
                    "Unexpected response:\n" + responseBody
                );
                return;
            }

            auto* candObj = candidates.getArray()->getFirst().getDynamicObject();
            auto content  = candObj->getProperty("content");

            if (! content.isObject())
                return;

            auto parts = content.getDynamicObject()->getProperty("parts");
            if (! parts.isArray() || parts.getArray()->isEmpty())
                return;

            auto* part0 = parts.getArray()->getFirst().getDynamicObject();
            juce::String jsonParams = part0->getProperty("text").toString();

            juce::var params = juce::JSON::parse(jsonParams);
            if (params.isObject())
                processor.applyParametersFromJson(params);
            else
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "Parse Error",
                    "Returned text was not valid JSON:\n" + jsonParams
                );
        });

    }).detach();
}

