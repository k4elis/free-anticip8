#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>

namespace anticip8::ReverseProcessor
{
    /** Reverses every channel of the buffer in place. */
    inline void reverseInPlace (juce::AudioBuffer<float>& b)
    {
        for (int c = 0; c < b.getNumChannels(); ++c)
        {
            auto* d = b.getWritePointer (c);
            std::reverse (d, d + b.getNumSamples());
        }
    }

    /** Writes src reversed into dest[0 .. src.numSamples). dest must be at least that long. */
    inline void copyReversed (const juce::AudioBuffer<float>& src, juce::AudioBuffer<float>& dest, int numChannels)
    {
        const int n = src.getNumSamples();
        for (int c = 0; c < numChannels; ++c)
        {
            const auto* s = src.getReadPointer (c);
            auto* d = dest.getWritePointer (c);
            for (int i = 0; i < n; ++i)
                d[i] = s[n - 1 - i];
        }
    }
}
