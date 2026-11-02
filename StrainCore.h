#pragma once

#include <algorithm>
#include <cmath>

// ============================================================================
//  StrainCore : cœur DSP d'un compresseur "à la Distressor".
//
//  - Aucune dépendance à JUCE : on peut le compiler et le tester tout seul.
//  - Ce n'est PAS une émulation de circuit (pas de schéma public) mais un
//    modèle comportemental. Toutes les constantes marquées CALIBRATION sont
//    des estimations à ajuster à l'oreille ou avec des mesures.
//  - Prévu pour tourner suréchantillonné (le processeur JUCE fait du 2x).
// ============================================================================

namespace strain
{
struct Params
{
    float inputDb    = 0.0f;  // gain avant détecteur : joue le rôle du seuil
    float outputDb   = 0.0f;
    float attack     = 4.0f;  // 1..10
    float release    = 5.0f;  // 1..10
    int   ratioIndex = 3;     // 0..7 -> 1:1, 2:1, 3:1, 4:1, 6:1, 10:1, 20:1, NUKE
    bool  dist2      = false; // harmoniques paires
    bool  dist3      = false; // harmoniques impaires
    bool  detHP      = false; // filtre passe-haut sur le détecteur
    bool  opto       = false; // comportement de type opto
    bool  link       = true;  // détection stéréo liée
};

class Core
{
public:
    static constexpr int   kMaxChannels = 2;
    static constexpr int   kNumRatios   = 8;
    static constexpr float kRatios[kNumRatios] = { 1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 10.0f, 20.0f, 1000.0f };

    // ---- CALIBRATION --------------------------------------------------------
    static constexpr float kThresholdDb  = -20.0f;   // seuil fixe (l'Input règle le niveau d'attaque)
    static constexpr float kDetectorHpHz = 100.0f;   // fréquence du filtre "Det HP"
    static constexpr float kAttackMinMs  = 0.05f;    // attack = 1
    static constexpr float kAttackMaxMs  = 150.0f;   // attack = 10
    static constexpr float kReleaseMinMs = 50.0f;    // release = 1
    static constexpr float kReleaseMaxMs = 3500.0f;  // release = 10
    static constexpr float kEvenDrive    = 1.2f;     // force de "Dist 2"
    static constexpr float kEvenBias     = 0.25f;
    static constexpr float kOddDrive     = 1.5f;     // force de "Dist 3"
    // ------------------------------------------------------------------------

    void prepare (double sampleRate)
    {
        fs         = (float) sampleRate;
        gainSmooth = 1.0f - std::exp (-1.0f / (0.02f * fs));
        dcCoef     = 1.0f - 6.2831853f * 10.0f / fs;

        designDetectorHp();

        evenBias0 = std::tanh (kEvenDrive * kEvenBias);
        evenNorm  = 1.0f / (kEvenDrive * (1.0f - evenBias0 * evenBias0));

        setParams (params);
        reset();
    }

    void reset()
    {
        for (int c = 0; c < kMaxChannels; ++c)
        {
            gr[c]  = 0.0f;
            hpZ1[c] = hpZ2[c] = 0.0f;
            dcX1[c] = dcY1[c] = 0.0f;
        }
        inGain  = inGainTarget;
        outGain = outGainTarget;
        meterGr = 0.0f;
    }

    void setParams (const Params& p)
    {
        params = p;

        inGainTarget  = dbToLin (p.inputDb);
        outGainTarget = dbToLin (p.outputDb);

        ratioIdx = std::clamp (p.ratioIndex, 0, kNumRatios - 1);
        ratio    = kRatios[ratioIdx];
        slope    = 1.0f - 1.0f / ratio;

        if (p.opto)             kneeDb = 10.0f;
        else if (ratio <= 4.0f) kneeDb = 6.0f;
        else if (ratio <= 10.0f) kneeDb = 3.0f;
        else                    kneeDb = 0.5f;

        const float a = (std::clamp (p.attack,  1.0f, 10.0f) - 1.0f) / 9.0f;
        const float r = (std::clamp (p.release, 1.0f, 10.0f) - 1.0f) / 9.0f;

        attackTau  = 0.001f * kAttackMinMs  * std::pow (kAttackMaxMs  / kAttackMinMs,  a);
        releaseTau = 0.001f * kReleaseMinMs * std::pow (kReleaseMaxMs / kReleaseMinMs, r);

        if (p.opto)
            attackTau = std::max (attackTau, 0.008f); // une cellule opto n'est jamais instantanée

        attackCoef = std::exp (-1.0f / (attackTau * fs));
    }

