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
    apvts.addParameterListener("pbLevel", &mainParamListener);
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
                juce::NormalisableRange<float>(-100.0f, 100.0f),
                0.0f,
                juce::AudioParameterFloatAttributes()
                ));

    auto freqRange = juce::NormalisableRange<float>(20.0f, 15000.0f);
    freqRange.setSkewForCentre(1000.0f);

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

    layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{ "pbLevel", 1 },
                "Pass Band Level",
                juce::NormalisableRange<float>(-60.0f, 12.0f, 0.01f, 1.5f),
                0.0f,
                juce::AudioParameterFloatAttributes().withLabel("dB")
                ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{ "mix", 1 },
                "Mix",
                juce::NormalisableRange<float>(0.0f, 100.0f),
                100.0f,
                juce::AudioParameterFloatAttributes().withLabel("%")
                ));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID{ "OS", 1 },
                "Oversampling",
                juce::StringArray{ "Off", "x2", "x4", "x8", "x16" },
                1));

    return layout;
}

//==============================================================================
void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    auto numChannels = getTotalNumInputChannels();

    floatProcessor.reset();
    doubleProcessor.reset();

    if(isUsingDoublePrecision())
    {
        DBG("is Double");
        doubleProcessor = std::make_unique<ProcessingModule<double>>();
        doubleProcessor->prepare(numChannels, samplesPerBlock);
        doubleProcessor->setSampleRate(sampleRate);
    }
    else
    {
        DBG("is Float");
        floatProcessor = std::make_unique<ProcessingModule<float>>();
        floatProcessor->prepare(numChannels, samplesPerBlock);
        floatProcessor->setSampleRate(sampleRate);
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

    auto block = juce::dsp::AudioBlock<float>(buffer).getSubsetChannelBlock(0, totalNumInputChannels);

    floatProcessor->processBlock(block);
    floatGainSmoother.applyGain(buffer, numSamples);
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
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    // return new AudioPluginAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ignoreUnused (destData);
}

void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ignoreUnused (data, sizeInBytes);
}

//==============================================================================

void AudioPluginAudioProcessor::updateOverSampling()
{
    auto osIndex = juce::roundToInt(loadRawParameterValue("OS"));
    DBG("OS Index:");
    DBG(osIndex);

    if(floatProcessor) {
        floatProcessor->setOverSampleIndex(osIndex);
    }

    if(doubleProcessor) {
        doubleProcessor->setOverSampleIndex(osIndex);
    }
}

void AudioPluginAudioProcessor::updateMainParameters()
{
    auto outGain = juce::Decibels::decibelsToGain(loadRawParameterValue("outGain"));

    floatGainSmoother.setTargetValue(outGain);
    doubleGainSmoother.setTargetValue(static_cast<double>(outGain));

    auto lowFreq = static_cast<double>(loadRawParameterValue("xOverLow"));
    auto highFreq = static_cast<double>(loadRawParameterValue("xOverHigh"));
    auto pbLevel = loadRawParameterValue("pbLevel");
    auto mix = loadRawParameterValue("mix") * 0.01f;

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

    auto symmetry = loadRawParameterValue("sym") * 0.01f;
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
