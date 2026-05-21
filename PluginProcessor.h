#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>
#include <cmath>
#include <algorithm>

// All-Pass Statique : Étale les transitoires sans jamais changer la hauteur (Pitch)
class StaticDiffuser {
public:
    void prepare(double sr, float delayMs, float g) {
        int samples = static_cast<int>(sr * (delayMs / 1000.0f));
        buffer.assign(samples + 10, 0.0f);
        gain = g; writePos = 0;
    }
    float process(float in) {
        if (std::isnan(in) || std::isinf(in)) in = 0.0f;
        float delayed = buffer[writePos];
        float out = -gain * in + delayed;
        buffer[writePos] = in + gain * delayed;
        writePos++;
        if (writePos >= buffer.size()) writePos = 0;
        return out;
    }
private:
    std::vector<float> buffer;
    int writePos = 0;
    float gain = 0.5f;
};

// Délai Statique Simple
class StaticDelay {
public:
    void prepare(double sr, float delayMs) {
        int samples = static_cast<int>(sr * (delayMs / 1000.0f));
        buffer.assign(samples + 10, 0.0f);
        writePos = 0;
    }
    float process(float in) {
        if (std::isnan(in) || std::isinf(in)) in = 0.0f;
        float out = buffer[writePos];
        buffer[writePos] = in;
        writePos++;
        if (writePos >= buffer.size()) writePos = 0;
        return out;
    }
private:
    std::vector<float> buffer;
    int writePos = 0;
};

// Filtre Damping (Chaleur) très réactif
class ToneLPF {
public:
    void prepare(double sr) { sampleRate = sr; z1 = 0.0f; }
    void setCutoff(float freq) {
        float w0 = 6.28318530718f * freq / static_cast<float>(sampleRate);
        alpha = w0 / (1.0f + w0);
    }
    float process(float in) {
        z1 += alpha * (in - z1);
        if (std::isnan(z1) || std::isinf(z1)) z1 = 0.0f;
        return z1;
    }
private:
    float sampleRate = 44100.0f, alpha = 1.0f, z1 = 0.0f;
};

// Moteur d'Aura (Haute Densité, Zéro PVC, Zéro Pitch-Shift)
class HighDensityAuraEngine {
public:
    void prepare(double sr) {
        // 1. Les Diffuseurs d'Entrée : Transforment les "clics" percussifs en "pfff" doux
        // avant même qu'ils n'entrent dans la boucle. Fini le son de tuyau.
        inAp1L.prepare(sr, 4.77f, 0.7f); inAp2L.prepare(sr, 3.58f, 0.7f);
        inAp3L.prepare(sr, 12.73f, 0.7f); inAp4L.prepare(sr, 9.30f, 0.7f);
        inAp1R.prepare(sr, 4.85f, 0.7f); inAp2R.prepare(sr, 3.65f, 0.7f);
        inAp3R.prepare(sr, 12.91f, 0.7f); inAp4R.prepare(sr, 9.45f, 0.7f);

        // 2. Les Lignes de Réservoir (Tailles incommensurables pour casser les ondes stationnaires)
        del1.prepare(sr, 31.0f); del2.prepare(sr, 37.0f);
        del3.prepare(sr, 41.0f); del4.prepare(sr, 47.0f);

        // 3. Les Diffuseurs Internes : Le secret ! À chaque boucle, l'onde traverse
        // un nouveau diffuseur. La densité double à chaque rebond. 
        tankAp1.prepare(sr, 11.3f, 0.5f); tankAp2.prepare(sr, 13.1f, 0.5f);
        tankAp3.prepare(sr, 17.7f, 0.5f); tankAp4.prepare(sr, 19.3f, 0.5f);

        lpf1.prepare(sr); lpf2.prepare(sr); lpf3.prepare(sr); lpf4.prepare(sr);
        fb1 = fb2 = fb3 = fb4 = 0.0f;
    }

    void process(float inL, float inR, float& outL, float& outR, float sustain, float toneFreq) {
        // Le Damping est fermement ancré dans la boucle : il agit puissamment
        lpf1.setCutoff(toneFreq); lpf2.setCutoff(toneFreq);
        lpf3.setCutoff(toneFreq); lpf4.setCutoff(toneFreq);

        // Diffusion du signal entrant
        float diffL = inAp4L.process(inAp3L.process(inAp2L.process(inAp1L.process(inL))));
        float diffR = inAp4R.process(inAp3R.process(inAp2R.process(inAp1R.process(inR))));

        // Injection dans le réservoir
        float node1 = diffL + fb1 * sustain;
        float node2 = diffR + fb2 * sustain;
        float node3 = diffL - fb3 * sustain;
        float node4 = diffR - fb4 * sustain;

        // Traitement de chaque branche (Délai -> Diffuseur -> Égaliseur)
        float b1 = lpf1.process(tankAp1.process(del1.process(node1)));
        float b2 = lpf2.process(tankAp2.process(del2.process(node2)));
        float b3 = lpf3.process(tankAp3.process(del3.process(node3)));
        float b4 = lpf4.process(tankAp4.process(del4.process(node4)));

        // Matrice de mixage de Hadamard (0.5 garantit une énergie constante sans explosion)
        fb1 = std::clamp((b1 + b2 + b3 + b4) * 0.5f, -1.0f, 1.0f);
        fb2 = std::clamp((b1 - b2 + b3 - b4) * 0.5f, -1.0f, 1.0f);
        fb3 = std::clamp((b1 + b2 - b3 - b4) * 0.5f, -1.0f, 1.0f);
        fb4 = std::clamp((b1 - b2 - b3 + b4) * 0.5f, -1.0f, 1.0f);

        // Extraction pour les oreilles
        outL = b1 - b3;
        outR = b2 - b4;
    }

private:
    StaticDiffuser inAp1L, inAp2L, inAp3L, inAp4L;
    StaticDiffuser inAp1R, inAp2R, inAp3R, inAp4R;
    StaticDelay del1, del2, del3, del4;
    StaticDiffuser tankAp1, tankAp2, tankAp3, tankAp4;
    ToneLPF lpf1, lpf2, lpf3, lpf4;
    float fb1, fb2, fb3, fb4;
};

class StageAuraMaster : public juce::AudioProcessor {
public:
    StageAuraMaster();
    ~StageAuraMaster() override = default;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor(*this); }
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Stage Aura Master"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
private:
    juce::AudioProcessorValueTreeState apvts;
    HighDensityAuraEngine auraEngine;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StageAuraMaster)
};
