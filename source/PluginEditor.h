#pragma once

#include "PluginProcessor.h"
#include "UI/SpectrumDisplay.hpp"
#include "UI/TransformDisplay.hpp"
#include "Utilities/TwoValueSliderAttachment.hpp"
#include <BinaryData.h>

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

    juce::DrawableButton powerButton { "Power", juce::DrawableButton::ButtonStyle::ImageFitted };

    juce::Slider filterSlider { juce::Slider::SliderStyle::TwoValueHorizontal, juce::Slider::TextEntryBoxPosition::NoTextBox };
    juce::ToggleButton filterModeButton { "Mute High/Low Bands" };

    SpectrumDisplay spectrumDisplay;
    TransformDisplay transformDisplay;

    juce::OwnedArray<juce::ComboBox> menus;
    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::Label> labels;

    juce::OwnedArray<juce::ComboBoxParameterAttachment> menuAttachments;
    juce::OwnedArray<juce::SliderParameterAttachment> sliderAttachments;
    juce::OwnedArray<juce::ButtonParameterAttachment> buttonAttachments;
    std::unique_ptr<TwoValueSliderAttachment> filterSliderAttachment;

    enum menu_ids
    {
        clipModeNeg_MenuId,
        clipModePos_MenuId,
        overSampling_MenuId,
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
