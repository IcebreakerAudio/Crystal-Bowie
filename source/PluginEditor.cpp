#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    juce::ignoreUnused (processorRef);
    setSize (400, 300);

    startTimer(1000 / 50);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);

    auto bounds = getLocalBounds().toFloat();
    auto redraw = processorRef.spectrumProcessor.checkForNewData();

    if(redraw)
    {
        processorRef.spectrumProcessor.createLinePath(path, bounds);
    }
    if(!path.isEmpty())
    {
        g.setColour(juce::Colours::white);
        g.strokePath(path, juce::PathStrokeType{1.5f}, {});
    }
}

void AudioPluginAudioProcessorEditor::resized()
{
}

void AudioPluginAudioProcessorEditor::timerCallback()
{
    repaint();
}
