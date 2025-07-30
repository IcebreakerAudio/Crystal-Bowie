
#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <mutex>
#include <condition_variable>

#include <IA_Utilities/FiFo.hpp>
#include <IA_Utilities/ResamplingFilter.hpp>

//==============================================================================
/*
*/

class SpectrumAnalyser : public juce::Thread
{
public:
    SpectrumAnalyser(int fftOrder, bool useMultiResolution);

    ~SpectrumAnalyser() override;

    void addAudioData (const juce::AudioBuffer<float>& buffer, int numChannels);
    void prepare (int audioFifoSize, double sampleRate);
    void reset();
    void run() override;

    void createLinePath (juce::Path& p, const juce::Rectangle<float> bounds);
    void createBarPath (juce::Path& p, const juce::Rectangle<float> bounds);

    void setFreqRange(float minimumFreq, float maximumFreq, int resolution, bool useLogScale = true);
    void setDecibelRange(float min, float max);
    void setDecayTime(float decayTimeMs);
    void setLineSmoothing(int lineSmoothing);

    bool checkForNewData() { return newDataAvailable.exchange(false); }

private:

    inline float indexToFrequency (float index) const
    {
        if(logScale) {
            return std::exp(index * scaleFactor / float(outputData.size() - 1)) * minFreq;
        }
        else {
            return (index * scaleFactor) + minFreq;
        }
    }

    inline float frequencyToBin (float freq) const
    {
        if(freq > internalSampleRate || freq < 0.0f) {
            return -1.0f;
        }

        return freq * float(fftSize) / internalSampleRate;
    }

    inline float frequencyToBassBin (float freq) const
    {
        if(freq > internalSampleRate * 0.5 || freq < 0.0f) {
            return -1.0f;
        }

        return freq * float(fftSize) / float(internalSampleRate * 0.5);
    }

    void generateSmoothingWeights(int n)
    {
        kernelWeights.resize(n + 1);

        float C = 1.0f;
        kernelWeights[0] = C;

        for (int i = 1; i <= n; ++i) {
            C *= float(n - i + 1) / float(i);
            kernelWeights[i] = C;
        }
    }

    void applyPathSmoothing(std::vector<float>& source, std::vector<float>& dest)
    {
        jassert(dest.size() >= source.size());

        if(!useSmoothing) {
            std::copy_n(source.begin(), source.size(), dest.begin());
        }

        const int kernelRadius = (kernelSize - 1) / 2;

        for (int i = 0; i < source.size(); ++i)
        {
            float sum = 0.0f;
            float weightSum = 0.0f;

            for (int j = -kernelRadius; j <= kernelRadius; ++j)
            {
                auto neighbourIdx = i + j;

                if (neighbourIdx >= 0 && neighbourIdx < source.size())
                {
                    float weight = kernelWeights[j + kernelRadius];
                    sum += source[neighbourIdx] * weight;
                    weightSum += weight;
                }
            }

            if (weightSum > 0.0f) {
                dest[i] = sum / weightSum;
            } else {
                dest[i] = source[i];
            }
        }
    }

    int fftSize = 0;
    float decayTime = 1.0f, decayFactor = 0.9f;
    float cutoffFreq = 2400.0f;

    float externalSampleRate = 48000.0f, internalSampleRate = 48000.0f;

    std::condition_variable dataWaitFlag;
    std::recursive_mutex readWriteMutex;
    std::mutex dataWaitMutex;

    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> windowing;
    ResamplingFilter downSamplingFilter, halfSampleFilter;

    std::vector<float> fftInBuffer, fftProcessBuffer, bassBuffer;
    std::vector<float> outputData { 128 }, smoothedData { 128 };
    Fifo<float> audioStreamFifo, bassStreamFiFo;

    bool useSmoothing = false;
    std::vector<float> kernelWeights;
    int kernelSize = 0;

    bool useHQBass = true;
    bool useDownSampling = false;

    bool logScale = true;
    float scaleFactor = 9.9f;
    float minFreq = 20.0f, maxFreq = 20000.0f;
    float dbMin = -96.0f, dbMax = 0.0f;

    std::atomic<bool> newDataAvailable = false;
    std::atomic<int> samplesSinceLastRead = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyser)
};
