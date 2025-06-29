#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../ProcessingModule.hpp"

class TransformDisplay : public juce::Component
{
public:
    TransformDisplay()
    {
        addAndMakeVisible(background);
        addAndMakeVisible(display);
    };
    ~TransformDisplay() override {};

    void paint (juce::Graphics& g) override
    {

    }

    void resized() override
    {
        background.setBounds(getLocalBounds());
        display.setBounds(getLocalBounds());
    }

    void setClippingToUse(int negativeIndex, int positiveIndex)
    {
        processor.setClippingToUse(negativeIndex, positiveIndex);
    }

    void setSymmetry(float newSymmetry)
    {
        auto symmetry = newSymmetry * 0.009f;
        auto negThresh = -1.0f;
        auto posThresh = 1.0f;

        if(symmetry < 0.0f) {
            posThresh = 1.0f + symmetry;
        }
        else {
            negThresh = symmetry - 1.0f;
        }

        processor.setThresholds(negThresh, posThresh);
    }

    void setDrive(float driveAmountDecibels)
    {
        processor.setDrive(driveAmountDecibels);
    }

    void updateDisplay()
    {
        processor.drawPath(display.path, display.getLocalBounds().toFloat(), scale);
        display.repaint();
    }

    void setMinMax(float min, float max)
    {
        min = std::sqrt(std::abs(min/ scale));
        max = std::sqrt(max/ scale);

        if(min > scopeMin) {
            scopeMin = min;
        }
        else {
            scopeMin *= 0.95f;
        }

        if(max > scopeMax) {
            scopeMax = max;
        }
        else {
            scopeMax *= 0.95f;
        }

        min = 0.5f - (scopeMin * 0.5f);
        max = 0.5f + (scopeMax * 0.5f);

        min = juce::jlimit(0.0f, 0.5f, min);
        max = juce::jlimit(0.5f, 1.0f, max);

        display.highlightMin = min;
        display.highlightMax = max;

        display.repaint();
    }

private:

    const float scale = 1.2f;

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

            g.fillRect(juce::Rectangle<float>(bounds.getCentreX() - 1.0f, bounds.getY(), 2.0f, bounds.getHeight()));
            g.fillRect(juce::Rectangle<float>(bounds.getX(), bounds.getCentreY() - 1.0f, bounds.getWidth(), 2.0f));
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
            g.setColour(juce::Colours::lightgrey);
            g.strokePath(path, juce::PathStrokeType(1.0f));

            juce::Graphics::ScopedSaveState savedState(g);

            auto bounds = getLocalBounds().toFloat();
            auto width = bounds.getWidth();
            bounds.removeFromLeft(highlightMin * width);
            bounds.removeFromRight((1.0f - highlightMax) * width);
            auto keepDrawing = g.reduceClipRegion(bounds.toNearestInt());
            if(!keepDrawing) {
                return;
            }

            g.setColour(juce::Colours::white);
            g.strokePath(path, juce::PathStrokeType(2.5f));
        }

        void resized() override
        {}

        juce::Path path;
        float highlightMin = 0.5f, highlightMax = 0.5f;
    };

    BackgroundComponent background;
    DisplayComponent display;

    ProcessingModule<float> processor;
    float scopeMin = 0.0f, scopeMax = 0.0f;

};
