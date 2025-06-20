#pragma once
#include <numbers>
#include <juce_audio_basics/juce_audio_basics.h>

class ResamplingFilter
{
public:

    void reset()
    {
        for(auto& fs : filterStates)
        {
            fs.x1 = 0.0;
            fs.x2 = 0.0;
            fs.y1 = 0.0;
            fs.y2 = 0.0;
        }
    }

    void prepare(int numChannels, int maximumInputBufferSize)
    {
        filterStates.resize(numChannels);
        internalBuffer.setSize(numChannels, maximumInputBufferSize);
        reset();
    }

    void setResamplingRatio(double newInToOutRatio)
    {
        ratio = newInToOutRatio;
        createLowPass(ratio);
    }

    void processMono (std::vector<float>& inputData, std::vector<float>& outputData, int samplesToProcess)
    {
        const int sampsNeeded = juce::roundToInt (samplesToProcess / ratio);
        jassert(inputData.size() >= samplesToProcess);
        jassert(outputData.size() >= sampsNeeded);
        
        processMono(inputData.data(), outputData.data(), samplesToProcess);
    }

    void processMono (float* inputData, float* outputData, int samplesToProcess)
    {
        const int sampsNeeded = juce::roundToInt (samplesToProcess / ratio);
        
        internalBuffer.copyFrom(0, 0, inputData, samplesToProcess);

        if (ratio > 1.0001)
        {
            // for down-sampling, pre-apply the filter..
            applyFilter (internalBuffer.getWritePointer(0), samplesToProcess, filterStates[0]);
        }

        auto subSampleOffset = 0.0;
        auto bufferPos = 0;
        int nextPos = (bufferPos + 1) % samplesToProcess;
        for (int s = 0; s < sampsNeeded; ++s)
        {
            const float alpha = (float) subSampleOffset;

            auto* data = internalBuffer.getWritePointer(0);
            auto value = data[bufferPos] + alpha * (data[nextPos] - data[bufferPos]);

            outputData[s] = value;
            subSampleOffset += ratio;

            while (subSampleOffset >= 1.0)
            {
                if (++bufferPos >= samplesToProcess)
                    bufferPos = 0;

                nextPos = (bufferPos + 1) % samplesToProcess;
                subSampleOffset -= 1.0;
            }
        }

        if (ratio < 0.9999)
        {
            // for up-sampling, post-apply the filter..
            applyFilter (outputData, sampsNeeded, filterStates[0]);
        }
    }

    void processBuffer (juce::AudioBuffer<float>& inputBuffer, juce::AudioBuffer<float>& outputBuffer, int samplesToProcess)
    {
        const int sampsNeeded = juce::roundToInt (samplesToProcess * ratio);
        jassert(sampsNeeded <= outputBuffer.getNumSamples());
        const int channelsToProcess = juce::jmin (inputBuffer.getNumChannels(), outputBuffer.getNumChannels());
        
        for (int c = 0; c < channelsToProcess; ++c) {
            internalBuffer.copyFrom(c,0,inputBuffer,c,0,samplesToProcess);
        }

        if (ratio > 1.0001)
        {
            // for down-sampling, pre-apply the filter..
            for (int c = 0; c < channelsToProcess; ++c) {
                applyFilter (internalBuffer.getWritePointer(c), samplesToProcess, filterStates[c]);
            }
        }

        auto subSampleOffset = 0.0;
        auto bufferPos = 0;
        int nextPos = (bufferPos + 1) % samplesToProcess;
        for (int s = 0; s < sampsNeeded; ++s)
        {
            const float alpha = (float) subSampleOffset;

            for (int c = 0; c < channelsToProcess; ++c)
            {
                auto* data = internalBuffer.getWritePointer(c);
                auto value = data[bufferPos] + alpha * (data[nextPos] - data[bufferPos]);

                outputBuffer.setSample(c, s, value);
            }
            subSampleOffset += ratio;

            while (subSampleOffset >= 1.0)
            {
                if (++bufferPos >= samplesToProcess)
                    bufferPos = 0;

                nextPos = (bufferPos + 1) % samplesToProcess;
                subSampleOffset -= 1.0;
            }
        }

        if (ratio < 0.9999)
        {
            // for up-sampling, post-apply the filter..
            for (int c = 0; c < channelsToProcess; ++c) {
                applyFilter (outputBuffer.getWritePointer(c), sampsNeeded, filterStates[c]);
            }
        }
    }

private:
    
    struct FilterState
    {
        double x1, x2, y1, y2;
    };

    juce::AudioBuffer<float> internalBuffer;
    std::array<double, 6> coefficients { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    std::vector<FilterState> filterStates { 1 };
    double ratio = 1.0;

    void createLowPass (const double frequencyRatio)
    {
        const double proportionalRate = (frequencyRatio > 1.0) ? 0.5 / frequencyRatio
                                                               : 0.5 * frequencyRatio;

        const double n = 1.0 / std::tan (std::numbers::pi * juce::jmax (0.001, proportionalRate));
        const double nSquared = n * n;
        const double c1 = 1.0 / (1.0 + std::numbers::sqrt2 * n + nSquared);

        coefficients[0] = c1;
        coefficients[1] = c1 * 2.0;
        coefficients[2] = c1;
        coefficients[3] = 1.0;
        coefficients[4] = c1 * 2.0 * (1.0 - nSquared);
        coefficients[5] = c1 * (1.0 - std::numbers::sqrt2 * n + nSquared);
    }

    void applyFilter (float* samples, int num, FilterState& fs)
    {
        while (--num >= 0)
        {
            const double in = *samples;

            double out = coefficients[0] * in
                        + coefficients[1] * fs.x1
                        + coefficients[2] * fs.x2
                        - coefficients[4] * fs.y1
                        - coefficients[5] * fs.y2;

            #if JUCE_INTEL
                if (! (out < -1.0e-8 || out > 1.0e-8))
                    out = 0.0;
            #endif

            fs.x2 = fs.x1;
            fs.x1 = in;
            fs.y2 = fs.y1;
            fs.y1 = out;

            *samples++ = (float) out;
        }
    }
};