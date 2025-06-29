#pragma once
#include "SpectrumAnalyser.hpp"

SpectrumAnalyser::SpectrumAnalyser(int fftOrder, bool useMultiResolution)
    : juce::Thread ("Analyser"), fft {fftOrder},
      windowing { size_t (1 << fftOrder), juce::dsp::WindowingFunction<float>::blackmanHarris, true }
{
    useHQBass = useMultiResolution;
    fftSize = fft.getSize();

    fftInBuffer.resize(fftSize);
    fftProcessBuffer.resize(fftSize * 2);

    reset();
}

SpectrumAnalyser::~SpectrumAnalyser()
{
    dataWaitFlag.notify_all();
    stopThread(500);
}

void SpectrumAnalyser::addAudioData (const juce::AudioBuffer<float>& buffer, int numChannels)
{
    if (audioStreamFifo.isFull()) {
        return;
    }
    audioStreamFifo.addToFifo(buffer, numChannels);
    dataWaitFlag.notify_all();
}

void SpectrumAnalyser::reset()
{
    std::fill(fftInBuffer.begin(), fftInBuffer.end(), 0.0f);
    std::fill(fftProcessBuffer.begin(), fftProcessBuffer.end(), 0.0f);
    std::fill(outputData.begin(), outputData.end(), 0.0f);
    if(useHQBass) {
        std::fill(bassBuffer.begin(), bassBuffer.end(), 0.0f);
    }
}

void SpectrumAnalyser::prepare (int audioFifoSize, double sampleRate)
{
    externalSampleRate = static_cast<float>(sampleRate);
    internalSampleRate = externalSampleRate;
    useDownSampling = false;
    auto sizeFactor = 1;
    while(internalSampleRate > 80000.0f) {
        internalSampleRate *= 0.5f;
        sizeFactor *= 2;
        useDownSampling = true;
    }

    if(useDownSampling)
    {
        auto resampleRatio = externalSampleRate / internalSampleRate;
        downSamplingFilter.setResamplingRatio(resampleRatio);
        downSamplingFilter.prepare(1, fftSize * sizeFactor);
    }

    if(useHQBass)
    {
        halfSampleFilter.setResamplingRatio(2.0);
        halfSampleFilter.prepare(1, fftSize);
        bassStreamFiFo.setSize(audioFifoSize);
        bassBuffer.resize(fftSize);
    }

    audioStreamFifo.setSize(audioFifoSize);
    fftInBuffer.resize(fftSize * sizeFactor);

    cutoffFreq = useHQBass ? float(internalSampleRate * 0.05) : 0.0f;

    reset();

    startThread(juce::Thread::Priority::normal);
}

void SpectrumAnalyser::run()
{
    while (!threadShouldExit())
    {
        if (audioStreamFifo.getSizeToRead() >= fftInBuffer.size())
        {
            auto samplesPassed = samplesSinceLastRead.load();
            samplesSinceLastRead.store(audioStreamFifo.getSizeToRead() + samplesPassed);
            while(audioStreamFifo.getSizeToRead() >= fftInBuffer.size())
            {
                audioStreamFifo.readFromFifo(fftInBuffer.data(), fftInBuffer.size());

                if (useDownSampling) {
                    downSamplingFilter.processMono(fftInBuffer, fftProcessBuffer, fftInBuffer.size());
                }
                else {
                    std::copy(fftInBuffer.begin(), fftInBuffer.end(), fftProcessBuffer.begin());
                }

                if(useHQBass)
                {
                    halfSampleFilter.processMono(fftProcessBuffer, bassBuffer, fftSize);
                    bassStreamFiFo.addToFifo(bassBuffer.data(), fftSize / 2);
                }
            }
            audioStreamFifo.reset();

            windowing.multiplyWithWindowingTable (fftProcessBuffer.data(), size_t (fftSize));
            fft.performFrequencyOnlyForwardTransform (fftProcessBuffer.data());

            std::lock_guard lockedForWriting (readWriteMutex);

            for(int i = 0; i < outputData.size(); ++i)
            {
                auto freq = indexToFrequency(float(i));
                if(freq < cutoffFreq) {
                    continue;
                }

                auto bin = frequencyToBin(freq);
                auto index = int(floor(bin));
                auto alpha = bin - float(index);
                auto value = fftProcessBuffer[index] + ((fftProcessBuffer[index + 1] - fftProcessBuffer[index]) * alpha);

                value = value / float(fftSize);
                if(value > outputData[i]) {
                    outputData[i] = value;
                }
            }

            newDataAvailable = true;
        }

        if (useHQBass && bassStreamFiFo.getSizeToRead() >= fftSize)
        {
            while(bassStreamFiFo.getSizeToRead() >= fftSize) {
                bassStreamFiFo.readFromFifo(fftProcessBuffer.data(), fftSize);
            }

            windowing.multiplyWithWindowingTable (fftProcessBuffer.data(), size_t (fftSize));
            fft.performFrequencyOnlyForwardTransform (fftProcessBuffer.data());

            std::lock_guard lockedForWriting (readWriteMutex);

            for(int i = 0; i < outputData.size(); ++i)
            {
                auto freq = indexToFrequency(float(i));
                if(freq >= cutoffFreq * 2.0f) {
                    break;
                }

                auto bin = frequencyToBassBin(freq);
                auto index = int(floor(bin));
                auto alpha = bin - float(index);
                auto value = fftProcessBuffer[index] + ((fftProcessBuffer[index + 1] - fftProcessBuffer[index]) * alpha);

                value = value / float(fftSize);
                if(freq > cutoffFreq) {
                    value += (outputData[i] - value) * ((freq - cutoffFreq) / cutoffFreq);
                }

                if(value > outputData[i]) {
                    outputData[i] = value;
                }
            }

            newDataAvailable = true;
        }

        if (audioStreamFifo.getSizeToRead() < fftSize) {
            std::unique_lock ul (dataWaitMutex);
            dataWaitFlag.wait(ul);
        }
    }
}

