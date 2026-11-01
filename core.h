#pragma once
#include <opxy/effect.hpp>
namespace graindelay {
const opxy::u32 history_frames = 65536, buffer_words = history_frames * 2;
struct Grain { opxy::u32 position, rate, age, length, window_step; };
struct State {
    Grain grains[4];
    opxy::u32 write, written, countdown, random;
    opxy::i32 filtered_left, filtered_right, feedback, level;
};
void initialize(State&);
void process(State&, opxy::Buffer, opxy::Channel, opxy::Channel, opxy::Parameters);
}