    // Traite le bloc en place. channels[c] = pointeur vers les échantillons du canal c.
    void process (float* const* channels, int numChannels, int numSamples)
    {
        numChannels = std::min (numChannels, kMaxChannels);
        float blockMeter = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            inGain  += gainSmooth * (inGainTarget  - inGain);
            outGain += gainSmooth * (outGainTarget - outGain);

            float x[kMaxChannels]      = {};
            float target[kMaxChannels] = {};
            float maxTarget = 0.0f;

            // 1) détection
            for (int c = 0; c < numChannels; ++c)
            {
                x[c] = channels[c][i] * inGain;

                const float filtered = runDetectorHp (c, x[c]);
                const float det      = params.detHP ? filtered : x[c];
                const float levelDb  = 20.0f * std::log10 (std::max (std::abs (det), 1.0e-6f));

                target[c] = gainComputer (levelDb);
                maxTarget = std::max (maxTarget, target[c]);
            }

            // 2) balistique + application du gain + distorsion
            for (int c = 0; c < numChannels; ++c)
            {
                const float t = params.link ? maxTarget : target[c];
                updateBallistics (c, t);

                float y = x[c] * dbToLin (-gr[c]);

                if (params.dist2)
                {
                    y = shapeEven (y);
                    y = blockDc (c, y);
                }
                else
                {
                    dcX1[c] = dcY1[c] = 0.0f;
                }

                if (params.dist3)
                    y = shapeOdd (y);

                channels[c][i] = y * outGain;
                blockMeter = std::max (blockMeter, gr[c]);
            }
        }

        meterGr = blockMeter;
    }

    // Réduction de gain maximale (en dB, positive) du dernier bloc traité.
    float getGainReductionDb() const { return meterGr; }

private:
    static float dbToLin (float db) { return std::pow (10.0f, db * 0.05f); }

    // Courbe statique avec coude doux (formulation classique, domaine log).
    float gainComputer (float levelDb) const
    {
        const float over = levelDb - kThresholdDb;

        if (2.0f * over < -kneeDb)
            return 0.0f;

        if (2.0f * std::abs (over) <= kneeDb)
        {
            const float t = over + kneeDb * 0.5f;
            return slope * t * t / (2.0f * kneeDb);
        }

        return slope * over;
    }

    // Attack/release dans le domaine dB, release dépendant du programme.
    void updateBallistics (int c, float target)
    {
        if (target > gr[c])
        {
            gr[c] = target + attackCoef * (gr[c] - target);
        }
        else
        {
            const float k = std::clamp (gr[c] / 12.0f, 0.0f, 1.0f);

            // Normal : plus la réduction est forte, plus le relâchement est rapide.
            // Opto   : relâchement qui ralentit quand la réduction est faible (longue queue).
            const float tau = params.opto ? releaseTau * (1.0f + 2.0f * (1.0f - k))
                                          : releaseTau * (1.0f - 0.5f * k);

            const float coef = std::exp (-1.0f / (tau * fs));
            gr[c] = target + coef * (gr[c] - target);
        }
    }

    // Dist 2 : saturation asymétrique -> harmoniques paires (le DC est retiré ensuite).
    float shapeEven (float x) const
    {
        return (std::tanh (kEvenDrive * (x + kEvenBias)) - evenBias0) * evenNorm;
    }

    // Dist 3 : saturation symétrique -> harmoniques impaires.
    static float shapeOdd (float x)
    {
        return std::tanh (kOddDrive * x) / kOddDrive;
    }

    float blockDc (int c, float x)
    {
        const float y = x - dcX1[c] + dcCoef * dcY1[c];
        dcX1[c] = x;
        dcY1[c] = y;
        return y;
    }

    // Passe-haut 2e ordre (Butterworth), forme transposée directe II.
    void designDetectorHp()
    {
        const float w0    = 6.2831853f * kDetectorHpHz / fs;
        const float cosw  = std::cos (w0);
        const float alpha = std::sin (w0) / (2.0f * 0.70710678f);
        const float a0    = 1.0f + alpha;

        hb0 = (1.0f + cosw) * 0.5f / a0;
        hb1 = -(1.0f + cosw) / a0;
        hb2 = hb0;
        ha1 = -2.0f * cosw / a0;
        ha2 = (1.0f - alpha) / a0;
    }

    float runDetectorHp (int c, float x)
    {
        const float y = hb0 * x + hpZ1[c];
        hpZ1[c] = hb1 * x - ha1 * y + hpZ2[c];
        hpZ2[c] = hb2 * x - ha2 * y;
        return y;
    }

    Params params;

    float fs = 48000.0f;
    float gainSmooth = 0.001f, dcCoef = 0.999f;

    float inGain = 1.0f, outGain = 1.0f, inGainTarget = 1.0f, outGainTarget = 1.0f;

    int   ratioIdx = 3;
    float ratio = 4.0f, slope = 0.75f, kneeDb = 6.0f;
    float attackTau = 0.001f, releaseTau = 0.3f, attackCoef = 0.0f;

    float gr[kMaxChannels] = {};
    float meterGr = 0.0f;

    float hb0 = 1.0f, hb1 = 0.0f, hb2 = 0.0f, ha1 = 0.0f, ha2 = 0.0f;
    float hpZ1[kMaxChannels] = {}, hpZ2[kMaxChannels] = {};

    float dcX1[kMaxChannels] = {}, dcY1[kMaxChannels] = {};
    float evenBias0 = 0.0f, evenNorm = 1.0f;
};
} // namespace strain
