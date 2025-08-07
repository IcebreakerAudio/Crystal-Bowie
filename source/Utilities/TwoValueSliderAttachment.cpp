#include "TwoValueSliderAttachment.hpp"

void TwoValueSliderAttachment::setMinValue (float newValue)
{
    const juce::ScopedValueSetter<bool> svs (ignoreCallbacks, true);
    auto maxValue = static_cast<float>(slider.getMaxValue());
    if(newValue > maxValue) {
        newValue = maxValue;
    }
    slider.setMinValue (newValue, juce::sendNotificationSync);
}

void TwoValueSliderAttachment::setMaxValue (float newValue)
{
    const juce::ScopedValueSetter<bool> svs (ignoreCallbacks, true);
    auto minValue = static_cast<float>(slider.getMinValue());
    if(newValue < minValue) {
        newValue = minValue;
    }
    slider.setMaxValue (newValue, juce::sendNotificationSync);
}
    
void TwoValueSliderAttachment::sliderValueChanged(juce::Slider* s)
{
    if (ignoreCallbacks) {
        return;
    }

    minAttachment.setValueAsPartOfGesture (static_cast<float>(s->getMinValue()));
    maxAttachment.setValueAsPartOfGesture (static_cast<float>(s->getMaxValue()));
}

void TwoValueSliderAttachment::sliderDragStarted([[maybe_unused]] juce::Slider* s)
{
    minAttachment.beginGesture();
    maxAttachment.beginGesture();
}

void TwoValueSliderAttachment::sliderDragEnded([[maybe_unused]] juce::Slider* s)
{
    minAttachment.endGesture();
    maxAttachment.endGesture();
}

TwoValueSliderAttachment::TwoValueSliderAttachment(juce::Slider& s, juce::RangedAudioParameter& min, juce::RangedAudioParameter& max, juce::UndoManager* um)
                        : slider (s),
                          minAttachment(min, [this] (float f) { setMinValue (f); }, um),
                          maxAttachment(max, [this] (float f) { setMaxValue (f); }, um)
{
    // slider.valueFromTextFunction = [&min] (const juce::String& text) { return (double) min.convertFrom0to1 (min.getValueForText (text)); };
    // slider.textFromValueFunction = [&min] (double value) { return min.getText (min.convertTo0to1 ((float) value), 0); };
    // slider.setDoubleClickReturnValue (true, min.convertFrom0to1 (min.getDefaultValue()));

    auto range = min.getNormalisableRange();

    auto convertFrom0To1Function = [range] (double currentRangeStart,
                                            double currentRangeEnd,
                                            double normalisedValue) mutable
    {
        range.start = (float) currentRangeStart;
        range.end = (float) currentRangeEnd;
        return (double) range.convertFrom0to1 ((float) normalisedValue);
    };

    auto convertTo0To1Function = [range] (double currentRangeStart,
                                          double currentRangeEnd,
                                          double mappedValue) mutable
    {
        range.start = (float) currentRangeStart;
        range.end = (float) currentRangeEnd;
        return (double) range.convertTo0to1 ((float) mappedValue);
    };

    auto snapToLegalValueFunction = [range] (double currentRangeStart,
                                             double currentRangeEnd,
                                             double mappedValue) mutable
    {
        range.start = (float) currentRangeStart;
        range.end = (float) currentRangeEnd;
        return (double) range.snapToLegalValue ((float) mappedValue);
    };

    juce::NormalisableRange<double> newRange { (double) range.start,
                                         (double) range.end,
                                         std::move (convertFrom0To1Function),
                                         std::move (convertTo0To1Function),
                                         std::move (snapToLegalValueFunction) };
    newRange.interval = range.interval;
    newRange.skew = range.skew;
    newRange.symmetricSkew = range.symmetricSkew;

    slider.setNormalisableRange (newRange);

    sendInitialUpdate();
    slider.addListener (this);
}

TwoValueSliderAttachment::~TwoValueSliderAttachment()
{
    slider.removeListener (this);
}

void TwoValueSliderAttachment::sendInitialUpdate()
{
    maxAttachment.sendInitialUpdate();
    minAttachment.sendInitialUpdate();
}