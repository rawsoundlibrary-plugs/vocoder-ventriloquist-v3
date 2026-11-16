// Offline test for the vocoder ventriloquist_ engine.
// Builds a synthetic "voice" (glottal pulses + moving formants + 's' consonants), runs it through
// every puppet and carrier, checks levels / NaN / pitch tracking, and writes WAV files.
//
//   g++ -O2 -std=c++17 -I../Source Tests/render_test.cpp -o render_test && ./render_test out_dir

#include "dsp/VentriloquistEngine.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>

static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int fs)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) { std::printf ("cannot write %s\n", path.c_str()); return; }
    const uint32_t n = (uint32_t) L.size(), dataBytes = n * 4, riff = 36 + dataBytes;
    const uint16_t fmt = 1, ch = 2, bits = 16, align = 4; const uint32_t rate = (uint32_t) fs, bps = rate * 4, fmtLen = 16;
    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riff, 4, 1, f); std::fwrite ("WAVEfmt ", 1, 8, f);
    std::fwrite (&fmtLen, 4, 1, f); std::fwrite (&fmt, 2, 1, f); std::fwrite (&ch, 2, 1, f); std::fwrite (&rate, 4, 1, f);
    std::fwrite (&bps, 4, 1, f); std::fwrite (&align, 2, 1, f); std::fwrite (&bits, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&dataBytes, 4, 1, f);
    for (uint32_t i = 0; i < n; ++i)
        for (int c = 0; c < 2; ++c)
        {
            const float v = std::clamp ((c ? R[i] : L[i]), -1.0f, 1.0f);
            const int16_t s = (int16_t) std::lround (v * 32767.0f);
            std::fwrite (&s, 2, 1, f);
        }
    std::fclose (f);
}

struct Syll { int midi; int vowel; bool s; float dur; };

static std::vector<float> makeVoice (int fs, std::vector<int>& truthMidi)
{
    // vowel formants (F1, F2, F3) for a, e, i, o, u
    const float F[5][3] = { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 570, 840, 2410 }, { 300, 870, 2240 } };
    const Syll ph[] = { { 57, 1, false, 0.28f }, { 60, 3, false, 0.32f }, { 64, 0, false, 0.30f }, { 62, 2, true, 0.36f },
                        { -1, 0, false, 0.25f }, { 55, 4, true, 0.30f }, { 59, 0, false, 0.28f }, { 62, 1, false, 0.40f },
                        { 64, 3, false, 0.34f }, { -1, 0, false, 0.35f }, { 57, 2, true, 0.30f }, { 60, 0, false, 0.50f } };
    std::vector<float> out;
    vq::Biquad f1, f2, f3, sHP, glot;
    glot.lowpass (fs, 1800, 0.6);
    sHP.highpass (fs, 4500, 0.9);
    double phs = 0; uint32_t rng = 99; float vowelF[3] = { 500, 1500, 2500 };
    auto noise = [&rng]() { rng = rng * 1664525u + 1013904223u; return (float) ((rng >> 8) & 0xFFFFFF) / 8388608.0f - 1.0f; };
    for (int rep = 0; rep < 2; ++rep)
        for (const auto& s : ph)
        {
            const int n = (int) (s.dur * fs);
            for (int i = 0; i < n; ++i)
            {
                const float t = (float) i / n;
                float smp = 0;
                if (s.midi > 0)
                {
                    for (int k = 0; k < 3; ++k) vowelF[k] += 0.002f * (F[s.vowel][k] - vowelF[k]);
                    if (i % 64 == 0) { f1.bandpass (fs, vowelF[0], 6); f2.bandpass (fs, vowelF[1], 8); f3.bandpass (fs, vowelF[2], 10); }
                    const double f0 = 440.0 * std::pow (2.0, (s.midi - 69) / 12.0) * (1.0 + 0.006 * std::sin (6.28 * 5.5 * i / fs));
                    const double dt = f0 / fs;
                    const float src = glot.process (vq::sawBL (phs, dt)); phs = vq::wrap01 (phs + dt);
                    const float env = std::min (1.0f, t / 0.08f) * std::min (1.0f, (1.0f - t) / 0.12f);
                    smp = (1.0f * f1.process (src) + 0.6f * f2.process (src) + 0.3f * f3.process (src)) * env * 0.9f;
                    if (s.s && t < 0.22f) smp = smp * (t / 0.22f) + sHP.process (noise()) * 0.25f * (1.0f - t / 0.22f);
                    truthMidi.push_back (s.midi);
                }
                else truthMidi.push_back (-1);
                out.push_back (smp + noise() * 0.0005f);
            }
        }
    return out;
}

static float rms (const std::vector<float>& v, size_t a = 0, size_t b = 0)
{
    if (b == 0) b = v.size();
    double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i];
    return (float) std::sqrt (s / std::max<size_t> (1, b - a));
}

