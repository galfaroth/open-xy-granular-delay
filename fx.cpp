#include "core.h"
#include <opxy/q31.hpp>
namespace graindelay {
namespace {
using namespace opxy;
const u32 mask = history_frames - 1;
i32 control(i32 x) { return x < 0 ? 0 : x; }
u32 scaled(i32 x, u32 range) { return (static_cast<u32>(x) >> 16) * range / 32767; }
// x + d / 64 (truncated), or at least one step toward target; branch-free.
i32 glide(i32 x, i32 target) {
    const i32 d = target - x, q = (d + ((d >> 31) & 63)) >> 6, sign = (d >> 31) | (d != 0);
    return x + (q | (sign & -(q == 0)));
}
u32 random(State& s) { s.random ^= s.random << 13; s.random ^= s.random >> 17; s.random ^= s.random << 5; return s.random; }

// One straight piece of a grain's triangular window: n samples read at a
// 16.16 position from the interleaved history, linearly interpolated, scaled
// by the window and added into the interleaved wet block. The history holds
// half-scale samples, so neighbour differences never overflow. `checked` wraps
// the neighbour at the end of the history and reads zero for frames at or past
// `limit` (unwritten since reset while the history first fills); it is used
// only for the rare pieces that need either. Otherwise every read is known to
// be written history short of the end.
template <bool checked>
__attribute__((noinline)) void ramp(const i32* history, i32* out, u32 n, Grain& grain, i32 step, u32 limit) {
    u32 position = grain.position;
    const u32 rate = grain.rate;
    i32 window = grain.window;
    for (u32 k = 0; k < n; ++k) {
        const u32 index = position >> 16;
        const i32 fraction = static_cast<i32>((position & 65535) << 15);
        const i32* a = history + 2 * index;
        i32 al, ar, bl, br;
        if (checked) {
            const u32 next = (index + 1) & mask;
            const i32* b = history + 2 * next;
            al = index < limit ? a[0] : 0; ar = index < limit ? a[1] : 0;
            bl = next < limit ? b[0] : 0; br = next < limit ? b[1] : 0;
        } else {
            al = a[0]; ar = a[1]; bl = a[2]; br = a[3];
        }
        out[2 * k] += q31::multiply(al + q31::multiply(bl - al, fraction), window);
        out[2 * k + 1] += q31::multiply(ar + q31::multiply(br - ar, fraction), window);
        position += rate;
        window += step;
    }
    grain.position = position;
    grain.window = window;
}

// The same for a whole-frame position and a stride of one or two frames
// (pitch 0 and +12 from a launch on a frame): no interpolation is needed.
// n must not run the reads past the end of the history.
__attribute__((noinline)) void stride(const i32* frame, i32* out, u32 n, i32 window, i32 step, u32 words) {
    for (u32 k = 0; k < n; ++k) {
        out[2 * k] += q31::multiply(frame[0], window);
        out[2 * k + 1] += q31::multiply(frame[1], window);
        frame += words;
        window += step;
    }
}

void ramp(const i32* history, i32* out, u32 n, Grain& grain, i32 step, u32 limit) {
    const u32 index = grain.position >> 16;
    // True when no read of this piece, neighbour included, reaches the last frame.
    const bool inside = index + ((grain.rate * n) >> 16) + 2 < history_frames;
    if (__builtin_expect(index >= limit, 0)) {
        // Launched before the reset point: reads stay unwritten (silent) until
        // they wrap past the end, so a piece that does not wrap adds nothing.
        if (inside) {
            grain.position += grain.rate * n;
            grain.window += step * static_cast<i32>(n);
        } else {
            ramp<true>(history, out, n, grain, step, limit);
        }
        return;
    }
    if ((grain.position | grain.rate) & 65535) {
        if (__builtin_expect(inside, 1)) ramp<false>(history, out, n, grain, step, limit);
        else ramp<true>(history, out, n, grain, step, limit);
        return;
    }
    const u32 frames = grain.rate >> 16;  // 1 or 2
    // Samples before the reads wrap to the start of the history.
    const u32 before = (history_frames - index + frames - 1) >> (frames - 1);
    const u32 first = n < before ? n : before;
    stride(history + 2 * index, out, first, grain.window, step, 2 * frames);
    if (first < n) {
        const u32 wrapped = (index + first * frames) & mask;
        stride(history + 2 * wrapped, out + 2 * first, n - first, grain.window + step * static_cast<i32>(first), step, 2 * frames);
    }
    grain.position += grain.rate * n;
    grain.window += step * static_cast<i32>(n);
}

// Renders up to n samples of one grain: the rest of its rise, then its fall.
void render(Grain& grain, const i32* history, i32* out, u32 n, u32 limit) {
    if (grain.rise) {
        const u32 k = n < grain.rise ? n : grain.rise;
        ramp(history, out, k, grain, grain.step, limit);
        grain.rise -= k; out += 2 * k; n -= k;
    }
    if (n && grain.fall) {
        const u32 k = n < grain.fall ? n : grain.fall;
        ramp(history, out, k, grain, -grain.step, limit);
        grain.fall -= k;
    }
}

// Damping, feedback write-back and level for the whole block. The history
// write position never wraps inside a block (65536 is a multiple of 64).
template <bool gliding>
__attribute__((noinline)) void finish(State& s, i32* write, const i32* wet, Channel left, Channel right,
                                      i32 feedback_target, i32 level_target, i32 tone) {
    i32 feedback = s.feedback, level = s.level, fl = s.filtered_left, fr = s.filtered_right;
    const i32 keep = q31::maximum - tone;
    for (u32 i = 0; i < block_frames; ++i) {
        if (gliding) { feedback = glide(feedback, feedback_target); level = glide(level, level_target); }
        const i32 wl = wet[2 * i], wr = wet[2 * i + 1];
        fl = q31::add(q31::multiply(wl, tone), q31::multiply(fl, keep));
        fr = q31::add(q31::multiply(wr, tone), q31::multiply(fr, keep));
        write[2 * i] = q31::add(left[i], q31::multiply(fl, feedback)) >> 1;
        write[2 * i + 1] = q31::add(right[i], q31::multiply(fr, feedback)) >> 1;
        left[i] = q31::multiply(wl, level); right[i] = q31::multiply(wr, level);
    }
    s.feedback = feedback; s.level = level; s.filtered_left = fl; s.filtered_right = fr;
}
}
void initialize(State& s) {
    s = State(); s.random = 0x51f15e1u;
    s.raw_time = s.raw_grain = s.raw_density = s.raw_spray = -1;
}
void process(State& s, opxy::Buffer buffer, opxy::Channel left, opxy::Channel right, opxy::Parameters p) {
    using namespace opxy;
    if (!buffer.words || buffer.count < buffer_words) {
        initialize(s);
        for (u32 i = 0; i < block_frames; ++i) left[i] = right[i] = 0;
        return;
    }
    i32* history = buffer.words;
    // Derived values change only with their controls: divide only then.
    const i32 time = control(p[0]), grain = control(p[1]), spray = control(p[5]);
    const i32 density = static_cast<i32>(1 + ((static_cast<u32>(control(p[2])) >> 16) * 4 >> 15));
    if (time != s.raw_time) { s.raw_time = time; s.delay = 882 + scaled(time, 43218); }
    if (spray != s.raw_spray) { s.raw_spray = spray; s.spray = scaled(spray, 4410); }
    if (grain != s.raw_grain || density != s.raw_density) {
        s.raw_grain = grain; s.raw_density = density;
        s.length = (220 + scaled(grain, 3748)) & ~3u;
        s.spacing = s.length / static_cast<u32>(density);
        s.half = s.length / 2;
        // Peak window just under 1/2 on half-scale history: each of the four
        // overlapping readers gets a quarter of the mix.
        s.step = (2147483647u / s.half) >> 1;
    }
    const u32 rates[5] = {32768, 43740, 65536, 98193, 131072};
    const u32 rate = rates[(static_cast<u32>(control(p[4])) >> 16) * 5 >> 15];
    // Pitch-up grains start further back so they never overtake the write head.
    const u32 catchup = rate > 65536 ? (rate - 65536) * s.length >> 16 : 0;
    const i32 feedback = q31::multiply(control(p[3]), 1975684955);
    const i32 tone = q31::maximum - q31::multiply(control(p[6]), 2040109465);
    // Until the history has filled once, frames at or past the write position
    // were never written since reset and read as silence.
    const u32 limit = s.written < history_frames ? s.write : history_frames;

    i32 wet[2 * block_frames];
    for (u32 i = 0; i < 2 * block_frames; ++i) wet[i] = 0;
    for (u32 i = 0; i < block_frames;) {
        if (!s.countdown) {
            for (u32 g = 0; g < grain_count; ++g) {
                Grain& slot = s.grains[g];
                if (slot.rise | slot.fall) continue;
                u32 extra = 0;
                if (s.spray) {
                    // Exactly (r * spray) / 65535 for these ranges, without a division.
                    const u32 x = (random(s) >> 16) * s.spray;
                    extra = (x + (x >> 16) + 1) >> 16;
                }
                slot.position = (s.write + i - s.delay - extra - catchup - 2) << 16;
                slot.rate = rate; slot.rise = slot.fall = s.half;
                slot.window = 0; slot.step = static_cast<i32>(s.step);
                break;
            }
            s.countdown = s.spacing;
        }
        const u32 n = s.countdown < block_frames - i ? s.countdown : block_frames - i;
        for (u32 g = 0; g < grain_count; ++g)
            if (s.grains[g].rise | s.grains[g].fall) render(s.grains[g], history, wet + 2 * i, n, limit);
        s.countdown -= n; i += n;
    }
    i32* write = history + 2 * s.write;
    const i32 level = control(p[7]);
    if (s.feedback != feedback || s.level != level) finish<true>(s, write, wet, left, right, feedback, level, tone);
    else finish<false>(s, write, wet, left, right, feedback, level, tone);
    s.write = (s.write + block_frames) & mask;
    if (s.written < history_frames) s.written += block_frames;
}
}
