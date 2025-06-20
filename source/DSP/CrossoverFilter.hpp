
#pragma once

#include <vector>
#include <cmath>
#include <numbers>
#include <algorithm>

template<typename Type>
class CrossoverFilter
{
public:

	CrossoverFilter();

	void reset();
    void setNumChannels(int numChannels);
    void setSampleRate(double newSampleRate);
	void setCutoffFrequency(double frequency);

    void processCrossover(Type in, Type& lowpassOutput, Type& highpassOutput, int channel = 0);

    void snapToZero();

private:

    void updateCoefficients();

    Type processSingle(Type in, std::vector<Type>& fbk, int channel = 0);

    double sampleRate = 48000.0, cutoff = 500.0, maxFrequency = 20000.0;
	Type g = 0.0, invSampleRate = 1.0 / 48000.0;
	std::vector<Type> s1 { 1 },  s2 { 1 },  s3 { 1 };
};