int main (int argc, char** argv)
{
    const std::string dir = argc > 1 ? argv[1] : ".";
    const int fs = 48000, block = 256;
    std::vector<int> truth;
    const auto voice = makeVoice (fs, truth);
    writeWav (dir + "/00_input_voice.wav", voice, voice, fs);
    std::printf ("input: %.2fs  rms %.4f\n", voice.size() / (double) fs, rms (voice));

    struct Case { std::string name; vq::Params p; bool midi; bool sidechain; bool freezeHalf; };
    std::vector<Case> cases;
    for (int m = 0; m < vq::kNumModes; ++m) { vq::Params p; p.mode = m; cases.push_back ({ std::string ("mode_") + vq::modeConfig (m).name, p, false, false, false }); }
    { vq::Params p; p.mode = 0; cases.push_back ({ "robot_midi_chord", p, true, false, false }); }
    { vq::Params p; p.mode = 0; p.carrier = 1; cases.push_back ({ "robot_noise_carrier", p, false, false, false }); }
    { vq::Params p; p.mode = 3; p.carrier = 2; cases.push_back ({ "angel_sidechain", p, false, true, false }); }
    { vq::Params p; p.mode = 0; p.sizePct = 0; cases.push_back ({ "robot_small_puppet", p, false, false, false }); }
    { vq::Params p; p.mode = 0; p.sizePct = 100; cases.push_back ({ "robot_big_puppet", p, false, false, false }); }
    { vq::Params p; p.mode = 2; p.freeze = false; cases.push_back ({ "ghost_freeze", p, false, false, true }); }
    { vq::Params p; p.mode = 0; p.pitchMode = 0; cases.push_back ({ "robot_free_pitch", p, false, false, false }); }

    bool allOk = true;
    for (auto& c : cases)
    {
        vq::Engine e; e.prepare (fs, block); e.setParams (c.p); e.prepare (fs, block);
        if (c.midi) { e.noteOn (48, 1); e.noteOn (55, 1); e.noteOn (63, 1); }
        std::vector<float> L (voice.size()), R (voice.size()), sc (voice.size());
        if (c.sidechain) { double p1 = 0, p2 = 0; for (size_t i = 0; i < sc.size(); ++i) { sc[i] = 0.3f * (vq::sawBL (p1, 130.8 / fs) + vq::sawBL (p2, 196.0 / fs)); p1 = vq::wrap01 (p1 + 130.8 / fs); p2 = vq::wrap01 (p2 + 196.0 / fs); } }
        int correct = 0, voicedFrames = 0, octaveErr = 0;
        const auto t0 = std::chrono::high_resolution_clock::now();
        for (size_t pos = 0; pos < voice.size(); pos += block)
        {
            const int n = (int) std::min<size_t> (block, voice.size() - pos);
            if (c.freezeHalf) { auto p = c.p; p.freeze = pos > voice.size() * 0.12 && pos < voice.size() * 0.6; e.setParams (p); }
            e.process (voice.data() + pos, voice.data() + pos, c.sidechain ? sc.data() + pos : nullptr, c.sidechain ? sc.data() + pos : nullptr, L.data() + pos, R.data() + pos, n);
            if (c.p.pitchMode == 0 && ! c.midi && truth[pos] > 0 && pos % (block * 8) == 0 && pos > 0 && truth[pos - block * 6] == truth[pos])
            {
                ++voicedFrames; const int shown = e.noteShown.load();
                if (shown == truth[pos]) ++correct; else if (shown > 0 && std::abs (shown - truth[pos]) == 12) ++octaveErr;
            }
        }
        const double secs = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
        bool nan = false; float peak = 0; for (size_t i = 0; i < L.size(); ++i) { if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) nan = true; peak = std::max (peak, std::max (std::abs (L[i]), std::abs (R[i]))); }
        const float ratioDb = 20.0f * std::log10 ((rms (L) + rms (R)) * 0.5f / rms (voice) + 1e-9f);
        const double rt = secs / (voice.size() / (double) fs);
        bool ok = ! nan && peak > 0.01f && ratioDb > -12 && ratioDb < 9;
        std::printf ("%-22s out/in %+5.1f dB  peak %.2f  cpu %5.2f%% of realtime  %s", c.name.c_str(), ratioDb, peak, rt * 100, ok ? "OK" : "CHECK");
        if (voicedFrames > 0) std::printf ("  pitch %d/%d exact, %d octave", correct, voicedFrames, octaveErr);
        if (c.freezeHalf)
        {
            // the voice is silent in the 5th syllable (index 4); with freeze the output should keep going
            const size_t a = (size_t) (voice.size() * 0.362), b = (size_t) (voice.size() * 0.392);
            std::printf ("  silent-gap rms frozen %.4f (input %.5f)", rms (L, a, b), rms (voice, a, b));
        }
        std::printf ("\n");
        allOk = allOk && ok;
        writeWav (dir + "/" + c.name + ".wav", L, R, fs);
    }

    // gate: low noise only, output should stay quiet
    {
        vq::Engine e; e.prepare (fs, block);
        std::vector<float> in (fs * 2), L (in.size()), R (in.size()); uint32_t rng = 5;
        for (auto& v : in) { rng = rng * 1664525u + 1013904223u; v = ((float) ((rng >> 8) & 0xFFFFFF) / 8388608.0f - 1.0f) * 0.0006f; }
        for (size_t pos = 0; pos < in.size(); pos += block) { const int n = (int) std::min<size_t> (block, in.size() - pos); e.process (in.data() + pos, in.data() + pos, nullptr, nullptr, L.data() + pos, R.data() + pos, n); }
        std::printf ("gate on -64 dBFS noise: out rms %.6f (%.1f dBFS)\n", rms (L, fs, in.size()), 20 * std::log10 (rms (L, fs, in.size()) + 1e-12));
    }
    std::printf (allOk ? "ALL OK\n" : "SOME CHECKS FAILED\n");
    return allOk ? 0 : 1;
}
