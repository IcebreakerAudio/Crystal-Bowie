#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <IA_Waveshaping/BasicClippers.hpp>
#include <IA_Filters/CrossoverFilter.hpp>

namespace Clipper
{
    const juce::StringArray Names
    {
        "Hard Clip",
        "Tanh",
        "Atan",
        "x / (1 + |x|)",
        "x / sqrt(1 + x^2)",
        "x - (x^3 / 6)",
        "Poly Soft Clip",
        "Ripple"
    };
};

template<typename Type>
class ProcessingModule
{
public:

    ProcessingModule();
    ~ProcessingModule() = default;

    void prepare(const int numChannels, const int expectedBufferSize);
    void setSampleRate(double newSampleRate);

    void setOverSampleIndex(int newIndex);
    void setClippingToUse(int negativeIndex, int positiveIndex);
    void setThresholds(Type newNegativeThreshold, Type newPositiveThreshold);
    void setCrossoverFrequencies(double low, double high);

    void setDrive(Type driveAmountDecibels);
    void setMix(Type dryWetMixRatio);
    void setPassBandLevel(Type levelInDecibels);

    int getLatency();
    bool isPrepared() { return prepared; }

    void processBlock(juce::dsp::AudioBlock<Type>& block);

    Type getMin()  { return minIn; }
    Type getMax()  { return maxIn; }

    //==============================================================================

    // for UI use. Do not use with an instance that is also processing realtime audio
    void drawPath(juce::Path& p, juce::Rectangle<float> bounds, float scale = 1.2f);

private:

    //==============================================================================

    const int numOversamplers = 5;
    const Type zero = static_cast<Type>(0.0);
    const Type one = static_cast<Type>(1.0);

    const double smoothingTimeMs = 20.0;

    //==============================================================================

    std::vector<std::unique_ptr<juce::dsp::Oversampling<Type>>> overSamplers;
    IADSP::CrossoverFilter<Type> xOverFilterLow, xOverFilterHigh;
    std::vector<std::function<Type(Type)>> clippers;

    Type processSample(Type sample, int channel = 0);

    //==============================================================================

    juce::LinearSmoothedValue<double> xOverFreqLowSm, xOverFreqHighSm;
    juce::LinearSmoothedValue<Type> negThresholdSm {one}, posThresholdSm {one};
    juce::LinearSmoothedValue<Type> driveGainSm {one}, driveHighGainSm {one}, driveOutGainSm {one}, mixSm {one}, passbandGainSm {one};
    Type negGainIn {one}, negGainOut {one}, posGainIn {one}, posGainOut {one}, driveGain {one}, driveOutGain {one}, driveHighGain {one}, mix {one}, passbandGain {one};
    int osIndex = 0, osFactor = 1;
    int negClipIndex = 0, posClipIndex = 0;

    void resetSmoothers();

    //==============================================================================

    double sampleRate = 48000.0, overSampleRate = 48000.0;
    void updateFilterSampleRate();

    bool prepared = false;

    //==============================================================================

    Type maxIn {zero}, minIn {zero};
};
