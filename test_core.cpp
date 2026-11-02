// Test autonome du cœur DSP (sans JUCE).
// Compilation : g++ -std=c++17 -O2 -Wall -Wextra Tests/test_core.cpp -o test_core && ./test_core

#include "../Source/StrainCore.h"

#include <cmath>
#include <cstdio>
#include <vector>

static constexpr double kFs = 96000.0; // 2 x 48 kHz (le plugin tourne suréchantillonné)
static constexpr double kPi = 3.14159265358979323846;

static float toDb (double lin) { return (float) (20.0 * std::log10 (std::max (lin, 1.0e-9))); }

static std::vector<float> sine (double freq, double amp, double seconds, double startPhase = 0.0)
{
    std::vector<float> v ((size_t) (seconds * kFs));
    for (size_t i = 0; i < v.size(); ++i)
        v[i] = (float) (amp * std::sin (2.0 * kPi * freq * (double) i / kFs + startPhase));
    return v;
}

static double peakOfTail (const std::vector<float>& v, double seconds)
{
    double p = 0.0;
    for (size_t i = v.size() - (size_t) (seconds * kFs); i < v.size(); ++i)
        p = std::max (p, (double) std::abs (v[i]));
    return p;
}

static double harmonicDb (const std::vector<float>& v, size_t from, size_t count, double freq, int k)
{
    double re = 0.0, im = 0.0;
    for (size_t i = 0; i < count; ++i)
    {
        const double ph = 2.0 * kPi * freq * k * (double) (from + i) / kFs;
        re += v[from + i] * std::cos (ph);
        im += v[from + i] * std::sin (ph);
    }
    return 20.0 * std::log10 (2.0 * std::sqrt (re * re + im * im) / (double) count + 1.0e-12);
}

int main()
{
    const char* ratioNames[strain::Core::kNumRatios] = { "1:1", "2:1", "3:1", "4:1", "6:1", "10:1", "20:1", "NUKE" };
    bool ok = true;

    // ---- 1) courbe statique : niveau de sortie selon le niveau d'entrée ------
    std::printf ("== 1) Courbe statique (sinus 1 kHz, attack 4, release 5, seuil %.0f dB) ==\n", strain::Core::kThresholdDb);
    std::printf ("%-6s | in -30  in -20  in -10  in  -3   (sortie crête, dBFS)\n", "ratio");

    for (int r = 0; r < strain::Core::kNumRatios; ++r)
    {
        std::printf ("%-6s |", ratioNames[r]);
        for (double inDb : { -30.0, -20.0, -10.0, -3.0 })
        {
            strain::Core core;
            strain::Params p;
            p.ratioIndex = r;
            core.setParams (p);
            core.prepare (kFs);

            auto x = sine (1000.0, std::pow (10.0, inDb / 20.0), 2.5);
            float* ch[1] = { x.data() };
            core.process (ch, 1, (int) x.size());

            const float out = toDb (peakOfTail (x, 0.25));
            std::printf (" %7.2f", out);

            if (std::isnan (out)) ok = false;
        }
        std::printf ("\n");
    }

    // ---- 2) temps d'attaque : réduction de gain après un saut de niveau -----
    std::printf ("\n== 2) Attaque (saut -50 -> -6 dBFS, ratio 10:1) : temps pour atteindre 63%% de la GR finale ==\n");
    for (float attack : { 1.0f, 4.0f, 7.0f, 10.0f })
    {
        strain::Core core;
        strain::Params p;
        p.ratioIndex = 5;
        p.attack = attack;
        p.release = 5.0f;
        core.setParams (p);
        core.prepare (kFs);

        auto quiet = sine (1000.0, std::pow (10.0, -50.0 / 20.0), 0.3);
        auto loud  = sine (1000.0, std::pow (10.0, -6.0 / 20.0), 2.0);

        float* q[1] = { quiet.data() };
        core.process (q, 1, (int) quiet.size());

        std::vector<float> trace;
        const int block = 8;
        for (size_t i = 0; i + block <= loud.size(); i += block)
        {
            float* l[1] = { loud.data() + i };
            core.process (l, 1, block);
            trace.push_back (core.getGainReductionDb());
        }

        const float finalGr = trace.back();
        double tms = -1.0;
        for (size_t i = 0; i < trace.size(); ++i)
            if (trace[i] >= 0.63f * finalGr) { tms = 1000.0 * (double) (i * block) / kFs; break; }

        std::printf ("attack %4.1f -> %8.2f ms  (GR finale %.1f dB)\n", attack, tms, finalGr);
    }

    // ---- 3) harmoniques des modes de distorsion (ratio 1:1, -6 dBFS) ----------
    std::printf ("\n== 3) Distorsion (1 kHz, -6 dBFS, ratio 1:1) : H2 et H3 en dB par rapport au fondamental ==\n");
    struct Mode { const char* name; bool d2; bool d3; };
    for (Mode m : { Mode { "off", false, false }, Mode { "dist2", true, false },
                    Mode { "dist3", false, true }, Mode { "dist2+3", true, true } })
    {
        strain::Core core;
        strain::Params p;
        p.ratioIndex = 0;
        p.dist2 = m.d2;
        p.dist3 = m.d3;
        core.setParams (p);
        core.prepare (kFs);

        auto x = sine (1000.0, std::pow (10.0, -6.0 / 20.0), 1.0);
        float* ch[1] = { x.data() };
        core.process (ch, 1, (int) x.size());

        const size_t from = (size_t) (0.5 * kFs), count = 96 * 100; // 100 périodes exactes
        const double h1 = harmonicDb (x, from, count, 1000.0, 1);
        std::printf ("%-8s H2 %7.1f dB   H3 %7.1f dB   (fondamental %.1f dBFS)\n", m.name,
                     harmonicDb (x, from, count, 1000.0, 2) - h1,
                     harmonicDb (x, from, count, 1000.0, 3) - h1, h1);
    }

    // ---- 4) stéréo lié / mono -------------------------------------------------
    {
        strain::Core core;
        strain::Params p; p.ratioIndex = 3; p.link = true;
        core.setParams (p);
        core.prepare (kFs);

        auto l = sine (1000.0, 0.8, 1.0);
        auto r = sine (1000.0, 0.05, 1.0);
        float* ch[2] = { l.data(), r.data() };
        core.process (ch, 2, (int) l.size());

        const float gl = toDb (peakOfTail (l, 0.2)), gr = toDb (peakOfTail (r, 0.2));
        std::printf ("\n== 4) Link stéréo : L entre -1.9 dBFS, R entre -26 dBFS -> sorties L %.2f / R %.2f dBFS\n", gl, gr);
        std::printf ("   (avec link, R doit être réduit du même nombre de dB que L : écart attendu ~ %.1f dB)\n", 24.1);
        std::printf ("   écart mesuré : %.2f dB\n", gl - gr);
    }

    std::printf ("\n%s\n", ok ? "OK : aucune valeur invalide." : "ERREUR : NaN détecté.");
    return ok ? 0 : 1;
}
