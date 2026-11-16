// vocoder ventriloquist_ — audio engine
// Plain C++17, no framework dependency, so it can be tested on its own (see tools/render_test.cpp).
// Real-time safe: no allocation after prepare(), no locks.

#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace vq
{
constexpr double kTwoPi = 6.283185307179586;
constexpr int kMaxBands = 32;
constexpr int kNumModes = 6;
constexpr int kMaxVoices = 8;

//==============================================================================
struct Biquad
{
    float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
    {
        b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0);
        a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
    }
    void bandpass (double fs, double f, double q)   // RBJ, 0 dB peak
    {
        f = std::clamp (f, 10.0, fs * 0.45);
        const double w = kTwoPi * f / fs, al = std::sin (w) / (2.0 * q);
        set (al, 0.0, -al, 1.0 + al, -2.0 * std::cos (w), 1.0 - al);
    }
    void lowpass (double fs, double f, double q)
    {
        f = std::clamp (f, 10.0, fs * 0.45);
        const double w = kTwoPi * f / fs, al = std::sin (w) / (2.0 * q), c = std::cos (w);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highpass (double fs, double f, double q)
    {
        f = std::clamp (f, 10.0, fs * 0.45);
        const double w = kTwoPi * f / fs, al = std::sin (w) / (2.0 * q), c = std::cos (w);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() noexcept { z1 = z2 = 0; }
};

inline float onePoleCoef (double fs, double ms) { return (float) (1.0 - std::exp (-1.0 / (std::max (ms, 0.01) * 0.001 * fs))); }

inline float polyBlep (double t, double dt) noexcept
{
    if (t < dt)       { t /= dt; return (float) (t + t - t * t - 1.0); }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
    return 0.0f;
}
inline double wrap01 (double p) noexcept { return p - std::floor (p); }
inline float sawBL (double ph, double dt) noexcept { return (float) (2.0 * ph - 1.0) - polyBlep (ph, dt); }
inline float pulseBL (double ph, double dt, double width) noexcept { return sawBL (ph, dt) - sawBL (wrap01 (ph + width), dt); }

//==============================================================================
/** The six puppets. Each one is a full algorithm: band layout, ballistics, carrier recipe, colour. */
struct ModeConfig
{
    const char* name;
    int bands;
    float fmin, fmax, qScale;
    float attackMs, releaseMs;
    float formantOffsetSt;   // added to the user's size control
    float spread;            // stereo spread of odd/even bands
    float noiseInCarrier;    // breathiness
    float makeupDb;
};

inline const ModeConfig& modeConfig (int m)
{
    static const ModeConfig modes[kNumModes] = {
        { "ROBOT",    16, 100.0f,  8000.0f, 1.00f, 1.5f,  35.0f,  0.0f, 0.15f, 0.00f, 0.0f },
        { "MONSTER",  20,  60.0f,  7000.0f, 1.00f, 4.0f,  60.0f, -5.0f, 0.10f, 0.05f, -1.5f },
        { "GHOST",    24, 120.0f, 11000.0f, 1.40f, 9.0f, 220.0f,  2.0f, 0.80f, 0.55f, 0.0f },
        { "ANGEL",    28,  90.0f, 12000.0f, 1.10f, 6.0f, 140.0f,  3.0f, 0.60f, 0.05f, 0.0f },
        { "RADIO",    10, 300.0f,  3400.0f, 0.90f, 2.0f,  40.0f,  0.0f, 0.00f, 0.08f, 2.0f },
        { "CREATURE", 16,  80.0f,  9000.0f, 1.00f, 2.0f,  55.0f,  0.0f, 0.50f, 0.10f, -2.5f },
    };
    return modes[std::clamp (m, 0, kNumModes - 1)];
}

//==============================================================================
struct Params
{
    int   mode      = 0;      // 0..5 puppets
    float sizePct   = 50.0f;  // 0..100 puppet size; bigger puppet = deeper formants
    int   carrier   = 0;      // 0 synth, 1 noise, 2 sidechain
    int   pitchMode = 2;      // 0 free, 1 major, 2 minor (auto-pitch of the synth carrier)
    int   key       = 0;      // 0 = C
    float linger    = 0.35f;  // 0..1 filter-bank tail
    float whisper   = 0.50f;  // 0..1 consonants / unvoiced
    float chorus    = 0.20f;  // 0..1
    float gate      = 0.40f;  // 0..1 expander keyed on the voice
    float mix       = 1.00f;  // 0..1
    float outputDb  = 0.0f;
    bool  freeze    = false;
};

/** Formant shift in semitones produced by a given puppet size (0..100). */
inline float formantFromSize (float sizePct) { return (50.0f - sizePct) / 50.0f * 12.0f; }

//==============================================================================
class Engine
{
public:
    // ---- values the interface reads (written on the audio thread) ----
    std::array<std::atomic<float>, kMaxBands> bandLevel {};
    std::atomic<int>   numBandsActive { 16 };
    std::atomic<float> mouth { 0.0f };       // 0..1, how wide the puppet's mouth opens
    std::atomic<float> inLevel { 0.0f }, outLevel { 0.0f };
    std::atomic<int>   noteShown { -1 };     // midi note the puppet is singing, -1 = none
    std::atomic<float> unvoicedShown { 0.0f };

    void prepare (double sampleRate, int /*maxBlock*/)
    {
        fs = sampleRate;
        decim = std::max (1, (int) std::lround (fs / 12000.0));
        fsd = fs / decim;
        yinW = (int) (fsd * 0.026);
        tauMin = std::max (2, (int) (fsd / 1000.0));
        tauMax = std::min ((int) (fsd / 65.0), (int) yinRing.size() / 2 - yinW);
        yinHop = std::max (1, (int) (fsd * 0.010));
        decLP1.lowpass (fs, fsd * 0.4, 0.707); decLP2.lowpass (fs, fsd * 0.4, 0.707);

        for (auto& f : hfDetect) f.highpass (fs, 4000.0, 0.707);
        hfThrough.highpass (fs, 3500.0, 0.707);
        for (size_t c = 0; c < 2; ++c) { radioHP[c].highpass (fs, 300.0, 0.707); radioLP[c].lowpass (fs, 3400.0, 0.707); }

        const int chorusLen = (int) (fs * 0.05) + 4;
        for (auto& d : chorusBuf) d.assign ((size_t) chorusLen, 0.0f);

        atkFull = onePoleCoef (fs, 1.0); relFull = onePoleCoef (fs, 80.0);
        gateAtk = onePoleCoef (fs, 2.0); gateRel = onePoleCoef (fs, 60.0);
        uvAtk = onePoleCoef (fs, 5.0); uvRel = onePoleCoef (fs, 40.0);
        carAtk = onePoleCoef (fs, 3.0); carRel = onePoleCoef (fs, 25.0);
        glide = onePoleCoef (fs, 18.0);
        voiceAtk = onePoleCoef (fs, 5.0); voiceRel = onePoleCoef (fs, 150.0);
        smooth20 = onePoleCoef (fs, 20.0);
        fadeStep = (float) (1.0 / (0.015 * fs));

        activeMode = params.mode;
        buildBands();
        reset();
    }

    void reset()
    {
        for (auto& b : bands) { b.a1.reset(); b.a2.reset(); b.s1.reset(); b.s2.reset(); b.env = 0; b.carEnv = 0; }
        std::fill (yinRing.begin(), yinRing.end(), 0.0f);
        for (auto& d : chorusBuf) std::fill (d.begin(), d.end(), 0.0f);
        for (auto& v : voices) v = Voice{};
        envFull = 0; gateGain = 1; uv = 0; hfEnv = 0; allEnv = 0; preEmphZ = 0;
        decLP1.reset(); decLP2.reset(); hfThrough.reset();
    }

    void setParams (const Params& p)
    {
        params = p;
        if (p.mode != activeMode && ! modeSwitchPending) { pendingMode = p.mode; modeSwitchPending = true; }
    }

    void noteOn (int note, float velocity)
    {
        Voice* slot = nullptr;
        for (auto& v : voices) if (v.active && v.note == note) slot = &v;
        if (slot == nullptr) for (auto& v : voices) if (! v.active) { slot = &v; break; }
        if (slot == nullptr) slot = &voices[(size_t) (voiceSteal++ % kMaxVoices)];
        *slot = Voice{};
        slot->active = true; slot->note = note; slot->vel = velocity;
        slot->freq = 440.0 * std::pow (2.0, (note - 69) / 12.0);
    }
    void noteOff (int note) { for (auto& v : voices) if (v.active && v.note == note) v.releasing = true; }
    void allNotesOff()      { for (auto& v : voices) v.releasing = true; }

    /** Out may alias in. Sidechain pointers may be null. */
    void process (const float* inL, const float* inR, const float* scL, const float* scR,
                  float* outL, float* outR, int n)
    {
        const ModeConfig& mc = modeConfig (activeMode);
        const float formantTarget = formantFromSize (params.sizePct) + mc.formantOffsetSt;
        const float relMs = mc.releaseMs + params.linger * 2500.0f;
        const float envAtk = onePoleCoef (fs, mc.attackMs), envRel = onePoleCoef (fs, relMs);
        const float gateThrDb = -70.0f + params.gate * 50.0f;
        const float outGainTarget = std::pow (10.0f, params.outputDb / 20.0f);
        const float makeup = std::pow (10.0f, mc.makeupDb / 20.0f) * 1.6f;
        const int nb = numBands;

        float peakIn = 0, peakOut = 0, mouthAcc = 0;

        for (int i = 0; i < n; ++i)
        {
            const float l = inL[i], r = inR != nullptr ? inR[i] : l;
            const float mod = 0.5f * (l + r);
            peakIn = std::max (peakIn, std::max (std::abs (l), std::abs (r)));

            // --- pitch tracking (decimated YIN) ---
            const float dl = decLP2.process (decLP1.process (mod));
            if (++decCount >= decim)
            {
                decCount = 0;
                yinRing[(size_t) yinPos] = dl; yinPos = (yinPos + 1) % (int) yinRing.size();
                if (++hopCount >= yinHop) { hopCount = 0; runYin(); }
            }

            // --- broadband level, gate, unvoiced detection ---
            const float a = std::abs (mod);
            envFull += (a > envFull ? atkFull : relFull) * (a - envFull);
            const float lvlDb = 20.0f * std::log10 (envFull + 1e-9f);
            float gTarget = lvlDb >= gateThrDb ? 1.0f : std::pow (10.0f, std::max (-120.0f, (lvlDb - gateThrDb) * 3.0f) / 20.0f);
            if (params.freeze) gTarget = 1.0f;
            gateGain += (gTarget > gateGain ? gateAtk : gateRel) * (gTarget - gateGain);

            const float hf = std::abs (hfDetect[1].process (hfDetect[0].process (mod)));
            hfEnv += 0.002f * (hf - hfEnv); allEnv += 0.002f * (a - allEnv);
            const float ratio = hfEnv / (allEnv + 1e-6f);
            float uvTarget = std::clamp ((ratio - 0.22f) / 0.35f, 0.0f, 1.0f) * (1.0f - 0.8f * voicedConf);
            if (envFull < 1e-4f) uvTarget = 0.0f;
            uv += (uvTarget > uv ? uvAtk : uvRel) * (uvTarget - uv);

            // --- carrier ---
            float car = 0.0f;
            const float noise = nextNoise();
            if (params.carrier == 1)      car = noise;
            else if (params.carrier == 2) car = scL != nullptr ? 0.5f * (scL[i] + (scR != nullptr ? scR[i] : scL[i])) : 0.0f;
            else                          car = synthSample (mc) * (1.0f - mc.noiseInCarrier) + noise * mc.noiseInCarrier;
            car = car + (noise - car) * std::min (1.0f, uv * params.whisper * 0.9f);
            const float pre = car - 0.7f * preEmphZ; preEmphZ = car; car = pre;   // tilt up so the top bands speak

            // --- formant (smoothed, fractional band shift) ---
            formantSt += smooth20 * (formantTarget - formantSt);
            const float shiftBands = -formantSt * bandsPerOctave / 12.0f;

            // --- analysis: band envelopes ---
            if (! params.freeze)
                for (int b = 0; b < nb; ++b)
                {
                    auto& B = bands[(size_t) b];
                    const float y = std::abs (B.a2.process (B.a1.process (mod)));
                    B.env += (y > B.env ? envAtk : envRel) * (y - B.env);
                }

            // --- synthesis ---
            float wl = 0, wr = 0;
            for (int b = 0; b < nb; ++b)
            {
                auto& B = bands[(size_t) b];
                const float yc = B.s2.process (B.s1.process (car));
                const float ac = std::abs (yc);
                B.carEnv += (ac > B.carEnv ? carAtk : carRel) * (ac - B.carEnv);

                float pos = (float) (activeMode == 5 ? scramble[(size_t) b] : b) + shiftBands;
                pos = std::clamp (pos, 0.0f, (float) (nb - 1));
                const int p0 = (int) pos; const int p1 = std::min (p0 + 1, nb - 1);
                const float fr = pos - (float) p0;
                const float e = bands[(size_t) p0].env * (1.0f - fr) + bands[(size_t) p1].env * fr;

                const float s = yc * e / std::max (B.carEnv, 1e-4f);
                wl += s * B.gl; wr += s * B.gr;
            }
            wl *= makeup * gateGain; wr *= makeup * gateGain;

            // consonants straight through
            const float hfx = hfThrough.process (mod) * params.whisper * uv * 0.8f * gateGain;
            wl += hfx; wr += hfx;

            // --- puppet colour ---
            if (activeMode == 1) { wl = std::tanh (1.6f * wl) / 1.2f; wr = std::tanh (1.6f * wr) / 1.2f; }
            if (activeMode == 4)
            {
                wl = radioLP[0].process (radioHP[0].process (wl)); wr = radioLP[1].process (radioHP[1].process (wr));
                wl = std::tanh (2.2f * wl) / 1.6f; wr = std::tanh (2.2f * wr) / 1.6f;
            }

            // --- mode switch crossfade ---
            if (modeSwitchPending)
            {
                fade -= fadeStep;
                if (fade <= 0.0f) { fade = 0.0f; activeMode = pendingMode; modeSwitchPending = false; buildBands(); return processTail (inL, inR, scL, scR, outL, outR, n, i, peakIn, peakOut, mouthAcc); }
            }
            else if (fade < 1.0f) fade = std::min (1.0f, fade + fadeStep);
            wl *= fade; wr *= fade;

            // --- chorus ---
            chorusSample (wl, wr);

            mouthAcc = std::max (mouthAcc, std::abs (wl) + std::abs (wr));

            // --- mix and output ---
            mixS += smooth20 * (params.mix - mixS);
            outGain += smooth20 * (outGainTarget - outGain);
            float ol = (l * (1.0f - mixS) + wl * mixS) * outGain;
            float orr = (r * (1.0f - mixS) + wr * mixS) * outGain;
            ol = softClip (ol); orr = softClip (orr);
            outL[i] = ol; outR[i] = orr;
            peakOut = std::max (peakOut, std::max (std::abs (ol), std::abs (orr)));
        }
        publish (peakIn, peakOut, mouthAcc);
    }

    float currentFormantSt() const { return formantSt; }
    int   activeModeIndex() const { return activeMode; }

private:
    struct Band { Biquad a1, a2, s1, s2; float env = 0, carEnv = 0, gl = 0.707f, gr = 0.707f; };
    struct Voice { bool active = false, releasing = false; int note = -1; float vel = 1, env = 0; double freq = 220; double ph[3] { 0, 0.33, 0.66 }; };

    // continue a block after a mode switch happened mid-block (rare): rest of the samples with the new layout
    void processTail (const float* inL, const float* inR, const float* scL, const float* scR, float* outL, float* outR,
                      int n, int done, float peakIn, float peakOut, float mouthAcc)
    {
        // write the sample at index 'done' as silence-faded dry/wet, then process the remainder normally
        const float l = inL[done], r = inR != nullptr ? inR[done] : l;
        outL[done] = l * (1.0f - mixS) * outGain; outR[done] = r * (1.0f - mixS) * outGain;
        publish (peakIn, peakOut, mouthAcc);
        const int rest = n - done - 1;
        if (rest > 0)
            process (inL + done + 1, inR != nullptr ? inR + done + 1 : nullptr,
                     scL != nullptr ? scL + done + 1 : nullptr, scR != nullptr ? scR + done + 1 : nullptr,
                     outL + done + 1, outR + done + 1, rest);
    }

    void buildBands()
    {
        const ModeConfig& mc = modeConfig (activeMode);
        numBands = std::min (mc.bands, kMaxBands);
        const double ratio = std::pow ((double) mc.fmax / mc.fmin, 1.0 / (numBands - 1));
        const double bwOct = std::log2 (ratio);
        const double q = mc.qScale / (2.0 * std::sinh (std::log (2.0) / 2.0 * bwOct));
        bandsPerOctave = (float) (1.0 / bwOct);
        for (int b = 0; b < numBands; ++b)
        {
            const double fc = mc.fmin * std::pow (ratio, b);
            auto& B = bands[(size_t) b];
            B.a1.bandpass (fs, fc, q); B.a2.bandpass (fs, fc, q);
            B.s1.bandpass (fs, fc, q); B.s2.bandpass (fs, fc, q);
            B.a1.reset(); B.a2.reset(); B.s1.reset(); B.s2.reset(); B.env = 0; B.carEnv = 0;
            const float pan = mc.spread * ((b % 2) ? 0.8f : -0.8f);
            const double ang = (pan + 1.0) * 0.25 * 3.141592653589793;
            B.gl = (float) std::cos (ang); B.gr = (float) std::sin (ang);
            scramble[(size_t) b] = std::min (b ^ 1, numBands - 1);
        }
        numBandsActive.store (numBands);
    }

    float synthSample (const ModeConfig&)
    {
        bool anyMidi = false;
        for (auto& v : voices) if (v.active) { anyMidi = true; break; }

        float out = 0.0f;
        if (anyMidi)
        {
            int count = 0;
            for (auto& v : voices)
            {
                if (! v.active) continue;
                if (v.releasing) { v.env -= voiceRel * v.env; if (v.env < 1e-4f) { v.active = false; continue; } }
                else v.env += voiceAtk * (1.0f - v.env);
                out += oscillator (v.freq, v.ph) * v.env;
                ++count;
            }
            out *= 0.6f / std::sqrt ((float) std::max (count, 1));
            if (count > 0) { int top = -1; for (auto& v : voices) if (v.active && ! v.releasing) top = std::max (top, v.note); shownNote = top; }
        }
        else
        {
            autoFreq += glide * (autoTarget - autoFreq);
            out = oscillator (autoFreq, autoPh) * 0.6f;
        }
        return out;
    }

    float oscillator (double f, double* ph)
    {
        const double dt = f / fs;
        float s = 0.0f;
        switch (activeMode)
        {
            case 0: s = sawBL (ph[0], dt); break;
            case 1: s = 0.7f * sawBL (ph[0], dt) + 0.6f * pulseBL (ph[1], dt * 0.5, 0.5); break;
            case 2: s = sawBL (ph[0], dt); break;
            case 3: s = 0.45f * (sawBL (ph[0], dt * 1.004) + sawBL (ph[1], dt * 0.996) + 0.5f * sawBL (ph[2], dt * 2.0)); break;
            case 4: s = pulseBL (ph[0], dt, 0.25); break;
            default: s = sawBL (ph[0], dt) * (float) std::sin (kTwoPi * ph[1]) + 0.3f * sawBL (ph[0], dt); break;
        }
        ph[0] = wrap01 (ph[0] + (activeMode == 3 ? dt * 1.004 : dt));
        const double m1 = activeMode == 1 ? 0.5 : activeMode == 3 ? 0.996 : activeMode == 5 ? 0.731 : 1.0;
        ph[1] = wrap01 (ph[1] + dt * m1);
        ph[2] = wrap01 (ph[2] + dt * 2.0);
        return s;
    }

    void runYin()
    {
        const int len = yinW + tauMax;
        const int ringN = (int) yinRing.size();
        for (int j = 0; j < len; ++j) yinBuf[(size_t) j] = yinRing[(size_t) ((yinPos - len + j + ringN) % ringN)];

        double energy = 0;
        for (int j = 0; j < yinW; ++j) energy += (double) yinBuf[(size_t) j] * yinBuf[(size_t) j];
        const double rms = std::sqrt (energy / yinW);

        double running = 0; int best = -1; float bestVal = 1.0f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double d = 0;
            for (int j = 0; j < yinW; ++j) { const double x = yinBuf[(size_t) j] - yinBuf[(size_t) (j + tau)]; d += x * x; }
            running += d;
            yinD[(size_t) tau] = (float) (running > 0 ? d * tau / running : 1.0);
        }
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (yinD[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < tauMax && yinD[(size_t) (tau + 1)] < yinD[(size_t) tau]) ++tau;
                best = tau; bestVal = yinD[(size_t) tau]; break;
            }
        }
        if (best > 0 && rms > 0.003)
        {
            const float y0 = yinD[(size_t) (best - 1)], y1 = yinD[(size_t) best], y2 = yinD[(size_t) (best + 1)];
            const float den = y0 - 2 * y1 + y2;
            const float shift = std::abs (den) > 1e-9f ? 0.5f * (y0 - y2) / den : 0.0f;
            const double f0 = fsd / (best + std::clamp (shift, -1.0f, 1.0f));
            voicedConf = 1.0f - bestVal;
            const double midi = 69.0 + 12.0 * std::log2 (f0 / 440.0);
            const double q = quantize (midi);
            autoTarget = 440.0 * std::pow (2.0, (q - 69.0) / 12.0);
            bool anyMidi = false; for (auto& v : voices) if (v.active) anyMidi = true;
            if (! anyMidi) shownNote = (int) std::lround (q);
        }
        else
        {
            voicedConf *= 0.7f;
            bool anyMidi = false; for (auto& v : voices) if (v.active) anyMidi = true;
            if (! anyMidi && voicedConf < 0.2f) shownNote = -1;
        }
    }

    double quantize (double midi) const
    {
        if (params.pitchMode == 0) return midi;
        static const int major[7] = { 0, 2, 4, 5, 7, 9, 11 }, minor[7] = { 0, 2, 3, 5, 7, 8, 10 };
        const int* sc = params.pitchMode == 1 ? major : minor;
        const int base = (int) std::floor (midi) - 12;
        double best = midi, bestD = 1e9;
        for (int n = base; n <= base + 24; ++n)
        {
            const int pc = ((n - params.key) % 12 + 12) % 12;
            bool in = false; for (int k = 0; k < 7; ++k) if (sc[k] == pc) in = true;
            if (in && std::abs (n - midi) < bestD) { bestD = std::abs (n - midi); best = n; }
        }
        return best;
    }

    void chorusSample (float& l, float& r)
    {
        const float depth = params.chorus;
        if (depth <= 0.001f && chorusIdle) return;
        chorusIdle = depth <= 0.001f;
        const int len = (int) chorusBuf[0].size();
        chorusBuf[0][(size_t) chorusPos] = l; chorusBuf[1][(size_t) chorusPos] = r;
        lfoPh = wrap01 (lfoPh + 0.27 / fs);
        float outv[2];
        for (int c = 0; c < 2; ++c)
        {
            const double ms = 12.0 + 6.0 * depth * std::sin (kTwoPi * (lfoPh + 0.25 * c));
            const double rd = chorusPos - ms * 0.001 * fs;
            double rp = rd < 0 ? rd + len : rd;
            const int i0 = (int) rp; const int i1 = (i0 + 1) % len; const float fr = (float) (rp - i0);
            outv[c] = chorusBuf[(size_t) c][(size_t) i0] * (1.0f - fr) + chorusBuf[(size_t) c][(size_t) i1] * fr;
        }
        chorusPos = (chorusPos + 1) % len;
        l = l * (1.0f - 0.4f * depth) + outv[0] * 0.7f * depth;
        r = r * (1.0f - 0.4f * depth) + outv[1] * 0.7f * depth;
    }

    static float softClip (float x) noexcept
    {
        const float ax = std::abs (x);
        if (ax <= 0.9f) return x;
        const float y = 0.9f + 0.1f * std::tanh ((ax - 0.9f) / 0.1f);
        return x < 0 ? -y : y;
    }

    float nextNoise() noexcept
    {
        rng = rng * 1664525u + 1013904223u;
        return (float) ((rng >> 8) & 0xFFFFFF) / 8388608.0f - 1.0f;
    }

    void publish (float peakIn, float peakOut, float mouthAcc)
    {
        for (int b = 0; b < kMaxBands; ++b) bandLevel[(size_t) b].store (b < numBands ? bands[(size_t) b].env : 0.0f);
        const float m = std::clamp (mouthAcc * 2.0f, 0.0f, 1.0f);
        mouthShown += 0.5f * (m - mouthShown);
        mouth.store (mouthShown);
        inLevel.store (peakIn); outLevel.store (peakOut);
        noteShown.store (shownNote);
        unvoicedShown.store (uv);
    }

    // state
    double fs = 48000, fsd = 12000;
    Params params;
    int activeMode = 0, pendingMode = 0; bool modeSwitchPending = false; float fade = 1.0f, fadeStep = 0.001f;
    std::array<Band, kMaxBands> bands;
    std::array<int, kMaxBands> scramble {};
    int numBands = 16; float bandsPerOctave = 2.0f, formantSt = 0.0f;

    std::array<Voice, kMaxVoices> voices; unsigned voiceSteal = 0;
    double autoFreq = 110, autoTarget = 110, autoPh[3] { 0, 0.33, 0.66 };
    int shownNote = -1;

    Biquad decLP1, decLP2; int decim = 4, decCount = 0;
    std::array<float, 1024> yinRing {}; std::array<float, 1024> yinBuf {}; std::array<float, 1024> yinD {};
    int yinPos = 0, yinW = 300, tauMin = 12, tauMax = 180, yinHop = 120, hopCount = 0;
    float voicedConf = 0;

    std::array<Biquad, 2> hfDetect; Biquad hfThrough; std::array<Biquad, 2> radioHP, radioLP;
    float envFull = 0, gateGain = 1, uv = 0, hfEnv = 0, allEnv = 0, preEmphZ = 0;
    float atkFull = 0, relFull = 0, gateAtk = 0, gateRel = 0, uvAtk = 0, uvRel = 0, carAtk = 0, carRel = 0;
    float glide = 0, voiceAtk = 0, voiceRel = 0, smooth20 = 0;
    float mixS = 1, outGain = 1, mouthShown = 0;

    std::array<std::vector<float>, 2> chorusBuf; int chorusPos = 0; double lfoPh = 0; bool chorusIdle = true;
    uint32_t rng = 0x12345678u;
};

} // namespace vq
