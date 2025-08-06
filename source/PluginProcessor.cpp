#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
     : AudioProcessor (BusesProperties()
       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
       apvts(*this, nullptr, "PARAMETERS", createParameters())
{
    apvts.addParameterListener("OS", &osParamListener);
    
    apvts.addParameterListener("modePos", &clippingParamListener);
    apvts.addParameterListener("modeNeg", &clippingParamListener);
    apvts.addParameterListener("sym", &clippingParamListener);
    apvts.addParameterListener("drive", &clippingParamListener);
    
    apvts.addParameterListener("active", &mainParamListener);
    apvts.addParameterListener("outGain", &mainParamListener);
    apvts.addParameterListener("xOverLow", &mainParamListener);
    apvts.addParameterListener("xOverHigh", &mainParamListener);
    apvts.addParameterListener("filter", &mainParamListener);
    apvts.addParameterListener("mix", &mainParamListener);
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout AudioPluginAudioProcessor::createParameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID{ "active", 1 },
                "On/Off",
                true));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{ "drive", 1 },
                "Drive",
                juce::NormalisableRange<float>(-12.0f, 48.0f, 0.01f, 1.5f),
                0.0f,
                juce::AudioParameterFloatAttributes().withLabel("dB")
                ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{ "outGain", 1 },
                "Output",
                juce::NormalisableRange<float>(-60.0f, 12.0f, 0.01f, 1.5f),
                0.0f,
                juce::AudioParameterFloatAttributes().withLabel("dB")
                ));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID{ "modePos", 1 },
                "Clip Mode +",
                Clipper::Names,
                0));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID{ "modeNeg", 1 },
                "Clip Mode -",
                Clipper::Names,
                0));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{ "sym", 1 },
                "Symmetry",
                juce::NormalisableRange<float>(-100.0f, 100.0f, 0.01f),
                0.0f,
                juce::AudioParameterFloatAttributes()
                ));

    auto freqRange = juce::NormalisableRange<float>(20.0f, 19500.0f,
        [](float currentRangeStart, float currentRangeEnd, float normalisedValue)
        {
            // convert from 0 to 1
            auto factor = std::log(currentRangeEnd / currentRangeStart);
            return std::exp(normalisedValue * factor) * currentRangeStart;
        },
        [](float currentRangeStart, float currentRangeEnd, float value)
        {
            // convert to 0 to 1
            auto factor = std::log(currentRangeEnd / currentRangeStart);
            return std::log(value / currentRangeStart) / factor;
        }
    );
    freqRange.interval = 0.01f;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{ "xOverLow", 1 },
                "Low Crossover",
                freqRange,
                200.0f,
                juce::AudioParameterFloatAttributes().withLabel("Hz")
                ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{ "xOverHigh", 1 },
                "High Crossover",
                freqRange,
                5000.0f,
                juce::AudioParameterFloatAttributes().withLabel("Hz")
                ));

    layout.add(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID{ "filter", 1 },
                "Mute High/Low",
                false));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{ "mix", 1 },
                "Mix",
                juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
                100.0f,
                juce::AudioParameterFloatAttributes().withLabel("%")
                ));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID{ "OS", 1 },
                "Oversampling",
                juce::StringArray{ "Off", "x2", "x4", "x8", "x16" },
                1,
                juce::AudioParameterChoiceAttributes().withAutomatable(false)));

    return layout;
}

