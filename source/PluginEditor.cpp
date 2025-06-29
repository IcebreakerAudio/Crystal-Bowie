#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    for(int i = 0; i < numMenus; ++i)
    {
        auto m = menus.add(std::make_unique<juce::ComboBox>());
        m->addItemList(Clipper::Names, 1);
        addAndMakeVisible(m);
    }
    menuAttachments.add(new juce::ComboBoxParameterAttachment(*(processorRef.apvts.getParameter("modeNeg")), *(menus[clipModeNeg_MenuId])));
    menuAttachments.add(new juce::ComboBoxParameterAttachment(*(processorRef.apvts.getParameter("modePos")), *(menus[clipModePos_MenuId])));


    for(int i = 0; i < numSliders; ++i)
    {
        auto s = sliders.add(std::make_unique<juce::Slider>());
        addAndMakeVisible(s);
    }
    sliderAttachments.add(new juce::SliderParameterAttachment(*(processorRef.apvts.getParameter("drive")), *(sliders[drive_SliderId])));
    sliderAttachments.add(new juce::SliderParameterAttachment(*(processorRef.apvts.getParameter("sym")), *(sliders[symmetry_SliderId])));
    sliderAttachments.add(new juce::SliderParameterAttachment(*(processorRef.apvts.getParameter("mix")), *(sliders[mix_SliderId])));
    sliderAttachments.add(new juce::SliderParameterAttachment(*(processorRef.apvts.getParameter("outGain")), *(sliders[level_SliderId])));

    addAndMakeVisible(spectrumDisplay);
    addAndMakeVisible(transformDisplay);

    filterSliderAttachment = std::make_unique<TwoValueSliderAttachment>(filterSlider, *(processorRef.apvts.getParameter("xOverLow")), *(processorRef.apvts.getParameter("xOverHigh")));
    addAndMakeVisible(filterSlider);

    setSize (420, 630);
    startTimer (1000 / 50);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void AudioPluginAudioProcessorEditor::resized()
{

    auto bounds = getLocalBounds();

    auto titleBounds = bounds.removeFromLeft(bounds.getWidth() * 2 / 11);

    const auto padding = bounds.getHeight() / 48;
    bounds.reduce(padding, padding);
    const auto height = bounds.getHeight();
    const auto width = bounds.getWidth();

    auto spectrumBounds = bounds.removeFromTop(height / 15);
    spectrumDisplay.setBounds(spectrumBounds);
    filterSlider.setBounds(spectrumBounds);

    bounds.removeFromTop(15);
    auto graphBounds = bounds.removeFromTop(width * 9 / 10);
    transformDisplay.setBounds(graphBounds);
    bounds.removeFromTop(15);

    auto menuBounds = bounds.removeFromTop(height / 18);
    menus[clipModeNeg_MenuId]->setBounds(menuBounds.removeFromLeft((width / 2) * 9 / 10));
    menus[clipModePos_MenuId]->setBounds(menuBounds.removeFromRight((width / 2) * 9 / 10));
    bounds.removeFromTop(15);

    sliders[drive_SliderId]->setBounds(bounds.removeFromTop(height / 5));
    sliders[symmetry_SliderId]->setBounds(bounds.removeFromTop(height / 15));
    auto sliderBounds = bounds.removeFromTop(height / 15);
    sliders[mix_SliderId]->setBounds(sliderBounds.removeFromLeft((width / 2) * 9 / 10));
    sliders[level_SliderId]->setBounds(sliderBounds.removeFromRight((width / 2) * 9 / 10));

}

void AudioPluginAudioProcessorEditor::timerCallback()
{
    auto redraw = processorRef.spectrumProcessor.checkForNewData();
    if(redraw)
    {
        auto& path = spectrumDisplay.getPath();
        processorRef.spectrumProcessor.createLinePath(path, spectrumDisplay.getLocalBounds().toFloat());
        spectrumDisplay.update();
    }

    updateTransformDisplay();
    transformDisplay.setMinMax(processorRef.getMin(), processorRef.getMax());
}

void AudioPluginAudioProcessorEditor::updateTransformDisplay()
{
    transformDisplay.setDrive(sliders[drive_SliderId]->getValue());
    transformDisplay.setSymmetry(sliders[symmetry_SliderId]->getValue());
    transformDisplay.setClippingToUse(menus[clipModeNeg_MenuId]->getSelectedItemIndex(), menus[clipModePos_MenuId]->getSelectedItemIndex());

    transformDisplay.updateDisplay();
}
