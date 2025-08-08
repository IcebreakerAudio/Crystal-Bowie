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
    ~SpectrumDisplay() override {}

    void paint (juce::Graphics& g) override { juce::ignoreUnused(g); }

    void resized() override
    {
        background.setBounds(getLocalBounds());
        display.setBounds(getLocalBounds());
    }

    juce::Path& getPath() { return display.path; }
    void update() { display.repaint(); }

    void setRange(const juce::NormalisableRange<float>& range)
    {
        background.freqRange = range;
        background.rangeSet = true;
    }

private:

    struct BackgroundComponent : public juce::Component
    {
        BackgroundComponent() { setBufferedToImage(true); }
        ~BackgroundComponent() override {}

        void paint (juce::Graphics& g) override
        {
            g.fillAll(juce::Colours::black.withAlpha(0.25f));

            const auto bounds = getLocalBounds().toFloat();
            const auto width = bounds.getWidth();
            const auto height = bounds.getHeight();

            if(rangeSet)
            {
                g.setColour(juce::Colours::lightgrey.withAlpha(0.33f));
                for(auto f = freqRange.start; f < 100.0f; f += 10.0f)
                {
                    auto proportion = freqRange.convertTo0to1(f);
                    auto x = proportion * width;

                    g.fillRect(x - 0.5f, 0.0f, 1.0f, height);
                }
                for(auto f = 100.0f; f < 1000.0f; f += 100.0f)
                {
                    auto proportion = freqRange.convertTo0to1(f);
                    auto x = proportion * width;

                    g.fillRect(x - 0.5f, 0.0f, 1.0f, height);
                }
                for(auto f = 1000.0f; f < freqRange.end; f += 1000.0f)
                {
                    auto proportion = freqRange.convertTo0to1(f);
                    auto x = proportion * width;

                    g.fillRect(x - 0.5f, 0.0f, 1.0f, height);
                }
            }

            g.setColour(juce::Colours::lightgrey);
            g.drawRect(bounds.reduced(0.5f));
        }

        void resized() override
        {}

        juce::NormalisableRange<float> freqRange;
        bool rangeSet = false;
    };

    struct DisplayComponent : public juce::Component
    {
        DisplayComponent() {}
        ~DisplayComponent() override {}
        
        void paint (juce::Graphics& g) override
        {
            g.setColour(juce::Colours::lightgrey.withAlpha(0.75f));
            g.strokePath(path, juce::PathStrokeType{1.5f}, {});
        }

        void resized() override
        {}

        juce::Path path;
    };

    BackgroundComponent background;
    DisplayComponent display;

};
