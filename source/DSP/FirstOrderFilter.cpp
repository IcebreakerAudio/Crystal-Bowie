#include "FirstOrderFilter.h"

template<typename Type>
FirstOrderFilter<Type>::FirstOrderFilter()
{
    reset();
}

template<typename Type>
FirstOrderFilter<Type>::FirstOrderFilter(FilterType initType)
{
    setMode(initType);
    reset();
}

template<typename Type>
void FirstOrderFilter<Type>::reset()
{
    auto zero = static_cast<Type>(0.0);
    for(auto& f : fbk)
        f = zero;
}

template<typename Type>
void FirstOrderFilter<Type>::setNumChannels(int numChannels)
{
    fbk.resize(numChannels);
    reset();
}

template<typename Type>
void FirstOrderFilter<Type>::setSampleRate(double newSampleRate)
{
    sampleRate = newSampleRate;
    invSampleRate = static_cast<Type>(1.0 / sampleRate);
    maxFrequency = std::min(sampleRate * 0.5, 20000.0);
    if(cutoff > maxFrequency)
        cutoff = maxFrequency;
    
    updateCoefficients();
}

template<typename Type>
void FirstOrderFilter<Type>::setCutoffFrequency(double frequency)
{
    cutoff = frequency > maxFrequency ? maxFrequency : frequency;
    updateCoefficients();
}

template<typename Type>
Type FirstOrderFilter<Type>::processSample(Type in, int channel)
{
    auto y = (in - fbk[channel]) * g;
    auto x = fbk[channel] + y;
    fbk[channel] = x + y;

    if(filterType == FilterType::Lowpass)
        return x;
    else
        return in - x;
}

template<typename Type>
void FirstOrderFilter<Type>::processCrossover(Type in, Type& lowpassOutput, Type& highpassOutput, int channel)
{
    auto y = (in - fbk[channel]) * g;
    auto x = fbk[channel] + y;
    fbk[channel] = x + y;

    lowpassOutput = x;
    highpassOutput = in - x;
}

template<typename Type>
void FirstOrderFilter<Type>::updateCoefficients()
{
    auto w = std::tan(cutoff * invSampleRate * std::numbers::pi_v<Type>);
    g = w / (w + 1.0);
}

template<typename Type>
void FirstOrderFilter<Type>::snapToZero()
{
    auto zero = static_cast<Type>(0.0);
    for(auto& f : fbk)
    {
        if (! (f < -1.0e-8f || f > 1.0e-8f))
            f = zero;
    }
}