//==============================================================================
void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    auto numChannels = getTotalNumInputChannels();

    floatProcessor.reset();
    doubleProcessor.reset();

    spectrumProcessor.prepare(juce::roundToInt(sampleRate * 0.5), sampleRate);
    spectrumProcessor.setFreqRange(20.0f, 19500.0f, 200);
    spectrumProcessor.setDecibelRange(-96.0f, 0.0f);
    spectrumProcessor.setLineSmoothing(9);
    spectrumProcessor.setDecayTime(1000.0f);

    auto spec = juce::dsp::ProcessSpec(sampleRate, samplesPerBlock, numChannels);

    if(isUsingDoublePrecision())
    {
        doubleProcessor = std::make_unique<ProcessingModule<double>>();
        doubleProcessor->prepare(numChannels, samplesPerBlock);
        doubleProcessor->setSampleRate(sampleRate);
        
        const auto latency = doubleProcessor->getMaxLatency();

        doubleDelay = std::make_unique<juce::dsp::DelayLine<double, juce::dsp::DelayLineInterpolationTypes::None>>();
        doubleDelay->prepare(spec);
        doubleDelay->setMaximumDelayInSamples(latency + 1);
        doubleDelay->setDelay(double(latency));

        setLatencySamples(latency);
    }
    else
    {
        floatProcessor = std::make_unique<ProcessingModule<float>>();
        floatProcessor->prepare(numChannels, samplesPerBlock);
        floatProcessor->setSampleRate(sampleRate);

        const auto latency = floatProcessor->getMaxLatency();

        floatDelay = std::make_unique<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None>>();
        floatDelay->prepare(spec);
        floatDelay->setMaximumDelayInSamples(latency + 1);
        floatDelay->setDelay(float(latency));

        setLatencySamples(latency);
    }

    floatGainSmoother.reset(sampleRate, smoothingTimeMs * 0.001);
    doubleGainSmoother.reset(sampleRate, smoothingTimeMs * 0.001);

    updateAllParameters();
}

void AudioPluginAudioProcessor::releaseResources()
{
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainInputChannels() > layouts.getMainOutputChannels())
        return false;

    return true;
}

void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    jassert(!isUsingDoublePrecision());

    if(isUsingDoublePrecision()) {
        return;
    }

    if(floatProcessor == nullptr) {
        return;
    }

    if(osParamListener.checkForChanges()) {
        updateOverSampling();
    }

    if(mainParamListener.checkForChanges()) {
        updateMainParameters();
    }

    if(clippingParamListener.checkForChanges()) {
        updateClippingParameters();
    }

    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto numSamples = buffer.getNumSamples();

    spectrumProcessor.addAudioData(buffer, totalNumInputChannels);

    auto block = juce::dsp::AudioBlock<float>(buffer).getSubsetChannelBlock(0, totalNumInputChannels);

    floatProcessor->processBlock(block);
    floatGainSmoother.applyGain(buffer, numSamples);

    min.store(floatProcessor->getMin());
    max.store(floatProcessor->getMax());

    if(totalNumOutputChannels > totalNumInputChannels)
    {
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
    }
}

void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<double>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    jassert(isUsingDoublePrecision());

    if(!isUsingDoublePrecision()) {
        return;
    }

    if(doubleProcessor == nullptr) {
        return;
    }

    if(osParamListener.checkForChanges()) {
        updateOverSampling();
    }

    if(mainParamListener.checkForChanges()) {
        updateMainParameters();
    }

    if(clippingParamListener.checkForChanges()) {
        updateClippingParameters();
    }

    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto numSamples = buffer.getNumSamples();

    auto block = juce::dsp::AudioBlock<double>(buffer).getSubsetChannelBlock(0, totalNumInputChannels);

    doubleProcessor->processBlock(block);
    doubleGainSmoother.applyGain(buffer, numSamples);

    min.store(static_cast<float>(doubleProcessor->getMin()));
    max.store(static_cast<float>(doubleProcessor->getMax()));

    if(totalNumOutputChannels > totalNumInputChannels)
    {
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
    }
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor (*this);
    // return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Store
    auto copyState = apvts.copyState();
    auto xml = copyState.createXml();

    xml->setAttribute("SizeRatio", static_cast<double>(interfaceSizeRatio));

    copyXmlToBinary(*xml.get(), destData);
}

void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Restore
    auto xml = getXmlFromBinary(data, sizeInBytes);

    interfaceSizeRatio = static_cast<float>(xml->getDoubleAttribute("SizeRatio", 1.0));
    xml->removeAttribute("SizeRatio");
    updateInterface.store(true);

    auto copyState = juce::ValueTree::fromXml(*xml.get());
    apvts.replaceState(copyState);
}

