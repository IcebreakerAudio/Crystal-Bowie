#include "CustomLookAndFeel.hpp"

CustomLookAndFeel::CustomLookAndFeel()
{
    auto darkGrey = juce::Colours::darkgrey.withAlpha(0.33f);
    setColour(juce::ToggleButton::tickColourId, juce::Colours::lightgrey);
    setColour(juce::ToggleButton::textColourId, juce::Colours::lightgrey);

    setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    
    setColour(juce::ComboBox::backgroundColourId, juce::Colours::black.withAlpha(0.25f));
    setColour(juce::ComboBox::outlineColourId, juce::Colours::grey);
    setColour(juce::ComboBox::arrowColourId, juce::Colours::grey);
    setColour(juce::ComboBox::textColourId, juce::Colours::lightgrey);

    setColour(juce::PopupMenu::backgroundColourId, juce::Colours::black.interpolatedWith(juce::Colours::darkgrey, 0.25f));
    
    setColour(juce::Slider::backgroundColourId, darkGrey);
    setColour(juce::Slider::trackColourId, juce::Colours::grey);
    setColour(juce::Slider::thumbColourId, juce::Colours::lightgrey);
    
    setColour(juce::Label::textColourId, juce::Colours::lightgrey);
}

void CustomLookAndFeel::drawTickBox (juce::Graphics& g, juce::Component& component,
                                     float x, float y, float w, float h,
                                     const bool ticked,
                                     [[maybe_unused]] const bool isEnabled,
                                     [[maybe_unused]] const bool shouldDrawButtonAsHighlighted,
                                     [[maybe_unused]] const bool shouldDrawButtonAsDown)
{
    juce::Rectangle<float> tickBounds (x, y, w, h);

    g.setColour(component.findColour (juce::ToggleButton::tickColourId));
    g.drawEllipse(tickBounds, 1.0f);

    if (ticked || shouldDrawButtonAsHighlighted)
    {
        g.setColour(component.findColour(juce::ToggleButton::tickColourId).withAlpha(
            shouldDrawButtonAsHighlighted && !ticked ? 0.25f : 1.0f
        ));
        g.fillEllipse(tickBounds.reduced(4, 4).toFloat());
    }
}

void CustomLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                      int, int, int, int, juce::ComboBox& box)
{
    juce::Rectangle<int> boxBounds (0, 0, width, height);

    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRect (boxBounds.toFloat());

    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRect (boxBounds.toFloat().reduced (0.5f, 0.5f), 1.0f);

    juce::Rectangle<int> arrowZone (width - 20, 0, 16, height);
    juce::Path path;
    path.startNewSubPath ((float) arrowZone.getX() + 3.0f, (float) arrowZone.getCentreY() - 2.0f);
    path.lineTo ((float) arrowZone.getCentreX(), (float) arrowZone.getCentreY() + 3.0f);
    path.lineTo ((float) arrowZone.getRight() - 3.0f, (float) arrowZone.getCentreY() - 2.0f);

    g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha ((box.isEnabled() ? 0.9f : 0.2f)));
    g.strokePath (path, juce::PathStrokeType (2.0f));
}

void CustomLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos,
                                          float minSliderPos,
                                          float maxSliderPos,
                                          const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (slider.isBar() || slider.isVertical() || style == juce::Slider::SliderStyle::ThreeValueHorizontal) {
        g.drawFittedText("No LnF Found", juce::Rectangle<int>(x, y, width, height), juce::Justification::centred, 2);
        return;
    }

    if(style == juce::Slider::SliderStyle::TwoValueHorizontal)
    {
        // fix JUCE's forced padding/resizing
        x = 0;
        width = slider.getLocalBounds().getWidth();
        minSliderPos = (float) slider.valueToProportionOfLength(slider.getMinValue()) * (float) width;
        maxSliderPos = (float) slider.valueToProportionOfLength(slider.getMaxValue()) * (float) width;
        // 

        auto trackWidth = height * 0.125f;

        g.setColour (slider.findColour (juce::Slider::backgroundColourId));
        g.fillRect(juce::Rectangle<float>{ 1.0f, 1.0f, minSliderPos, (float) (height-2) });
        g.fillRect(juce::Rectangle<float>{ maxSliderPos, 1.0f, (float) (width-2) - maxSliderPos, (float) (height-2) });

        auto pointerColour = slider.findColour (juce::Slider::thumbColourId);

        drawPointer (g, minSliderPos - trackWidth, 0.0f, trackWidth * 2.0f, pointerColour, 2);
        g.fillRect(juce::Rectangle<float>{minSliderPos - 0.5f, 0.0f, 1.0f, (float) height});

        drawPointer (g, maxSliderPos - trackWidth, (float) (y + height) - trackWidth * 2.0f, trackWidth * 2.0f, pointerColour, 4);
        g.fillRect(juce::Rectangle<float>{maxSliderPos - 0.5f, 0.0f, 1.0f, (float) height});
    }
    else
    {
        auto isBipolar =  juce::approximatelyEqual(std::abs(slider.getMinimum()), slider.getMaximum());
        auto trackWidth = juce::jmin (12.0f, (float) height * 0.5f);

        juce::Point<float> startPoint ((float) x, (float) y + (float) height * 0.5f);
        juce::Point<float> endPoint ((float) (width + x), startPoint.y);

        juce::Path backgroundTrack;
        backgroundTrack.startNewSubPath (startPoint);
        backgroundTrack.lineTo (endPoint);
        g.setColour (slider.findColour (juce::Slider::backgroundColourId));
        g.strokePath (backgroundTrack, { trackWidth, juce::PathStrokeType::mitered , juce::PathStrokeType::butt });

        juce::Path valueTrack;
        juce::Point<float> minPoint, maxPoint, thumbPoint;

        auto kx = slider.isHorizontal() ? sliderPos : ((float) x + (float) width * 0.5f);
        auto ky = slider.isHorizontal() ? ((float) y + (float) height * 0.5f) : sliderPos;

        minPoint = startPoint;
        maxPoint = { kx, ky };

        if(!isBipolar)
        {
            valueTrack.startNewSubPath (minPoint);
            valueTrack.lineTo (maxPoint);
        }
        else
        {
            auto midx = float (x + (width / 2));
            
            valueTrack.startNewSubPath (std::min(midx, maxPoint.x), minPoint.y);
            valueTrack.lineTo (std::max(midx, maxPoint.x), maxPoint.y);
        }
        g.setColour (slider.findColour (juce::Slider::trackColourId));
        g.strokePath (valueTrack, { trackWidth, juce::PathStrokeType::mitered, juce::PathStrokeType::butt });

        g.setColour (slider.findColour (juce::Slider::thumbColourId));
        g.fillRect (juce::Rectangle<float> (4.0f, trackWidth).withCentre (maxPoint));
    }
}

juce::Font CustomLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return withDefaultMetrics (juce::FontOptions { (float) box.getHeight() * 0.6f });
}

void CustomLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    DBG((float) button.getHeight() * 0.4f);
    auto fontSize = (float) button.getHeight() * 0.4f;
    auto tickWidth = fontSize * 1.1f;

    drawTickBox (g, button, 4.0f, ((float) button.getHeight() - tickWidth) * 0.5f,
                 tickWidth, tickWidth,
                 button.getToggleState(),
                 button.isEnabled(),
                 shouldDrawButtonAsHighlighted,
                 shouldDrawButtonAsDown);

    g.setColour (button.findColour (juce::ToggleButton::textColourId));
    g.setFont (fontSize);

    if (! button.isEnabled())
        g.setOpacity (0.5f);

    g.drawFittedText (button.getButtonText(),
                      button.getLocalBounds().withTrimmedLeft (juce::roundToInt (tickWidth) + 10)
                                             .withTrimmedRight (2),
                      juce::Justification::centredLeft, 10);
}