#include "PluginProcessor.h"

// To avoid clicks and pops when playing notes, we apply a simple envelope that
// fades the sound in and out. Comment out the define to turn off this envelope.
#define ENABLE_ENVELOPE

constexpr double PI     = 3.14159265358979323846264338327950288;
constexpr double TWO_PI = 6.28318530717958647692528676655900576;
constexpr double SQRT2  = 1.41421356237309504880168872420969808;

//==============================================================================

SynthAudioProcessor::SynthAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

SynthAudioProcessor::~SynthAudioProcessor()
{
}

const juce::String SynthAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SynthAudioProcessor::acceptsMidi() const
{
    return true;
}

bool SynthAudioProcessor::producesMidi() const
{
    return false;
}

bool SynthAudioProcessor::isMidiEffect() const
{
    return false;
}

double SynthAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SynthAudioProcessor::getNumPrograms()
{
    return 1;
}

int SynthAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SynthAudioProcessor::setCurrentProgram(int /*index*/)
{
}

const juce::String SynthAudioProcessor::getProgramName(int /*index*/)
{
    return {};
}

void SynthAudioProcessor::changeProgramName(int /*index*/, const juce::String& /*newName*/)
{
}

void SynthAudioProcessor::prepareToPlay(double sampleRate_, int /*samplesPerBlock*/)
{
    sampleRate = sampleRate_;
    activeNote = 0;
    env = 0.0;
    reset();
}

void SynthAudioProcessor::releaseResources()
{
}

bool SynthAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
    &&  layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) {
        return false;
    }
    return true;
}

void SynthAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that don't contain input data.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, buffer.getNumSamples());
    }

    updateParameters();
    splitBufferByEvents(buffer, midiMessages);
}

void SynthAudioProcessor::splitBufferByEvents(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    int bufferOffset = 0;

    for (const auto metadata : midiMessages) {
        int samplesThisSegment = metadata.samplePosition - bufferOffset;
        if (samplesThisSegment > 0) {
            render(buffer, samplesThisSegment, bufferOffset);
            bufferOffset += samplesThisSegment;
        }
        if (metadata.numBytes <= 3) {
            uint8_t data1 = (metadata.numBytes >= 2) ? metadata.data[1] : 0;
            uint8_t data2 = (metadata.numBytes == 3) ? metadata.data[2] : 0;
            handleMIDI(metadata.data[0], data1, data2);
        }
    }

    int samplesLastSegment = buffer.getNumSamples() - bufferOffset;
    if (samplesLastSegment > 0) {
        render(buffer, samplesLastSegment, bufferOffset);
    }

    midiMessages.clear();
}

void SynthAudioProcessor::handleMIDI(uint8_t data0, uint8_t data1, uint8_t data2)
{
    switch (data0 & 0xF0) {
        case 0x80:
            noteOff(data1);
            break;

        case 0x90: {
            uint8_t note = data1;
            uint8_t velo = data2;
            if (velo > 0) {
                noteOn(note, velo);
            } else {
                noteOff(note);
            }
            break;
        }
    }
}

static void protectYourEars(float* buffer, int sampleCount)
{
    if (buffer == nullptr) { return; }
    bool firstWarning = true;
    for (int i = 0; i < sampleCount; ++i) {
        float x = buffer[i];
        bool silence = false;
        if (std::isnan(x)) {
            DBG("!!! WARNING: nan detected in audio buffer, silencing !!!");
            silence = true;
        } else if (std::isinf(x)) {
            DBG("!!! WARNING: inf detected in audio buffer, silencing !!!");
            silence = true;
        } else if (x < -2.0f || x > 2.0f) {  // screaming feedback
            DBG("!!! WARNING: sample out of range, silencing !!!");
            silence = true;
        } else if (x < -1.0f) {
            if (firstWarning) {
                DBG("!!! WARNING: sample out of range, clamping !!!");
                firstWarning = false;
            }
            buffer[i] = -1.0f;
        } else if (x > 1.0f) {
            if (firstWarning) {
                DBG("!!! WARNING: sample out of range, clamping !!!");
                firstWarning = false;
            }
            buffer[i] = 1.0f;
        }
        if (silence) {
            memset(buffer, 0, sampleCount * sizeof(float));
            return;
        }
    }
}

void SynthAudioProcessor::render(juce::AudioBuffer<float>& buffer, int sampleCount, int bufferOffset)
{
    float* outputBufferLeft = buffer.getWritePointer(0) + bufferOffset;
    float* outputBufferRight = nullptr;
    if (getTotalNumOutputChannels() > 1) {
        outputBufferRight = buffer.getWritePointer(1) + bufferOffset;
    }

    for (int sample = 0; sample < sampleCount; ++sample) {
        double output = 0.0;

        if (activeNote > 0 || env > 0.0f) {
            output += processSample();

            #ifdef ENABLE_ENVELOPE
            env += envSlope;
            if (env >= 1.0f) {
                env = 1.0f;
            } else if (env <= 0.0f) {
                env = 0.0f;
            }
            output *= env;
            #endif
        }

        outputBufferLeft[sample] = static_cast<float>(output);
        if (outputBufferRight != nullptr) {
            outputBufferRight[sample] = static_cast<float>(output);
        }
    }

    protectYourEars(outputBufferLeft, sampleCount);
    protectYourEars(outputBufferRight, sampleCount);
}

void SynthAudioProcessor::noteOn(int note, int velocity)
{
    activeNote = note;
    amplitude = (velocity / 127.0) * 0.5;
    frequency = 440.0 * std::exp2(double(activeNote - 69) / 12.0);

    const float attackTime = 0.01;
    envSlope = 1.0 / (sampleRate * attackTime);

    startSound();
}

void SynthAudioProcessor::noteOff(int note)
{
    if (activeNote == note) {
        activeNote = 0;

        const float releaseTime = 0.01;
        envSlope = -1.0 / (sampleRate * releaseTime);
    }
}

bool SynthAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* SynthAudioProcessor::createEditor()
{
    auto editor = new juce::GenericAudioProcessorEditor(*this);
    editor->setSize(500, 500);
    return editor;
}

void SynthAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    copyXmlToBinary(*apvts.copyState().createXml(), destData);
}

void SynthAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SynthAudioProcessor();
}

juce::AudioProcessorValueTreeState::ParameterLayout SynthAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // You can create JUCE plug-in parameters here and read their values
    // in updateParameters(). Any parameters will automatically get added
    // to the plug-in's user interface.

    return layout;
}

//==============================================================================
// Implement the following methods to perform the sound synthesis algorithm
//==============================================================================

void SynthAudioProcessor::reset()
{
    phase = 0.0;
    inc = 0.0;
}

void SynthAudioProcessor::updateParameters()
{
    // Perform any necessary calculations here if the synthesis algorithm has
    // additional parameters that can change while the sound is playing.
}

void SynthAudioProcessor::startSound()
{
    inc = frequency * TWO_PI / sampleRate;
}

double SynthAudioProcessor::processSample()
{
    double output = amplitude * std::sin(phase);

    phase += inc;
    if (phase > TWO_PI) {
        phase -= TWO_PI;
    }

    return output;
}
