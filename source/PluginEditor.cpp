#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    for(int i = 0; i < 2; ++i)
    {
        auto m = menus.add(std::make_unique<juce::ComboBox>());
        m->addItemList(Clipper::Names, 1);
        addAndMakeVisible(m);
    }
    menuAttachments.add(new juce::ComboBoxParameterAttachment(*(processorRef.apvts.getParameter("modeNeg")), *(menus[clipModeNeg_MenuId])));
    menuAttachments.add(new juce::ComboBoxParameterAttachment(*(processorRef.apvts.getParameter("modePos")), *(menus[clipModePos_MenuId])));
    
    auto m = menus.add(std::make_unique<juce::ComboBox>());
    m->addItemList(juce::StringArray{ "HQ Off", "HQ x2", "HQ x4", "HQ x8", "HQ x16" }, 1);
    addAndMakeVisible(m);
    menuAttachments.add(new juce::ComboBoxParameterAttachment(*(processorRef.apvts.getParameter("modePos")), *m));

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

    auto onImage = juce::Drawable::createFromImageData(BinaryData::Power_On_svg, BinaryData::Power_On_svgSize);
    auto offImage = juce::Drawable::createFromImageData(BinaryData::Power_Off_svg, BinaryData::Power_Off_svgSize);
    powerButton.setImages(offImage.get(), nullptr, nullptr, nullptr,
                          onImage.get());
    powerButton.setClickingTogglesState(true);
    buttonAttachments.add(new juce::ButtonParameterAttachment(*(processorRef.apvts.getParameter("active")), powerButton));
    addAndMakeVisible(powerButton);
    
    buttonAttachments.add(new juce::ButtonParameterAttachment(*(processorRef.apvts.getParameter("filter")), filterModeButton));
    addAndMakeVisible(filterModeButton);

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
    const auto padding = bounds.getHeight() / 48;

    auto titleBounds = bounds.removeFromLeft(bounds.getWidth() * 2 / 11);
    titleBounds.reduce(padding, padding);
    bounds.reduce(padding, padding);

    const auto height = bounds.getHeight();
    const auto width = bounds.getWidth();

    powerButton.setBounds(titleBounds.removeFromTop(height / 20));

    auto spectrumBounds = bounds.removeFromTop(height / 13);
    spectrumDisplay.setBounds(spectrumBounds);
    filterSlider.setBounds(spectrumBounds);
    bounds.removeFromTop(2);
    filterModeButton.setBounds(bounds.removeFromTop(height / 16));

    bounds.removeFromTop(12);
    auto graphBounds = bounds.removeFromTop(width * 8 / 10);
    transformDisplay.setBounds(graphBounds);
    bounds.removeFromTop(10);

    auto menuBounds = bounds.removeFromTop(height / 22);
    menus[clipModeNeg_MenuId]->setBounds(menuBounds.removeFromLeft((width / 2) * 9 / 10));
    menus[clipModePos_MenuId]->setBounds(menuBounds.removeFromRight((width / 2) * 9 / 10));
    bounds.removeFromTop(10);

    sliders[drive_SliderId]->setBounds(bounds.removeFromTop(height / 10));
    sliders[symmetry_SliderId]->setBounds(bounds.removeFromTop(height / 12));
    auto sliderBounds = bounds.removeFromTop(height / 12);
    sliders[mix_SliderId]->setBounds(sliderBounds.removeFromLeft((width / 2) * 9 / 10));
    sliders[level_SliderId]->setBounds(sliderBounds.removeFromRight((width / 2) * 9 / 10));
    
    menus[overSampling_MenuId]->setBounds(bounds.removeFromBottom(height / 22).reduced(width / 4, 0));
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
    transformDisplay.setDrive(static_cast<float>(sliders[drive_SliderId]->getValue()));
    transformDisplay.setSymmetry(static_cast<float>(sliders[symmetry_SliderId]->getValue()));
    transformDisplay.setClippingToUse(menus[clipModeNeg_MenuId]->getSelectedItemIndex(), menus[clipModePos_MenuId]->getSelectedItemIndex());

    transformDisplay.updateDisplay();
}