void SpectrumAnalyser::createLinePath (juce::Path& p, const juce::Rectangle<float> bounds)
{
    auto samplesPassed = samplesSinceLastRead.exchange(0);
    if(samplesPassed == 0) {
        return;
    }

    p.clear();
    p.preallocateSpace (8 + outputData.size() * 3);

    std::lock_guard lockedForReading (readWriteMutex);
    auto secondsPassed = float(samplesPassed) / externalSampleRate;
    auto decayMult = std::pow(decayFactor, secondsPassed);

    auto width = bounds.getWidth();
    auto size = float(outputData.size() - 1);

    applyPathSmoothing(outputData, smoothedData);

    auto oX = bounds.getX();
    auto oY = bounds.getY();
    p.startNewSubPath(oX, oY + juce::jmap(juce::Decibels::gainToDecibels(smoothedData[0]), -72.0f, 0.0f, bounds.getBottom(), bounds.getY()));

    for(int i = 1; i < smoothedData.size(); ++i)
    {
        auto xPos = i * width / size;
        auto yPos = juce::jmap (juce::Decibels::gainToDecibels(smoothedData[i]), -72.0f, 0.0f, bounds.getBottom(), bounds.getY());
        p.lineTo(oX + xPos, oY + yPos);
    }

    std::transform(outputData.begin(), outputData.end(), outputData.begin(),
        [decayMult] (float value)
        {
            if(value < 0.0001f) {
                return 0.0f;
            }
            return value * decayMult;
        }
    );
}

void SpectrumAnalyser::createBarPath (juce::Path& p, const juce::Rectangle<float> bounds)
{
}

void SpectrumAnalyser::setRange(float minimumFreq, float maximumFreq, int resolution, bool useLogScale)
{
    outputData.resize(resolution);
    std::fill(outputData.begin(), outputData.end(), 0.0f);

    minFreq = minimumFreq;
    maxFreq = maximumFreq;
    logScale = useLogScale;
    if(logScale) {
        scaleFactor = std::log(maximumFreq / minimumFreq);
    }
    else {
        scaleFactor = (maximumFreq - minimumFreq) / float(resolution - 1);
    }
}

void SpectrumAnalyser::setLineSmoothing (int lineSmoothing)
{
    useSmoothing = lineSmoothing > 0;

    if(!useSmoothing)
    {
        kernelSize = 0;
        kernelWeights.clear();
        smoothedData.clear();
        
        return;
    }

    if(lineSmoothing % 2 == 0) {
        lineSmoothing = lineSmoothing + 1;
    }

    if(smoothedData.size() != outputData.size()) {
        smoothedData.resize(outputData.size());
    }

    kernelSize = lineSmoothing;
    generateSmoothingWeights(kernelSize);
}

void SpectrumAnalyser::setDecayTime(float decayTimeMs)
{
    decayTime = decayTimeMs * 0.001f;
    decayFactor = std::pow(0.01f, 1.0f / (decayTime));
}
