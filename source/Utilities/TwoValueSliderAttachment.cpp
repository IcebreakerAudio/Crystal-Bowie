#include "TwoValueSliderAttachment.hpp"


void TwoValueSliderAttachment::setMinValue (float newValue)
{
    const juce::ScopedValueSetter<bool> svs (ignoreCallbacks, true);
    slider.setMinValue (newValue, juce::sendNotificationSync);
}

void TwoValueSliderAttachment::setMaxValue (float newValue)
{
    const juce::ScopedValueSetter<bool> svs (ignoreCallbacks, true);
    slider.setMaxValue (newValue, juce::sendNotificationSync);
}
    
void TwoValueSliderAttachment::sliderValueChanged(juce::Slider* s)
{
    if (s->getThumbBeingDragged() == 1) {
        minAttachment.setValueAsPartOfGesture (s->getMinValue());
    }
    else if (s->getThumbBeingDragged() == 2) {
        maxAttachment.setValueAsPartOfGesture (s->getMaxValue());
    }
}

void TwoValueSliderAttachment::sliderDragStarted(juce::Slider* s)
{
    if (s->getThumbBeingDragged() == 1) {
        minAttachment.beginGesture();
    }
    else if (s->getThumbBeingDragged() == 2) {
        maxAttachment.beginGesture();
    }
}

void TwoValueSliderAttachment::sliderDragEnded(juce::Slider* s)
{
    if (s->getThumbBeingDragged() == 1) {
        minAttachment.endGesture();
    }
    else if (s->getThumbBeingDragged() == 2) {
        maxAttachment.endGesture();
    }
}

TwoValueSliderAttachment::TwoValueSliderAttachment(juce::Slider& s, juce::RangedAudioParameter& min, juce::RangedAudioParameter& max, juce::UndoManager* um)
                        : slider (s),
                          minAttachment(min, [this] (float f) { setMinValue (f); }, um),
                          maxAttachment(max, [this] (float f) { setMaxValue (f); }, um)
{
    slider.valueFromTextFunction = [&min] (const juce::String& text) { return (double) min.convertFrom0to1 (min.getValueForText (text)); };
    slider.textFromValueFunction = [&min] (double value) { return min.getText (min.convertTo0to1 ((float) value), 0); };
    slider.setDoubleClickReturnValue (true, min.convertFrom0to1 (min.getDefaultValue()));

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

    minAttachment.sendInitialUpdate();
    maxAttachment.sendInitialUpdate();

    slider.valueChanged();
    slider.addListener (this);
}

TwoValueSliderAttachment::~TwoValueSliderAttachment()
{
    slider.removeListener (this);
}