//==============================================================================

void AudioPluginAudioProcessor::updateOverSampling()
{
    auto osIndex = juce::roundToInt(loadRawParameterValue("OS"));

    if(floatProcessor) {
        floatProcessor->setOverSampleIndex(osIndex);
    }

    if(doubleProcessor) {
        doubleProcessor->setOverSampleIndex(osIndex);
    }
}

void AudioPluginAudioProcessor::updateMainParameters()
{
    auto active = loadRawParameterValue("active") > 0.5f;

    auto outGain = juce::Decibels::decibelsToGain(loadRawParameterValue("outGain"));

    auto lowFreq = static_cast<double>(loadRawParameterValue("xOverLow"));
    auto highFreq = static_cast<double>(loadRawParameterValue("xOverHigh"));
    auto pbLevel = loadRawParameterValue("filter") > 0.5f ? -96.0f : 0.0f;
    auto mix = loadRawParameterValue("mix") * 0.01f;

    if(!active) {
        outGain = 1.0f;
        mix = 0.0f;
    }

    floatGainSmoother.setTargetValue(outGain);
    doubleGainSmoother.setTargetValue(static_cast<double>(outGain));

    if(floatProcessor) {
        floatProcessor->setCrossoverFrequencies(lowFreq, highFreq);
        floatProcessor->setPassBandLevel(pbLevel);
        floatProcessor->setMix(mix);
    }

    if(doubleProcessor) {
        doubleProcessor->setCrossoverFrequencies(lowFreq, highFreq);
        doubleProcessor->setPassBandLevel(static_cast<double>(pbLevel));
        doubleProcessor->setMix(static_cast<double>(mix));
    }
}

void AudioPluginAudioProcessor::updateClippingParameters()
{
    auto posIndex = juce::roundToInt(loadRawParameterValue("modePos"));
    auto negIndex = juce::roundToInt(loadRawParameterValue("modeNeg"));

    auto symmetry = loadRawParameterValue("sym") * 0.009f;
    auto negThresh = -1.0f;
    auto posThresh = 1.0f;

    if(symmetry < 0.0f) {
        posThresh = 1.0f + symmetry;
    }
    else {
        negThresh = symmetry - 1.0f;
    }
    
    auto drive = loadRawParameterValue("drive");

    if(floatProcessor) {
        floatProcessor->setClippingToUse(negIndex, posIndex);
        floatProcessor->setThresholds(negThresh, posThresh);
        floatProcessor->setDrive(drive);
    }

    if(doubleProcessor) {
        doubleProcessor->setClippingToUse(negIndex, posIndex);
        doubleProcessor->setThresholds(negThresh, posThresh);
        doubleProcessor->setDrive(drive);
    }
}

void AudioPluginAudioProcessor::updateAllParameters()
{
    updateOverSampling();
    updateClippingParameters();
    updateMainParameters();
}

//==============================================================================

void AudioPluginAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);

    if(!floatDelay) {
        return;
    }

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto numSamples = buffer.getNumSamples();
    
    auto block = juce::dsp::AudioBlock<float>(buffer).getSubsetChannelBlock(0, totalNumInputChannels);
    auto context = juce::dsp::ProcessContextReplacing(block);
    floatDelay->process(context);

    if(totalNumOutputChannels > totalNumInputChannels)
    {
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
    }
}

void AudioPluginAudioProcessor::processBlockBypassed (juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);

    if(!doubleDelay) {
        return;
    }

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto numSamples = buffer.getNumSamples();
    
    auto block = juce::dsp::AudioBlock<double>(buffer).getSubsetChannelBlock(0, totalNumInputChannels);
    auto context = juce::dsp::ProcessContextReplacing(block);
    doubleDelay->process(context);

    if(totalNumOutputChannels > totalNumInputChannels)
    {
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
    }
}
