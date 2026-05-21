#include "PluginProcessor.h"
#include <memory>
#include <cmath>

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    // Aura Amount : L'épaisseur de l'air sous la note
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("amount", 1), "Aura Amount", 
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.30f));
        
    // Sustain : La longueur organique du fondu (max 0.98 pour éviter les boucles infinies)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("sustain", 1), "Natural Sustain", 
        juce::NormalisableRange<float>(0.0f, 0.98f, 0.01f), 0.85f));
        
    // Tone : Fréquence de coupure LPF pure (De 500Hz très sombre à 12000Hz air libre)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tone", 1), "Aura Warmth (Hz)", 
        juce::NormalisableRange<float>(500.0f, 12000.0f, 10.0f, 3500.0f), 3500.0f));
        
    return layout;
}

StageAuraMaster::StageAuraMaster()
    : juce::AudioProcessor(juce::AudioProcessor::BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

void StageAuraMaster::prepareToPlay(double sampleRate, int samplesPerBlock) {
    auraEngine.prepare(sampleRate);
}

void StageAuraMaster::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;

    int numSamples = buffer.getNumSamples();
    auto* channelDataL = buffer.getWritePointer(0);
    auto* channelDataR = buffer.getWritePointer(1);

    float amt = apvts.getRawParameterValue("amount")->load();
    float sustain = apvts.getRawParameterValue("sustain")->load();
    float toneFreq = apvts.getRawParameterValue("tone")->load();

    for (int i = 0; i < numSamples; ++i) {
        float inL = channelDataL[i];
        float inR = channelDataR[i];
        float auraL = 0.0f, auraR = 0.0f;

        // Le moteur génère l'Aura sans jamais toucher au signal original
        auraEngine.process(inL, inR, auraL, auraR, sustain, toneFreq);

        // Mixage ultra pur : Timbre d'origine + Nuage (Zéro annulation, zéro délai de phase)
        channelDataL[i] = inL + (auraL * amt);
        channelDataR[i] = inR + (auraR * amt);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new StageAuraMaster();
}
