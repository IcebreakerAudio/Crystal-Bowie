#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class SpectrumDisplay : public juce::Component
{
public:
    SpectrumDisplay()
    {
        addAndMakeVisible(background);
        addAndMakeVisible(display);
    }
    ~SpectrumDisplay() override = default;

    void paint (juce::Graphics& g) override { juce::ignoreUnused(g); }

    void resized() override
    {
        background.setBounds(getLocalBounds());
        display.setBounds(getLocalBounds());
    }

    void updatePath(const juce::Path& newPath)
    {
        display.path = newPath;
        display.repaint();
    }

    void setRange(const juce::NormalisableRange<float>& range)
    {
        background.freqRange = range;
        background.rangeSet = true;
        background.repaint();
    }

private:

    struct BackgroundComponent : public juce::Component
    {
        BackgroundComponent() { setBufferedToImage(true); }
        ~BackgroundComponent() override = default;

        void paint (juce::Graphics& g) override
        {
            g.fillAll(juce::Colours::black.withAlpha(0.25f));

            const auto bounds = getLocalBounds().toFloat();
            const auto width = bounds.getWidth();
            const auto height = bounds.getHeight();

            if(rangeSet)
            {
                g.setColour(juce::Colours::lightgrey.withAlpha(0.33f));

                auto drawGridLines = [&](float from, float to, float step) {
                    for (auto f = from; f < to; f += step) {
                        auto x = freqRange.convertTo0to1(f) * width;
                        g.fillRect(x - 0.5f, 0.0f, 1.0f, height);
                    }
                };
                drawGridLines(freqRange.start, 100.0f,        10.0f);
                drawGridLines(100.0f,          1000.0f,       100.0f);
                drawGridLines(1000.0f,         freqRange.end, 1000.0f);
            }

            g.setColour(juce::Colours::lightgrey);
            g.drawRect(bounds.reduced(0.5f));
        }

        juce::NormalisableRange<float> freqRange;
        bool rangeSet = false;
    };

    struct DisplayComponent : public juce::Component
    {
        ~DisplayComponent() override = default;

        void paint (juce::Graphics& g) override
        {
            g.setColour(juce::Colours::lightgrey.withAlpha(0.75f));
            g.strokePath(path, juce::PathStrokeType{1.5f}, {});
        }

        juce::Path path;
    };

    BackgroundComponent background;
    DisplayComponent display;

};
