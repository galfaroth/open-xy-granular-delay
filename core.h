#pragma once
#include <opxy/effect.hpp>
namespace graindelay {
// History: 65536 interleaved stereo frames (left, right), a power of two so
// positions wrap for free in the top 16 bits of a 16.16 read position.
const opxy::u32 history_frames = 65536, buffer_words = history_frames * 2;
const opxy::u32 grain_count = 4;
// A grain's triangular window rises for `rise` samples and falls for `fall`;
// `window` is its current gain (already including the 1/4 mix share) and
// `step` its per-sample change. A grain with rise == fall == 0 is free.
struct Grain { opxy::u32 position, rate, rise, fall; opxy::i32 window, step; };
struct State {
    Grain grains[grain_count];
    opxy::u32 write, written, countdown, random;
    opxy::i32 filtered_left, filtered_right, feedback, level;
    // Per-block derived values, recomputed only when their controls move.
    opxy::i32 raw_time, raw_grain, raw_density, raw_spray;
    opxy::u32 delay, length, spacing, spray, half, step;
};
void initialize(State&);
void process(State&, opxy::Buffer, opxy::Channel, opxy::Channel, opxy::Parameters);
}
