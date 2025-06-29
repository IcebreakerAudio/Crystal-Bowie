#pragma once

#include "PluginProcessor.h"
#include "UI/SpectrumDisplay.hpp"
#include "UI/TransformDisplay.hpp"
#include "Utilities/TwoValueSliderAttachment.hpp"

//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    void timerCallback() override;

private:

    AudioPluginAudioProcessor& processorRef;

    juce::Slider filterSlider { juce::Slider::SliderStyle::TwoValueHorizontal, juce::Slider::TextEntryBoxPosition::NoTextBox };
    juce::TextButton filterModeButton {"Filter", "FILTER"};

    SpectrumDisplay spectrumDisplay;
    TransformDisplay transformDisplay;

    juce::OwnedArray<juce::ComboBox> menus;
    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::Label> labels;

    juce::OwnedArray<juce::ComboBoxParameterAttachment> menuAttachments;
    juce::OwnedArray<juce::SliderParameterAttachment> sliderAttachments;
    std::unique_ptr<TwoValueSliderAttachment> filterSliderAttachment;

    enum menu_ids
    {
        clipModeNeg_MenuId,
        clipModePos_MenuId,
        numMenus
    };

    enum slider_ids
    {
        drive_SliderId,
        symmetry_SliderId,
        mix_SliderId,
        level_SliderId,
        numSliders
    };

    enum label_ids
    {
        numLabels
    };

    void updateTransformDisplay();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
