#include "ProcessingModule.h"

template<typename Type>
ProcessingModule<Type>::ProcessingModule()
{
    clippers.push_back(BasicClippers::hardClip<Type>);
    clippers.push_back([](Type x) -> Type { return std::tanh(x); });
    clippers.push_back([](Type x) -> Type { return std::atan(x); });
    clippers.push_back(BasicClippers::saturate<Type>);
    clippers.push_back(BasicClippers::cubicSoftClip<Type>);
    clippers.push_back([](Type x) -> Type { return BasicClippers::softClipWithFactor(x, static_cast<Type>(5.0)); });
    clippers.push_back(BasicClippers::polySoftClip<Type>);
}

template<typename Type>
void ProcessingModule<Type>::prepare(const int numChannels, const int expectedBufferSize)
{
    for(int i = 0; i < numOversamplers; ++i)
    {
        auto& os = overSamplers.emplace_back(std::make_unique<juce::dsp::Oversampling<Type>>(numChannels));
        if(i == 0) {
            os->addDummyOversamplingStage();
        }
        else
        {
            os->addOversamplingStage(juce::dsp::Oversampling<Type>::FilterType::filterHalfBandFIREquiripple, 0.05f, -90.0f, 0.06f, -75.0f);
            for(int x = 1; x < i; ++x) {
                os->addOversamplingStage(juce::dsp::Oversampling<Type>::FilterType::filterHalfBandPolyphaseIIR, 0.1f, -70.0f, 0.12f, -60.0f);
            }
        }
        os->setUsingIntegerLatency(true);
        os->initProcessing(expectedBufferSize);
    }

    xOverFilterLow.setNumChannels(numChannels);
    xOverFilterHigh.setNumChannels(numChannels);

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

    auto osBlock = overSamplers[osIndex]->processSamplesUp(block);
    jassert(osFactor * numSamples == osBlock.getNumSamples());

    for(int s = 0; s < numSamples; ++s)
    {
        negGainOut = negThresholdSm.getNextValue();
        negGainIn = one / negGainOut;

        posGainOut = posThresholdSm.getNextValue();
        posGainIn = one / posGainOut;

        driveGain = driveGainSm.getNextValue();
        driveOutGain = driveOutGainSm.getNextValue();
        passbandGain = passbandGainSm.getNextValue();
        mix = mixSm.getNextValue();

        if(xOverFreqLowSm.isSmoothing()) {
            xOverFilterLow.setCutoffFrequency(xOverFreqLowSm.getNextValue());
        }

        if(xOverFreqHighSm.isSmoothing()) {
            xOverFilterHigh.setCutoffFrequency(xOverFreqHighSm.getNextValue());
        }

        for(int c = 0; c < numChannels; ++c)
        {
            auto* data = osBlock.getChannelPointer(c);
            for(int i = 0; i < osFactor; ++i) {
                data[(s * osFactor) + i] = processSample(data[(s * osFactor) + i], c);
            }
        }
    }

    overSamplers[osIndex]->processSamplesDown(block);
}

template<typename Type>
Type ProcessingModule<Type>::processSample(Type sample, int channel)
{
    Type low, mid, high;
    low = mid = high = zero;

    xOverFilterLow.processCrossover(sample, low, mid, channel);
    xOverFilterHigh.processCrossover(mid, mid, high, channel);

    auto cleanMid = mid;

    mid *= driveGain;
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
    mid *= driveOutGain;

    mid = cleanMid + ((mid - cleanMid) * mix);
    low *= passbandGain + ((one - passbandGain) * mix);
    high *= passbandGain + ((one - passbandGain) * mix);

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

    overSamplers[osIndex]->reset();
    osFactor = static_cast<int>(overSamplers[osIndex]->getOversamplingFactor());
    updateFilterSampleRate();
}

template<typename Type>
int ProcessingModule<Type>::getLatency()
{
    jassert(prepared);
    if(!prepared) {
        return 0;
    }

    return static_cast<int>(overSamplers[osIndex]->getLatencyInSamples());
}

//==============================================================================

template<typename Type>
void ProcessingModule<Type>::setClippingToUse(int negativeIndex, int positiveIndex)
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

template<typename Type>
void ProcessingModule<Type>::setDrive(Type driveAmountDecibels)
{
    auto gain = juce::Decibels::decibelsToGain(driveAmountDecibels);
    auto gainComp = static_cast<Type>(1.0) / std::atan(gain);

    driveGainSm.setTargetValue(gain);
    driveOutGainSm.setTargetValue(gainComp);
}

template<typename Type>
void ProcessingModule<Type>::setMix(Type dryWetMixRatio)
{
    mixSm.setTargetValue(dryWetMixRatio);
}

template<typename Type>
void ProcessingModule<Type>::setPassBandLevel(Type levelInDecibels)
{
    passbandGainSm.setTargetValue(juce::Decibels::decibelsToGain(levelInDecibels));
}

//==============================================================================

template<typename Type>
void ProcessingModule<Type>::resetSmoothers()
{
    xOverFreqLowSm.reset(sampleRate, smoothingTimeMs * 0.001);
    xOverFreqHighSm.reset(sampleRate, smoothingTimeMs * 0.001);
    negThresholdSm.reset(sampleRate, smoothingTimeMs * 0.001);
    posThresholdSm.reset(sampleRate, smoothingTimeMs * 0.001);
    passbandGainSm.reset(sampleRate, smoothingTimeMs * 0.001);
    driveGainSm.reset(sampleRate, smoothingTimeMs * 0.001);
    driveOutGainSm.reset(sampleRate, smoothingTimeMs * 0.001);
    mixSm.reset(sampleRate, smoothingTimeMs * 0.001);
}

//==============================================================================

template class ProcessingModule<float>;
template class ProcessingModule<double>;