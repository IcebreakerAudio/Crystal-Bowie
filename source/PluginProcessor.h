#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <IA_Utilities/ParameterListener.hpp>
#include "ProcessingModule.hpp"
#include "Utilities/SpectrumAnalyser.hpp"

//==============================================================================
class AudioPluginAudioProcessor final : public juce::AudioProcessor
{
public:
    //==============================================================================
    AudioPluginAudioProcessor();
    ~AudioPluginAudioProcessor() override;

    //==============================================================================

    bool supportsDoublePrecisionProcessing() const override { return true; }
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;

    //==============================================================================
    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int index) override { juce::ignoreUnused (index); }
    void changeProgramName (int index, const juce::String& newName) override { juce::ignoreUnused (index, newName); }
    const juce::String getProgramName (int index) override
    {
        juce::ignoreUnused (index);
        return {};
    }

    //==============================================================================
    SpectrumAnalyser spectrumProcessor { 10, true };

    float getMin() { return min.load(); }
    float getMax() { return max.load(); }

    std::atomic<bool> updateInterface { false };
    float getSizeRatio() {
        return interfaceSizeRatio;
    }
    void setSizeRatio(float ratio) {
        interfaceSizeRatio = ratio;
    }

private:

    juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

    float loadRawParameterValue(juce::StringRef parameterID) const
    {
        return apvts.getRawParameterValue(parameterID)->load();
    }

    ParameterListener mainParamListener, clippingParamListener, osParamListener;

    void updateMainParameters();
    void updateClippingParameters();
    void updateOverSampling();
    void updateAllParameters();

    //==============================================================================

    std::unique_ptr<ProcessingModule<float>> floatProcessor;
    std::unique_ptr<ProcessingModule<double>> doubleProcessor;

    std::unique_ptr<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None>> floatDelay;
    std::unique_ptr<juce::dsp::DelayLine<double, juce::dsp::DelayLineInterpolationTypes::None>> doubleDelay;

    const double smoothingTimeMs = 20.0;
    juce::LinearSmoothedValue<float> floatGainSmoother {1.0f};
    juce::LinearSmoothedValue<double> doubleGainSmoother {1.0};

    //==============================================================================

    std::atomic<float> min {0.0f}, max {0.0f};
    float interfaceSizeRatio = 1.0f;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
};
