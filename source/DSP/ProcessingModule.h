#pragma once
#include "BasicClippers.h"
#include "CrossoverFilter.h"
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>

namespace Clipper
{
    const juce::StringArray Names
    {
        "Hard Clip",
        "TanH",
        "Atan",
        "1 / (1+|x|)",
        "x - (x^3 / 3)",
        "x - (x^5 / 5)",
        "Poly Soft Clip"
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

    int getLatency();
    bool isPrepared() { return prepared; }

    void processBlock(juce::dsp::AudioBlock<Type>& block);

private:

    //==============================================================================

    const int numOversamplers = 5;
    const Type zero = static_cast<Type>(0.0);
    const Type one = static_cast<Type>(1.0);

    const double smoothingTimeMs = 20.0;

    //==============================================================================

    std::vector<std::unique_ptr<juce::dsp::Oversampling<Type>>> overSamplers;
    CrossoverFilter<Type> xOverFilterLow, xOverFilterHigh;
    std::vector<std::function<Type(Type)>> clippers;

    Type processSample(Type sample, int channel = 0);

    //==============================================================================

    juce::LinearSmoothedValue<double> xOverFreqLowSm, xOverFreqHighSm;
    juce::LinearSmoothedValue<Type> negThresholdSm, posThresholdSm;
    Type negGainIn, negGainOut, posGainIn, posGainOut;
    int osIndex = 0, osFactor = 1;
    int negClipIndex = 0, posClipIndex = 0;

    void resetSmoothers();

    //==============================================================================

    double sampleRate = 48000.0, overSampleRate = 48000.0;
    void updateFilterSampleRate();

    bool prepared = false;
};
