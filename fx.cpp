#include "core.h"
#include <opxy/q31.hpp>
namespace graindelay {
namespace {
using namespace opxy;
i32 control(i32 x) { return x < 0 ? 0 : x; }
u32 scaled(i32 x, u32 range) { return (static_cast<u32>(control(x)) >> 16) * range / 32767; }
i32 glide(i32 x, i32 target) { const i32 d = target - x; return x + (d / 64 ? d / 64 : (d > 0) - (d < 0)); }
u32 random(State& s) { s.random ^= s.random << 13; s.random ^= s.random >> 17; s.random ^= s.random << 5; return s.random; }
i32 sample(const i32* line, const State& s, u32 index) {
    const u32 age = (s.write - index) & (history_frames - 1);
    return age && age <= s.written ? line[index] : 0;
}
i32 tap(const i32* line, const State& s, u32 position) {
    const u32 index = position >> 16, fraction = position & 65535;
    const i32 a = sample(line, s, index), b = sample(line, s, (index + 1) & (history_frames - 1));
    // Divide before multiplying: bounded products, no signed wrap at full scale.
    return a / 65536 * static_cast<i32>(65536 - fraction) + b / 65536 * static_cast<i32>(fraction);
}
}
void initialize(State& s) { s = State(); s.random = 0x51f15e1u; }
void process(State& s, opxy::Buffer buffer, opxy::Channel left, opxy::Channel right, opxy::Parameters p) {
    using namespace opxy;
    if (!buffer.words || buffer.count < buffer_words) {
        initialize(s);
        for (u32 i = 0; i < block_frames; ++i) left[i] = right[i] = 0;
        return;
    }
    i32* l = buffer.words; i32* r = l + history_frames;
    const u32 length = (220 + scaled(p[1], 3748)) & ~3u;
    const u32 density = 1 + ((static_cast<u32>(control(p[2])) >> 16) * 4 >> 15);
    const u32 rates[5] = {32768, 43740, 65536, 98193, 131072};
    const u32 rate = rates[(static_cast<u32>(control(p[4])) >> 16) * 5 >> 15];
    const u32 delay = 882 + scaled(p[0], 43218), spray = scaled(p[5], 4410);
    const i32 feedback = q31::multiply(control(p[3]), 1975684955);
    const i32 tone = q31::maximum - q31::multiply(control(p[6]), 2040109465);
    for (u32 i = 0; i < block_frames; ++i) {
        if (!s.countdown) {
            for (u32 g = 0; g < 4; ++g) if (!s.grains[g].length) {
                const u32 extra = spray ? (random(s) >> 16) * spray / 65535 : 0;
                const u32 catchup = rate > 65536 ? (rate - 65536) * length / 65536 : 0;
                s.grains[g] = Grain{(s.write - delay - extra - catchup - 2) << 16, rate, 0, length, 2147483647u / (length / 2)};
                break;
            }
            s.countdown = length / density;
        }
        --s.countdown;
        i32 wet_l = 0, wet_r = 0;
        for (u32 g = 0; g < 4; ++g) {
            Grain& grain = s.grains[g];
            if (!grain.length) continue;
            const u32 phase = grain.age < grain.length / 2 ? grain.age : grain.length - grain.age;
            const i32 window = static_cast<i32>(phase * grain.window_step);
            wet_l += q31::multiply(tap(l, s, grain.position), window) / 4;
            wet_r += q31::multiply(tap(r, s, grain.position), window) / 4;
            grain.position += grain.rate;
            if (++grain.age == grain.length) grain.length = 0;
        }
        s.feedback = glide(s.feedback, feedback); s.level = glide(s.level, control(p[7]));
        s.filtered_left = q31::add(q31::multiply(wet_l, tone), q31::multiply(s.filtered_left, q31::maximum - tone));
        s.filtered_right = q31::add(q31::multiply(wet_r, tone), q31::multiply(s.filtered_right, q31::maximum - tone));
        l[s.write] = q31::add(left[i], q31::multiply(s.filtered_left, s.feedback));
        r[s.write] = q31::add(right[i], q31::multiply(s.filtered_right, s.feedback));
        s.write = (s.write + 1) & (history_frames - 1);
        if (s.written < history_frames) ++s.written;
        left[i] = q31::multiply(wet_l, s.level); right[i] = q31::multiply(wet_r, s.level);
    }
}
}
