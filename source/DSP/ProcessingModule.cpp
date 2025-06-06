#include "ProcessingModule.h"

template<typename Type>
void ProcessingModule<Type>::prepare(const int numChannels, const int expectedBufferSize)
{
    for(int i = 0; i < numOversamplers; ++i)
    {
        auto& os = overSamplers.emplace_back(numChannels);
        if(i == 0) {
            os.addDummyOversamplingStage();
        }
        else
        {
            os.addOversamplingStage(juce::dsp::Oversampling<double>::FilterType::filterHalfBandFIREquiripple, 0.05f, -90.0f, 0.06f, -75.0f);
            for(int x = 1; x < i; ++x) {
                os.addOversamplingStage(juce::dsp::Oversampling<double>::FilterType::filterHalfBandPolyphaseIIR, 0.1f, -70.0f, 0.12f, -60.0f);
            }
        }
        os.setUsingIntegerLatency(true);
        os.initProcessing(expectedBufferSize);
    }

    xOverFilterLow.setNumChannels(numChannels);
    xOverFilterHigh.setNumChannels(numChannels);

    clippers.push_back(BasicClippers::hardClip<Type>);
    clippers.push_back([](Type x) -> Type { return std::tanh(x); });
    clippers.push_back([](Type x) -> Type { return std::atan(x); });
    clippers.push_back(BasicClippers::saturate<Type>);
    clippers.push_back(BasicClippers::softClip<Type>);
    clippers.push_back(BasicClippers::polySoftClip<Type>);
    clippers.push_back([](Type x) -> Type { return BasicClippers::cubicSoftClip(x, static_cast<Type>(3.0)); });

    prepared = true;
}

template<typename Type>
void ProcessingModule<Type>::processBlock(juce::dsp::AudioBlock<Type>& block)
{
    jassert(prepared);
    if(!prepared) {
        return;
    }

    const auto numChannels = block.getNumChannels();
    const auto numSamples = block.getNumSamples();

    auto osBlock = overSamplers[osIndex].processSamplesUp(block);
    jassert(osFactor * numSamples == osBlock.getNumSamples());

    for(int s = 0; s < numSamples; ++s)
    {
        negGainOut = negThresholdSm.getNextValue();
        negGainIn = one / negGainOut;

        posGainOut = posThresholdSm.getNextValue();
        posGainIn = one / posGainOut;

        if(xOverFreqLowSm.isSmoothing()) {
            xOverFilterLow.setCutoffFrequency(xOverFreqLowSm.getNextValue());
        }

        if(xOverFreqHighSm.isSmoothing()) {
            xOverFilterHigh.setCutoffFrequency(xOverFreqHighSm.getNextValue());
        }

        for(int c = 0; c < numChannels; ++c)
        {
            auto* data = block.getChannelPointer(c);
            for(int i = 0; i < osFactor; ++i) {
                data[i] = processSample(data[i], c);
            }
        }
    }

    overSamplers[osIndex].processSamplesDown(block);
}

template<typename Type>
Type ProcessingModule<Type>::processSample(Type sample, int channel)
{
    Type low, mid, high;
    low = mid = high = zero;

    xOverFilterLow.processCrossover(sample, low, mid);
    xOverFilterHigh.processCrossover(mid, mid, high);

    if(mid < zero)
    {
        mid *= negGainIn;
        mid = clippers[negClipIndex](mid);
        mid *= negGainOut;
    }
    else
    {
        mid *= posGainIn;
        mid = clippers[posClipIndex](mid);
        mid *= posGainOut;
    }

    return low + mid + high;
}

//==============================================================================

template<typename Type>
void ProcessingModule<Type>::setSampleRate(double newSampleRate)
{
    jassert(prepared);
    if(!prepared) {
        return;
    }

    sampleRate = newSampleRate;
    updateFilterSampleRate();
    resetSmoothers();
}

template<typename Type>
void ProcessingModule<Type>::updateFilterSampleRate()
{
    jassert(prepared);

    overSampleRate = sampleRate * static_cast<Type>(osFactor);

    xOverFilterLow.setSampleRate(overSampleRate);
    xOverFilterHigh.setSampleRate(overSampleRate);
}

template<typename Type>
void ProcessingModule<Type>::setOverSampleIndex(int newIndex)
{
    jassert(juce::isPositiveAndBelow(newIndex, numOversamplers));

    if(osIndex == newIndex) {
        return;
    }

    osIndex = newIndex;

    jassert(prepared);
    if(!prepared) {
        return;
    }

    overSamplers[osIndex].reset();
    osFactor = overSamplers[osIndex].getOversamplingFactor();
    updateFilterSampleRate();
}

template<typename Type>
int ProcessingModule<Type>::getLatency()
{
    jassert(prepared);
    if(!prepared) {
        return;
    }

    return static_cast<int>(overSamplers[osIndex].getLatencyInSamples());
}

//==============================================================================

template<typename Type>
void ProcessingModule<Type>::setClippingToUse(int positiveIndex, int negativeIndex)
{
    jassert(juce::isPositiveAndBelow(positiveIndex, clippers.size()));
    jassert(juce::isPositiveAndBelow(negativeIndex, clippers.size()));

    negClipIndex = negativeIndex;
    posClipIndex = positiveIndex;
}

template<typename Type>
void ProcessingModule<Type>::setThresholds(Type newNegativeThreshold, Type newPositiveThreshold)
{
    negThresholdSm.setTargetValue(newNegativeThreshold);
    posThresholdSm.setTargetValue(newPositiveThreshold);
}

template<typename Type>
void ProcessingModule<Type>::setCrossoverFrequencies(double low, double high)
{
    jassert(low <= high);
    
    xOverFreqLowSm.setTargetValue(low);
    xOverFreqHighSm.setTargetValue(high);
}

//==============================================================================

template<typename Type>
void ProcessingModule<Type>::resetSmoothers()
{
    xOverFreqLowSm.reset(sampleRate, smoothingTimeMs * 0.001);
    xOverFreqHighSm.reset(sampleRate, smoothingTimeMs * 0.001);
    negThresholdSm.reset(sampleRate, smoothingTimeMs * 0.001);
    posThresholdSm.reset(sampleRate, smoothingTimeMs * 0.001);
}
