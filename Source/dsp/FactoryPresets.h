#pragma once

// Factory presets: ten per Mode, ordered from everyday to experimental, then
// "Real World" presets that recreate everyday speakers and spaces.
// Plain data with no JUCE dependency so the engine tests can render every one.

#include "ReverbEngine.h"

#include <array>

namespace halcyon
{

struct FactoryPreset
{
    const char* name;
    ReverbParams params;
};

namespace detail
{
constexpr auto P = Mode::Plate;
constexpr auto H = Mode::Hall;
constexpr auto R = Mode::Room;
constexpr auto C = Mode::Chamber;
constexpr auto K = Mode::Cathedral;
constexpr auto Mod = Color::Modern;
constexpr auto Vin = Color::Vintage;
constexpr auto Drk = Color::Dark;
constexpr auto Off = Speaker::Off;
} // namespace detail

inline constexpr int presetsPerMode = 10;

// clang-format off
inline const std::array<FactoryPreset, 5 * presetsPerMode> factoryPresets {{
    //                     mix   pre    decay  size  diff  rate  depth  highcut  lowcut  width  mode       color        lofi
    { "Clean Plate",       { 0.30f,  10.0f,  1.8f, 0.50f, 0.80f, 0.60f, 0.30f,  9000.0f, 120.0f, 1.0f, detail::P, detail::Mod, false } },
    { "Vocal Plate",       { 0.25f,  40.0f,  1.6f, 0.45f, 0.85f, 0.50f, 0.25f,  7500.0f, 200.0f, 1.0f, detail::P, detail::Mod, false } },
    { "Snare Plate",       { 0.30f,   0.0f,  1.2f, 0.35f, 0.90f, 0.70f, 0.20f, 10000.0f, 180.0f, 1.0f, detail::P, detail::Mod, false } },
    { "Vintage Plate",     { 0.30f,  20.0f,  2.4f, 0.55f, 0.80f, 0.45f, 0.45f,  7000.0f, 150.0f, 0.9f, detail::P, detail::Vin, false } },
    { "Silver Plate",      { 0.35f,  15.0f,  3.5f, 0.60f, 0.85f, 1.10f, 0.50f, 16000.0f, 250.0f, 1.0f, detail::P, detail::Mod, false } },
    { "Velvet Plate",      { 0.35f,  25.0f,  3.0f, 0.60f, 0.80f, 0.40f, 0.40f,  4500.0f, 100.0f, 1.0f, detail::P, detail::Drk, false } },
    { "Plate Wash",        { 0.50f,  30.0f,  8.0f, 0.80f, 0.90f, 0.35f, 0.55f,  8000.0f, 150.0f, 1.0f, detail::P, detail::Vin, false } },
    { "Tin Can",           { 0.40f,   0.0f,  2.0f, 0.05f, 0.30f, 2.50f, 0.20f, 12000.0f, 600.0f, 0.6f, detail::P, detail::Mod, false } },
    { "Seasick Plate",     { 0.45f,  20.0f,  4.0f, 0.60f, 0.80f, 4.50f, 1.00f,  9000.0f, 150.0f, 1.0f, detail::P, detail::Vin, false } },
    { "Broken Toy",        { 0.55f,   0.0f,  3.0f, 0.30f, 0.70f, 3.00f, 0.70f,  5000.0f, 300.0f, 1.0f, detail::P, detail::Vin, true  } },

    { "Concert Hall",      { 0.30f,  20.0f,  2.5f, 0.60f, 0.75f, 0.60f, 0.40f,  8000.0f,  80.0f, 1.0f, detail::H, detail::Mod, false } },
    { "Small Hall",        { 0.25f,  12.0f,  1.6f, 0.40f, 0.70f, 0.60f, 0.35f,  8500.0f, 100.0f, 1.0f, detail::H, detail::Mod, false } },
    { "Vocal Hall",        { 0.25f,  60.0f,  2.2f, 0.55f, 0.80f, 0.50f, 0.35f,  7000.0f, 220.0f, 1.0f, detail::H, detail::Mod, false } },
    { "Vintage Hall",      { 0.30f,  25.0f,  3.0f, 0.60f, 0.75f, 0.50f, 0.55f,  6500.0f, 120.0f, 1.0f, detail::H, detail::Vin, false } },
    { "String Hall",       { 0.40f,  30.0f,  4.0f, 0.75f, 0.80f, 0.45f, 0.50f,  7500.0f,  90.0f, 1.0f, detail::H, detail::Mod, false } },
    { "Dark Hall",         { 0.35f,  30.0f,  3.5f, 0.70f, 0.75f, 0.40f, 0.45f,  4000.0f,  80.0f, 1.0f, detail::H, detail::Drk, false } },
    { "Endless Hall",      { 0.55f,  40.0f, 25.0f, 0.90f, 0.85f, 0.30f, 0.60f,  6000.0f, 120.0f, 1.0f, detail::H, detail::Vin, false } },
    { "Slapback Hall",     { 0.40f, 350.0f,  1.8f, 0.50f, 0.50f, 0.60f, 0.30f,  7000.0f, 200.0f, 1.0f, detail::H, detail::Mod, false } },
    { "Warped Tape Hall",  { 0.50f,  30.0f,  5.0f, 0.70f, 0.80f, 0.20f, 1.00f,  5500.0f, 150.0f, 1.0f, detail::H, detail::Vin, false } },
    { "Ghost Radio",       { 0.60f,  80.0f,  6.0f, 0.80f, 0.80f, 1.50f, 0.70f,  3000.0f, 500.0f, 0.7f, detail::H, detail::Drk, true  } },

    { "Studio Room",       { 0.20f,   0.0f,  0.6f, 0.35f, 0.70f, 0.60f, 0.20f, 10000.0f, 100.0f, 1.0f, detail::R, detail::Mod, false } },
    { "Drum Room",         { 0.30f,   0.0f,  0.9f, 0.50f, 0.75f, 0.50f, 0.20f,  9000.0f,  80.0f, 1.0f, detail::R, detail::Mod, false } },
    { "Vocal Booth",       { 0.15f,   0.0f,  0.35f, 0.15f, 0.80f, 0.50f, 0.10f, 8000.0f, 200.0f, 0.7f, detail::R, detail::Mod, false } },
    { "Wood Room",         { 0.25f,   5.0f,  1.1f, 0.45f, 0.75f, 0.50f, 0.25f,  6500.0f, 120.0f, 1.0f, detail::R, detail::Vin, false } },
    { "Live Room",         { 0.30f,   8.0f,  1.5f, 0.65f, 0.70f, 0.60f, 0.30f,  8000.0f,  90.0f, 1.0f, detail::R, detail::Mod, false } },
    { "Basement",          { 0.30f,   4.0f,  1.3f, 0.55f, 0.75f, 0.40f, 0.25f,  3500.0f,  60.0f, 0.9f, detail::R, detail::Drk, false } },
    { "Garage",            { 0.35f,   3.0f,  2.0f, 0.45f, 0.45f, 0.80f, 0.25f, 11000.0f, 150.0f, 1.0f, detail::R, detail::Vin, false } },
    { "Bathroom Tiles",    { 0.40f,   0.0f,  1.8f, 0.10f, 0.35f, 1.00f, 0.15f, 14000.0f, 300.0f, 0.8f, detail::R, detail::Mod, false } },
    { "Shoebox Resonator", { 0.50f,   0.0f,  6.0f, 0.00f, 0.15f, 0.10f, 0.00f, 12000.0f, 400.0f, 0.4f, detail::R, detail::Mod, false } },
    { "Cassette Closet",   { 0.45f,   0.0f,  1.4f, 0.30f, 0.70f, 2.20f, 0.60f,  4000.0f, 350.0f, 0.8f, detail::R, detail::Vin, true  } },

    { "Echo Chamber",      { 0.30f,  15.0f,  1.8f, 0.50f, 0.75f, 0.50f, 0.35f,  8000.0f, 100.0f, 1.0f, detail::C, detail::Mod, false } },
    { "Vintage Chamber",   { 0.30f,  20.0f,  2.0f, 0.55f, 0.75f, 0.45f, 0.50f,  6500.0f, 140.0f, 1.0f, detail::C, detail::Vin, false } },
    { "Vocal Chamber",     { 0.25f,  45.0f,  1.7f, 0.50f, 0.80f, 0.50f, 0.30f,  7500.0f, 220.0f, 1.0f, detail::C, detail::Mod, false } },
    { "Stone Chamber",     { 0.30f,  10.0f,  2.6f, 0.60f, 0.70f, 0.40f, 0.30f,  9500.0f,  90.0f, 1.0f, detail::C, detail::Mod, false } },
    { "Warm Chamber",      { 0.35f,  20.0f,  2.4f, 0.60f, 0.80f, 0.40f, 0.40f,  4500.0f, 100.0f, 1.0f, detail::C, detail::Drk, false } },
    { "Piano Chamber",     { 0.30f,  25.0f,  2.2f, 0.65f, 0.85f, 0.35f, 0.30f,  7000.0f,  70.0f, 1.0f, detail::C, detail::Vin, false } },
    { "Big Chamber",       { 0.40f,  30.0f,  4.5f, 0.85f, 0.80f, 0.45f, 0.45f,  7000.0f, 100.0f, 1.0f, detail::C, detail::Mod, false } },
    { "Mono Bunker",       { 0.40f, 120.0f,  3.0f, 0.50f, 0.60f, 0.50f, 0.30f,  5000.0f, 400.0f, 0.0f, detail::C, detail::Drk, false } },
    { "Chorus Chamber",    { 0.45f,  20.0f,  3.0f, 0.55f, 0.80f, 3.50f, 0.90f,  9000.0f, 150.0f, 1.0f, detail::C, detail::Vin, false } },
    { "Arcade Chamber",    { 0.50f,  60.0f,  3.5f, 0.50f, 0.75f, 0.80f, 0.40f,  6000.0f, 250.0f, 1.0f, detail::C, detail::Mod, true  } },

    { "Cathedral",         { 0.35f,  30.0f,  6.0f, 0.75f, 0.80f, 0.40f, 0.45f,  6500.0f,  80.0f, 1.0f, detail::K, detail::Mod, false } },
    { "Small Church",      { 0.30f,  20.0f,  3.5f, 0.50f, 0.75f, 0.45f, 0.40f,  7000.0f, 100.0f, 1.0f, detail::K, detail::Mod, false } },
    { "Stone Nave",        { 0.40f,  40.0f,  8.0f, 0.85f, 0.80f, 0.35f, 0.50f,  5500.0f,  70.0f, 1.0f, detail::K, detail::Vin, false } },
    { "Choir Loft",        { 0.35f,  70.0f,  5.0f, 0.70f, 0.85f, 0.40f, 0.40f,  7500.0f, 180.0f, 1.0f, detail::K, detail::Mod, false } },
    { "Crypt",             { 0.40f,  25.0f,  7.0f, 0.80f, 0.80f, 0.30f, 0.45f,  3000.0f,  60.0f, 1.0f, detail::K, detail::Drk, false } },
    { "Ambient Pad",       { 0.60f,  50.0f, 12.0f, 0.90f, 0.90f, 0.25f, 0.60f,  6000.0f, 150.0f, 1.0f, detail::K, detail::Vin, false } },
    { "Frozen Cathedral",  { 0.70f,   0.0f, 30.0f, 1.00f, 0.90f, 0.15f, 0.50f,  5000.0f, 120.0f, 1.0f, detail::K, detail::Drk, false } },
    { "Distant Echo",      { 0.50f, 500.0f,  9.0f, 0.90f, 0.40f, 0.30f, 0.40f,  4500.0f, 300.0f, 1.0f, detail::K, detail::Vin, false } },
    { "Underwater",        { 0.65f,  40.0f, 10.0f, 0.85f, 0.85f, 4.00f, 1.00f,  1500.0f, 100.0f, 1.0f, detail::K, detail::Drk, false } },
    { "Dying Satellite",   { 0.75f, 150.0f, 20.0f, 1.00f, 0.80f, 2.00f, 0.80f,  3500.0f, 600.0f, 0.8f, detail::K, detail::Vin, true  } },
}};

inline const std::array<FactoryPreset, 6> realWorldPresets {{
    //                     mix   pre    decay  size  diff  rate  depth  highcut  lowcut  width  mode       color        lofi   speaker
    // Music out of a phone's tiny speaker, held up in a small room.
    { "Cell Phone",        { 0.10f,   0.0f,  0.35f, 0.10f, 0.60f, 0.50f, 0.10f,  9000.0f, 150.0f, 0.6f, detail::R, detail::Mod, false, Speaker::CellPhone } },
    // Standing in a glass phone booth: a tiny, hard, ringing box with no bass.
    { "Phone Booth",       { 0.45f,   0.0f,  0.45f, 0.00f, 0.35f, 0.40f, 0.10f, 12000.0f, 220.0f, 0.4f, detail::R, detail::Mod, false, detail::Off } },
    // A tube TV's little mono speaker across a living room, whine included.
    { "90's CRT TV",       { 0.18f,   5.0f,  0.60f, 0.25f, 0.70f, 0.50f, 0.15f,  6000.0f, 120.0f, 0.8f, detail::R, detail::Vin, false, Speaker::CrtTv } },
    // A cassette boom box with the bass boost on: warm, hissy, a little wobbly.
    { "80's Boom Box",     { 0.15f,  10.0f,  0.90f, 0.40f, 0.70f, 0.50f, 0.30f,  7000.0f, 100.0f, 1.0f, detail::R, detail::Vin, false, Speaker::BoomBox } },
    // The other end of a call: narrow band, phone-line codec, line hiss.
    { "Land Line",         { 0.06f,   0.0f,  0.30f, 0.00f, 0.50f, 0.50f, 0.00f,  6000.0f, 200.0f, 0.5f, detail::R, detail::Mod, false, Speaker::Landline } },
    // A cabinet's cranked, boxy speakers in a busy arcade.
    { "90's Arcade",       { 0.25f,  15.0f,  1.20f, 0.55f, 0.75f, 0.60f, 0.25f,  6500.0f, 120.0f, 0.9f, detail::C, detail::Vin, false, Speaker::Arcade } },
}};
// clang-format on

inline constexpr int numFactoryPresets = 5 * presetsPerMode + 6;

// Factory presets in menu order: the per-Mode groups, then Real World.
inline const FactoryPreset* getFactoryPreset (int index)
{
    if (index >= 0 && index < (int) factoryPresets.size())
        return &factoryPresets[(size_t) index];
    index -= (int) factoryPresets.size();
    if (index >= 0 && index < (int) realWorldPresets.size())
        return &realWorldPresets[(size_t) index];
    return nullptr;
}

} // namespace halcyon
