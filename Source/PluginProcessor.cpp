#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

DeReverbenjaminProcessor::DeReverbenjaminProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout DeReverbenjaminProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Etapa 1: Spectral De-Room
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"roomAmount", 1}, "De-Room",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.55f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"focus", 1}, "Ambiance",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.45f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"decay", 1}, "Room Decay",
        juce::NormalisableRange<float>(0.05f, 0.95f, 0.01f), 0.50f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"smooth", 1}, "Smooth",
        juce::NormalisableRange<float>(0.0f, 0.95f, 0.01f), 0.50f));

    // Etapa 2: Tail De-Reverb
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"tailAmount", 1}, "De-Tail",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"sensitivity", 1}, "Sensitivity",
        juce::NormalisableRange<float>(0.1f, 2.0f, 0.01f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"release", 1}, "Release",
        juce::NormalisableRange<float>(20.0f, 400.0f, 1.0f), 120.0f));

    // Salida
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"output", 1}, "Output",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

void DeReverbenjaminProcessor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = sampleRate;
    setLatencySamples(fftSize);

    channels.resize(static_cast<size_t>(std::max(1, getTotalNumInputChannels())));
    for (auto& ch : channels)
        ch.reset();

    fastEnv = 0.0f;
    slowEnv = 0.0f;
    tailSmoothGain = 1.0f;
    currentGainReduction.store(1.0f);
}

bool DeReverbenjaminProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void DeReverbenjaminProcessor::processFrame(ChannelState& state, float amount, float roomDecay, float roomFocus, float smoothness)
{
    std::copy(state.inputBuffer.begin(), state.inputBuffer.end(), state.fftData.begin());
    std::fill(state.fftData.begin() + fftSize, state.fftData.end(), 0.0f);
    window.multiplyWithWindowingTable(state.fftData.data(), fftSize);

    fft.performRealOnlyForwardTransform(state.fftData.data(), true);

    const float overSub = 0.6f + roomFocus * 2.4f;
    const float minGain = 1.0f - (amount * 0.94f);
    const float timeSmooth = 0.25f + smoothness * 0.65f;

    for (int k = 0; k < numBins; ++k)
    {
        const float re = state.fftData[2 * k];
        const float im = state.fftData[2 * k + 1];
        const float power = re * re + im * im;

        state.reverbEst[k] = std::max(roomDecay * state.reverbEst[k],
                                      0.35f * state.reverbEst[k] + 0.65f * state.prevPower[k]);
        state.prevPower[k] = power;

        const float estRoom = state.reverbEst[k] * overSub;
        const float cleanPower = std::max(0.0f, power - estRoom);
        const float wiener = cleanPower / (power + 1e-9f);

        state.rawGain[k] = juce::jlimit(minGain, 1.0f, (1.0f - amount) + amount * wiener);
    }

    float gainSum = 0.0f;
    for (int k = 0; k < numBins; ++k)
    {
        float g = state.rawGain[k];
        if (k > 0 && k < numBins - 1)
            g = 0.25f * state.rawGain[k - 1] + 0.50f * state.rawGain[k] + 0.25f * state.rawGain[k + 1];

        state.smoothGain[k] = timeSmooth * state.smoothGain[k] + (1.0f - timeSmooth) * g;
        gainSum += state.smoothGain[k];

        state.fftData[2 * k]     *= state.smoothGain[k];
        state.fftData[2 * k + 1] *= state.smoothGain[k];
    }
    state.lastAvgGain = gainSum / static_cast<float>(numBins);

    fft.performRealOnlyInverseTransform(state.fftData.data());
    window.multiplyWithWindowingTable(state.fftData.data(), fftSize);

    constexpr float normFactor = 2.0f / 3.0f;
    for (int i = 0; i < fftSize; ++i)
        state.outputAccum[i] += state.fftData[i] * normFactor;

    std::copy(state.outputAccum.begin(), state.outputAccum.begin() + hopSize, state.outputReady.begin());
    std::copy(state.outputAccum.begin() + hopSize, state.outputAccum.end(), state.outputAccum.begin());
    std::fill(state.outputAccum.end() - hopSize, state.outputAccum.end(), 0.0f);
    std::copy(state.inputBuffer.begin() + hopSize, state.inputBuffer.end(), state.inputBuffer.begin());
}

void DeReverbenjaminProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const float roomAmount = apvts.getRawParameterValue("roomAmount")->load();
    const float focus = apvts.getRawParameterValue("focus")->load();
    const float decay = apvts.getRawParameterValue("decay")->load();
    const float smooth = apvts.getRawParameterValue("smooth")->load();

    const float tailAmount = apvts.getRawParameterValue("tailAmount")->load();
    const float sensitivity = apvts.getRawParameterValue("sensitivity")->load();
    const float releaseMs = apvts.getRawParameterValue("release")->load();
    const float outGain = juce::Decibels::decibelsToGain(apvts.getRawParameterValue("output")->load());

    const int numChannels = std::min(buffer.getNumChannels(), static_cast<int>(channels.size()));
    const int numSamples = buffer.getNumSamples();

    // --- ETAPA 1: Spectral De-Room ---
    float spectralGainAvg = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& state = channels[static_cast<size_t>(ch)];
        float* channelData = buffer.getWritePointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            state.inputBuffer[fftSize - hopSize + state.hopIndex] = channelData[i];
            channelData[i] = state.outputReady[state.hopIndex];
            state.hopIndex++;

            if (state.hopIndex == hopSize)
            {
                state.hopIndex = 0;
                processFrame(state, roomAmount, decay, focus, smooth);
            }
        }
        spectralGainAvg += state.lastAvgGain;
    }
    if (numChannels > 0)
        spectralGainAvg /= static_cast<float>(numChannels);

    // --- ETAPA 2: Dual-Envelope Tail De-Reverb + Output ---
    const float fastAttack = std::exp(-1.0f / (static_cast<float>(currentSampleRate) * 0.003f));
    const float fastRelease = std::exp(-1.0f / (static_cast<float>(currentSampleRate) * 0.040f));
    const float slowAttack = std::exp(-1.0f / (static_cast<float>(currentSampleRate) * 0.050f));
    const float slowRelease = std::exp(-1.0f / (static_cast<float>(currentSampleRate) * (releaseMs * 0.001f)));
    const float gainSmoothCoeff = std::exp(-1.0f / (static_cast<float>(currentSampleRate) * 0.015f));

    for (int i = 0; i < numSamples; ++i)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            peak = std::max(peak, std::abs(buffer.getSample(ch, i)));

        fastEnv = (peak > fastEnv)
            ? fastAttack * fastEnv + (1.0f - fastAttack) * peak
            : fastRelease * fastEnv + (1.0f - fastRelease) * peak;

        slowEnv = (peak > slowEnv)
            ? slowAttack * slowEnv + (1.0f - slowAttack) * peak
            : slowRelease * slowEnv + (1.0f - slowRelease) * peak;

        float ratio = fastEnv / (slowEnv + 1e-6f);
        float targetGain = 1.0f;

        if (ratio < sensitivity)
        {
            float reduction = juce::jlimit(0.0f, 1.0f, ratio / sensitivity);
            targetGain = (1.0f - tailAmount) + (tailAmount * reduction * reduction);
        }

        tailSmoothGain = gainSmoothCoeff * tailSmoothGain + (1.0f - gainSmoothCoeff) * targetGain;
        const float finalGain = tailSmoothGain * outGain;

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample(ch, i, buffer.getSample(ch, i) * finalGain);
    }

    currentGainReduction.store(juce::jlimit(0.0f, 1.0f, spectralGainAvg * tailSmoothGain));
}

juce::AudioProcessorEditor* DeReverbenjaminProcessor::createEditor()
{
    return new DeReverbenjaminEditor(*this);
}

void DeReverbenjaminProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void DeReverbenjaminProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DeReverbenjaminProcessor();
}
