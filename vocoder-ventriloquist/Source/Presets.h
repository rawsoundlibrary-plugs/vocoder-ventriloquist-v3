#pragma once

namespace vq
{
struct FactoryPreset
{
    const char* name;
    int mode; float size; int carrier; int pitch;
    float linger, whisper, chorus, gate, mix;
};

// mode: 0 robot, 1 monster, 2 ghost, 3 angel, 4 radio, 5 creature
// carrier: 0 synth, 1 noise, 2 sidechain — pitch: 0 free, 1 major, 2 minor
inline const FactoryPreset factoryPresets[] = {
    { "Tin Robot",         0, 50.0f, 0, 2, 20.0f, 50.0f, 10.0f, 40.0f, 100.0f },
    { "Basement Monster",  1, 85.0f, 0, 2, 30.0f, 40.0f, 15.0f, 45.0f, 100.0f },
    { "Haunted Hallway",   2, 40.0f, 0, 2, 80.0f, 60.0f, 45.0f, 35.0f, 100.0f },
    { "Choir of One",      3, 45.0f, 0, 1, 55.0f, 50.0f, 60.0f, 40.0f, 100.0f },
    { "Pirate Radio",      4, 50.0f, 0, 0, 15.0f, 30.0f,  0.0f, 50.0f, 100.0f },
    { "Swamp Thing",       5, 70.0f, 0, 2, 35.0f, 45.0f, 25.0f, 40.0f, 100.0f },
    { "Whisper Box",       0, 30.0f, 1, 0, 30.0f, 80.0f, 20.0f, 40.0f, 100.0f },
    { "Sidechain Puppet",  0, 50.0f, 2, 0, 25.0f, 50.0f, 10.0f, 40.0f, 100.0f },
    { "Tiny Helper",       0,  8.0f, 0, 1, 20.0f, 55.0f, 20.0f, 40.0f, 100.0f },
    { "Giant in the Wall", 2, 98.0f, 0, 2, 90.0f, 40.0f, 35.0f, 30.0f, 100.0f },
};
inline constexpr int numFactoryPresets = (int) (sizeof (factoryPresets) / sizeof (factoryPresets[0]));
} // namespace vq
