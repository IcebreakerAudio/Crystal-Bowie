
#pragma once

#include <vector>
#include <cmath>
#include <numbers>
#include <algorithm>

enum struct FirstOrderFilterMode
{
    Lowpass,
    Highpass
};

template<typename Type>
class FirstOrderFilter
{
public:

    using FilterType = FirstOrderFilterMode;

	FirstOrderFilter();

	FirstOrderFilter(FilterType initType);

	void reset();
    void setMode(FilterType newType) { filterType = newType; }
    void setNumChannels(int numChannels);
    void setSampleRate(double newSampleRate);
	void setCutoffFrequency(double frequency);

	Type processSample(Type in, int channel = 0);
    void processCrossover(Type in, Type& lowpassOutput, Type& highpassOutput, int channel = 0);

    void snapToZero();

private:

    void updateCoefficients();

    double sampleRate = 48000.0, cutoff = 500.0, maxFrequency = 20000.0;
	Type g = 0.0, invSampleRate = 1.0 / 48000.0;
	std::vector<Type> fbk { 1 };
    FilterType filterType = FilterType::Lowpass;
};
