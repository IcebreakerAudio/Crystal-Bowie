#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class TwoValueSliderAttachment : private juce::Slider::Listener
{
public:

    TwoValueSliderAttachment(juce::Slider& s, juce::RangedAudioParameter& min, juce::RangedAudioParameter& max, juce::UndoManager* um = nullptr);
    ~TwoValueSliderAttachment();
    
    void sendInitialUpdate();

private:
    void setMinValue (float newValue);
    void setMaxValue (float newValue);

    void sliderValueChanged (juce::Slider*) override;
    void sliderDragStarted  (juce::Slider*) override;
    void sliderDragEnded    (juce::Slider*) override;

    juce::Slider& slider;
    juce::ParameterAttachment minAttachment, maxAttachment;

    bool ignoreCallbacks = false;

    int thumbBeingDragged = -1;
};

