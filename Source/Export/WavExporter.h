#pragma once
#include <juce_audio_formats/juce_audio_formats.h>

namespace anticip8::WavExporter
{
    /** Ecrit le buffer en WAV PCM 24-bit a la frequence donnee. */
    inline juce::Result writeWav24 (const juce::AudioBuffer<float>& buffer, double sampleRate, const juce::File& file)
    {
        if (buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0)
            return juce::Result::fail ("Rien a exporter.");
        if (file.existsAsFile() && ! file.deleteFile())
            return juce::Result::fail ("Impossible d'ecraser " + file.getFileName());
        if (! file.getParentDirectory().createDirectory() && ! file.getParentDirectory().isDirectory())
            return juce::Result::fail ("Impossible de creer le dossier d'export.");

        juce::WavAudioFormat format;
        std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
        if (stream == nullptr)
            return juce::Result::fail ("Impossible d'ecrire " + file.getFullPathName());

        std::unique_ptr<juce::AudioFormatWriter> writer (
            format.createWriterFor (stream.release(), sampleRate,
                                    (unsigned int) buffer.getNumChannels(), 24, {}, 0));
        if (writer == nullptr)
            return juce::Result::fail ("Impossible de creer l'ecrivain WAV.");

        if (! writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples()))
            return juce::Result::fail ("Echec d'ecriture WAV (disque plein ?).");
        return juce::Result::ok();
    }
}
