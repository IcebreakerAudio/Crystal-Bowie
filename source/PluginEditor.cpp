#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setLookAndFeel(&lnf);
    lnf.setDefaultSansSerifTypeface(juce::Typeface::createSystemTypefaceFor(BinaryData::RobotoRegular_ttf, BinaryData::RobotoRegular_ttfSize));

    background = juce::Drawable::createFromImageData(BinaryData::Background_svg, BinaryData::Background_svgSize);
    addAndMakeVisible(background.get());

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
    menuAttachments.add(new juce::ComboBoxParameterAttachment(*(processorRef.apvts.getParameter("OS")), *m));

    const juce::StringArray labelText { "Drive:", "Sym:", "Mix:", "Level:" };
    for(int i = 0; i < numSliders; ++i)
    {
        auto s = sliders.add(std::make_unique<juce::Slider>());
        s->setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 1, 1);
        addAndMakeVisible(s);

        auto l = sliderLabels.add(std::make_unique<juce::Label>(labelText[i], labelText[i]));
        l->setJustificationType(juce::Justification::centredRight);
        l->setBorderSize(juce::BorderSize<int>{0});
        addAndMakeVisible(l);
    }
    sliderAttachments.add(new juce::SliderParameterAttachment(*(processorRef.apvts.getParameter("drive")), *(sliders[drive_SliderId])));
    sliderAttachments.add(new juce::SliderParameterAttachment(*(processorRef.apvts.getParameter("sym")), *(sliders[symmetry_SliderId])));
    sliderAttachments.add(new juce::SliderParameterAttachment(*(processorRef.apvts.getParameter("mix")), *(sliders[mix_SliderId])));
    sliderAttachments.add(new juce::SliderParameterAttachment(*(processorRef.apvts.getParameter("outGain")), *(sliders[level_SliderId])));

    spectrumDisplay.setRange(processorRef.apvts.getParameter("xOverLow")->getNormalisableRange());
    addAndMakeVisible(spectrumDisplay);
    addAndMakeVisible(transformDisplay);

    filterSlider.setColour(juce::Slider::ColourIds::backgroundColourId, juce::Colours::black.withAlpha(0.5f));
    filterSliderAttachment = std::make_unique<TwoValueSliderAttachment>(filterSlider, *(processorRef.apvts.getParameter("xOverLow")), *(processorRef.apvts.getParameter("xOverHigh")));
    addAndMakeVisible(filterSlider);

    auto onImage = juce::Drawable::createFromImageData(BinaryData::Power_On_svg, BinaryData::Power_On_svgSize);
    auto offImage = juce::Drawable::createFromImageData(BinaryData::Power_Off_svg, BinaryData::Power_Off_svgSize);
    auto overImage = juce::Drawable::createFromImageData(BinaryData::Power_Over_svg, BinaryData::Power_Over_svgSize);
    powerButton.setImages(offImage.get(), overImage.get(), overImage.get(), nullptr, onImage.get());
    powerButton.setClickingTogglesState(true);
    buttonAttachments.add(new juce::ButtonParameterAttachment(*(processorRef.apvts.getParameter("active")), powerButton));
    addAndMakeVisible(powerButton);
    
    buttonAttachments.add(new juce::ButtonParameterAttachment(*(processorRef.apvts.getParameter("filter")), filterModeButton));
    addAndMakeVisible(filterModeButton);

    setResizable(true, true);
    double ratio = double(oWidth) / double(oHeight);
    getConstrainer()->setFixedAspectRatio(ratio);
    setSize(juce::roundToInt(float(oWidth) * resizeRatio),
            juce::roundToInt(float(oHeight) * resizeRatio));
    setResizeLimits(oWidth / 2, oHeight / 2, oWidth * 2, oHeight * 2);

    setSize (oWidth, oHeight);
    startTimer (1000 / 50);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void AudioPluginAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    resizeRatio = static_cast<float>(bounds.getHeight()) / float(oHeight);

    if(background) {
        background->setTransformToFit(bounds.toFloat(), juce::RectanglePlacement::fillDestination);
        background->setBounds(bounds);
    }

    const auto padding = bounds.getHeight() / 48;

    auto titleBounds = bounds.removeFromLeft(bounds.getWidth() * 2 / 11);
    titleBounds.reduce(padding, padding);
    bounds.reduce(0, padding);
    bounds.removeFromRight(padding);

    const auto height = bounds.getHeight();
    const auto width = bounds.getWidth();

    titleBounds.removeFromTop(height / 48);
    powerButton.setBounds(titleBounds.removeFromTop(height / 22));

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

    auto tempBounds = bounds.removeFromTop(height / 11);
    sliderLabels[drive_SliderId]->setBounds(tempBounds.removeFromLeft(width / 8));
    sliders[drive_SliderId]->setBounds(tempBounds);

    tempBounds = bounds.removeFromTop(height / 11);
    sliderLabels[symmetry_SliderId]->setBounds(tempBounds.removeFromLeft(width / 8));
    sliders[symmetry_SliderId]->setBounds(tempBounds);

    tempBounds = bounds.removeFromTop(height / 11);
    sliderLabels[mix_SliderId]->setBounds(tempBounds.removeFromLeft(width / 8));
    sliders[mix_SliderId]->setBounds(tempBounds.removeFromLeft(width * 3 / 8));
    sliderLabels[level_SliderId]->setBounds(tempBounds.removeFromLeft(width / 8));
    sliders[level_SliderId]->setBounds(tempBounds.removeFromLeft(width * 3  / 8));
    
    menus[overSampling_MenuId]->setBounds(bounds.removeFromBottom(height / 22).reduced(width / 4, 0));

    auto font = juce::Font(juce::FontOptions().withHeight(15.0f * resizeRatio));
    for(auto& l : sliderLabels) {
        l->setFont(font);
    }
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
