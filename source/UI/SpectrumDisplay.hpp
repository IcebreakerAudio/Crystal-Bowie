#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class SpectrumDisplay : public juce::Component
{
public:
    SpectrumDisplay()
    {
        addAndMakeVisible(background);
        addAndMakeVisible(display);
    };
    ~SpectrumDisplay() override {};

    void paint (juce::Graphics& g) override { juce::ignoreUnused(g); }

    void resized() override
    {
        background.setBounds(getLocalBounds());
        display.setBounds(getLocalBounds());
    }

    juce::Path& getPath() { return display.path; }
    void update() { display.repaint(); }

private:

    struct BackgroundComponent : public juce::Component
    {
        BackgroundComponent() {};
        ~BackgroundComponent() override {};

        void paint (juce::Graphics& g) override
        {
            g.fillAll(juce::Colours::black.withAlpha(0.1f));

            g.setColour(juce::Colours::lightgrey);
            const auto bounds = getLocalBounds().toFloat();

            g.drawRect(bounds.reduced(0.5f));
        }

        void resized() override
        {}
    };

    struct DisplayComponent : public juce::Component
    {
        DisplayComponent() {};
        ~DisplayComponent() override {};
        
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
