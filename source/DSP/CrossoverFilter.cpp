#include "CrossoverFilter.hpp"

template<typename Type>
CrossoverFilter<Type>::CrossoverFilter()
{
    reset();
}

template<typename Type>
void CrossoverFilter<Type>::reset()
{
    auto zero = static_cast<Type>(0.0);
    std::fill(s1.begin(), s1.end(), zero);
    std::fill(s2.begin(), s2.end(), zero);
    std::fill(s3.begin(), s3.end(), zero);
}

template<typename Type>
void CrossoverFilter<Type>::setNumChannels(int numChannels)
{
    s1.resize(numChannels);
    s2.resize(numChannels);
    s3.resize(numChannels);
    reset();
}

template<typename Type>
void CrossoverFilter<Type>::setSampleRate(double newSampleRate)
{
    sampleRate = newSampleRate;
    invSampleRate = static_cast<Type>(1.0 / sampleRate);
    maxFrequency = std::min(sampleRate * 0.5, 20000.0);
    if(cutoff > maxFrequency) {
        cutoff = maxFrequency;
    }

    updateCoefficients();
}

template<typename Type>
void CrossoverFilter<Type>::setCutoffFrequency(double frequency)
{
    cutoff = frequency > maxFrequency ? maxFrequency : frequency;
    updateCoefficients();
}

template<typename Type>
Type CrossoverFilter<Type>::processSingle(Type in, std::vector<Type>& fbk, int channel)
{
    auto y = (in - fbk[channel]) * g;
    auto x = fbk[channel] + y;
    fbk[channel] = x + y;

    return x;
}

template<typename Type>
void CrossoverFilter<Type>::processCrossover(Type in, Type& lowpassOutput, Type& highpassOutput, int channel)
{
    auto lp1 = processSingle(in, s1, channel);
    auto hp1 = in - lp1;

    lowpassOutput = processSingle(lp1, s2, channel);
    highpassOutput = processSingle(hp1, s3, channel) - hp1;
}

template<typename Type>
void CrossoverFilter<Type>::updateCoefficients()
{
    auto w = std::tan(static_cast<Type>(cutoff) * invSampleRate * std::numbers::pi_v<Type>);
    g = w / (w + static_cast<Type>(1.0));
}

template<typename Type>
void CrossoverFilter<Type>::snapToZero()
{
    return;
    // to do: snap to zero
}

//==============================================================================
template class CrossoverFilter<float>;
template class CrossoverFilter<double>;
