/*

  Compile and run this on macOS:
  $ clang -std=c++11 -lstdc++ -Wall -Wextra main.cpp -o synth
  $ ./synth

  Compile and run this on Windows:
  TODO

  Compile and run this on Linux:
  TODO

 */

#include <cstdio>
#include <cmath>
#include <new>

constexpr double PI     = 3.14159265358979323846264338327950288;
constexpr double TWO_PI = 6.28318530717958647692528676655900576;
constexpr double SQRT2  = 1.41421356237309504880168872420969808;

//==============================================================================
// Change these settings to use a different sampling rate or pitch
//==============================================================================

double sampleRate = 48000;
double frequency  = 261.63;
double amplitude  = 0.5;  // -6 dB

double lengthInSeconds = 10;

//==============================================================================
// Declare the variables for the synthesis algorithm here
//==============================================================================

double phase;
double inc;

//==============================================================================
// Implement the following methods to perform the sound synthesis algorithm
//==============================================================================

void reset()
{
    phase = 0.0;
    inc = 0.0;
}

void startSound()
{
    inc = frequency * TWO_PI / sampleRate;
}

double processSample()
{
    double output = amplitude * std::sin(phase);

    phase += inc;
    if (phase > TWO_PI) {
        phase -= TWO_PI;
    }

    return output;
}

//==============================================================================
// Run the synthesis algorithm and write the output to a WAV file
//==============================================================================

int main(int argc, char* argv[])
{
    reset();
    startSound();

    int sampleCount = static_cast<int>(sampleRate * lengthInSeconds);
    int16_t* outputBuffer = new int16_t[sampleCount];

    for (int sample = 0; sample < sampleCount; ++sample) {
        double output = processSample();
        outputBuffer[sample] = static_cast<int16_t>(output * 32767.0);
    }

    FILE* f = fopen("output.wav", "wb");
    if (!f) {
        printf("Error: could not open WAV file for writing.\n");
        return -1;
    }

    uint32_t dataLength = static_cast<uint32_t>(sampleCount * 2);
    uint32_t length = 44 + dataLength;
    uint32_t blockSize = 16;
    uint16_t format = 1;    // PCM
    uint16_t channels = 1;  // mono
    uint32_t srate = static_cast<uint32_t>(sampleRate);
    uint32_t bytesPerSecond = srate * 2;
    uint16_t bytesPerSample = 2;
    uint16_t bitsPerSample = 16;

    fwrite("RIFF", 4, 1, f);
    fwrite(&length, 4, 1, f);
    fwrite("WAVE", 4, 1, f);
    fwrite("fmt ", 4, 1, f);
    fwrite(&blockSize, 4, 1, f);
    fwrite(&format, 2, 1, f);
    fwrite(&channels, 2, 1, f);
    fwrite(&srate, 4, 1, f);
    fwrite(&bytesPerSecond, 4, 1, f);
    fwrite(&bytesPerSample, 2, 1, f);
    fwrite(&bitsPerSample, 2, 1, f);
    fwrite("data", 4, 1, f);
    fwrite(&dataLength, 4, 1, f);
    fwrite(outputBuffer, sampleCount, 2, f);
    fclose(f);

    delete[] outputBuffer;
    return 0;
}
