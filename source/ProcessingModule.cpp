#include "ProcessingModule.hpp"

template<typename Type>
ProcessingModule<Type>::ProcessingModule()
{
    clippers.push_back([](Type x) -> Type { return std::atan(x); });
    clippers.push_back(IADSP::BasicClippers::saturate<Type>);
    clippers.push_back(IADSP::BasicClippers::saturateRootSquared<Type>);
    clippers.push_back([](Type x) -> Type { return std::tanh(x); });
    clippers.push_back(IADSP::BasicClippers::polySoftClip<Type>);
    clippers.push_back(IADSP::BasicClippers::cubicSoftClip<Type>);
    clippers.push_back(IADSP::BasicClippers::hardClip<Type>);
    clippers.push_back(IADSP::BasicClippers::ripple<Type>);
}

template<typename Type>
void ProcessingModule<Type>::prepare(const int numChannels, const int expectedBufferSize)
{
    if(overSamplers.size() > 0) {
        overSamplers.clear();
    }

    maxLatency = 0;
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

        auto l = static_cast<int>(os->getLatencyInSamples());
        if(l > maxLatency) {
            maxLatency = l;
        }
    }

    xOverFilterLow.setNumChannels(numChannels);
    xOverFilterHigh.setNumChannels(numChannels);

    delaySpec.numChannels = numChannels;
    delaySpec.maximumBlockSize = expectedBufferSize;

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

    maxIn = minIn = zero;

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

    auto context = juce::dsp::ProcessContextReplacing(block);
    delay.process(context);
}

template<typename Type>
Type ProcessingModule<Type>::processSample(Type sample, int channel)
{
    Type low, mid, high;
    low = mid = high = zero;

    xOverFilterLow.processCrossover(sample, low, mid, channel);
    xOverFilterHigh.processCrossover(mid, mid, high, channel);

    auto cleanMid = mid;

    if(mid < minIn) {
        minIn = mid;
    }
    else if(mid > maxIn) {
        maxIn = mid;
    }

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
    low *= one + ((passbandGain - one) * mix);
    high *= one + ((passbandGain - one) * mix);

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

    delaySpec.sampleRate = newSampleRate;
    jassert(delaySpec.numChannels > 0 && delaySpec.maximumBlockSize > 0);

    delay.prepare(delaySpec);
    delay.setMaximumDelayInSamples(maxLatency);

    sampleRate = newSampleRate;
    updateFilterSampleRate();
    updateDelayTime();
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
void ProcessingModule<Type>::updateDelayTime()
{
    auto delayTime = static_cast<Type>(maxLatency) - overSamplers[osIndex]->getLatencyInSamples();
    if(delayTime < zero) {
        delayTime = zero;
    }
    delay.setDelay(delayTime);
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
    updateDelayTime();
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
    if(low != xOverFreqLowSm.getTargetValue()) {
        if(low > high) {
            low = high;
        }
        xOverFreqLowSm.setTargetValue(low);
    }
    
    if(high != xOverFreqHighSm.getTargetValue()) {
        if(high < low) {
            high = low;
        }
        xOverFreqHighSm.setTargetValue(high);
    }
}

template<typename Type>
void ProcessingModule<Type>::setDrive(Type driveAmountDecibels)
{
    auto gain = juce::Decibels::decibelsToGain(driveAmountDecibels);
    auto gainComp = one;
    if(gain >= one) {
        gainComp = one / std::log(gain * std::numbers::e_v<Type>);
    }
    else {
        gainComp = one / gain;
    }

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

template<>
void ProcessingModule<float>::drawPath(juce::Path& p, juce::Rectangle<float> bounds, float scale)
{
    const auto xO = bounds.getX();
    const auto yO = bounds.getY();
    const auto width = bounds.getWidth();
    const auto height = bounds.getHeight();
    const auto numPoints = juce::roundToInt(height);
    
    negGainOut = negThresholdSm.getTargetValue();
    negGainIn = one / negGainOut;

    posGainOut = posThresholdSm.getTargetValue();
    posGainIn = one / posGainOut;

    driveGain = driveGainSm.getTargetValue();
    driveOutGain = driveOutGainSm.getTargetValue();

    p.clear();
    p.preallocateSpace((numPoints * 3) + 8);

    for(int i = 0; i < numPoints; ++i)
    {
        auto x = float(i) * width / float(numPoints);
        auto value = (x * 2.0f / width) - 1.0f;
        if(value < 0.0f) {
            value *= -value;
        }
        else {
            value *= value;
        }

        value *= scale;
        value *= driveGain;
        if(value < zero)
        {
            value *= negGainIn;
            value = clippers[negClipIndex](value);
            value *= negGainOut;
            value = std::sqrt(std::abs(value)) * -1.0f;
        }
        else
        {
            value *= posGainIn;
            value = clippers[posClipIndex](value);
            value *= posGainOut;
            value = std::sqrt(value);
        }

        auto y = juce::jmap(value, -scale, scale, height, 0.0f);

        x += xO;
        y += yO;

        if(i == 0) {
            p.startNewSubPath(x, y);
        }
        else {
            p.lineTo(x, y);
        }
    }

    resetSmoothers();
}

template<>
void ProcessingModule<double>::drawPath(juce::Path& p, juce::Rectangle<float> bounds, float scale)
{
    jassertfalse; // only use the drawPath() function with float version 
    juce::ignoreUnused(p, bounds, scale);
    return;
}

//==============================================================================

template class ProcessingModule<float>;
template class ProcessingModule<double>;