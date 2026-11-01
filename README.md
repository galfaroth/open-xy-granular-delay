# Granular Delay by Veff

A stereo **live-input granular send delay** for Open-XY / OP-XY OS 1.2.1.
Four overlapping triangular-window readers play a bounded stereo history at
independent, launch-time pitch and delay positions. This is an effect, not a
sample instrument. It returns wet audio only; the normal dry mixer is preserved.

Press any encoder to switch the whole four-control page.

| Page 1 | Range | Page 2 | Range |
|---|---|---|---|
| Time | 20–1000 ms | Pitch | −12, −7, 0, +7, +12 semitones |
| Grain | 5–90 ms | Spray | 0–100 ms of extra random delay |
| Density | 1–4 overlapping grains | Damping | 0–100, darker clockwise |
| Feedback | 0–92% | Level | 0–100% wet return |

The default is a clean, unpitched echo, with no spray or damping. Time, grain
and pitch changes apply to newly launched grains; existing windows finish.
Feedback and level glide. Four fixed readers share conservative 1/4 gain each:
maximum-density steady material has about 6 dB wet headroom, while sparse
settings are intentionally quieter. Feedback is damped and capped below unity.
Pitch-up adds read headroom so no grain overtakes the live write cursor; the
resulting delay is longer than Time. Interpolation is linear, not a band-limited
pitch shifter; bright material can alias when pitched up.

Useful starting settings (set page 2 level to 100):

- **Clean echo:** Time 250 ms, Grain 35 ms, Density 4, Feedback 28%; Pitch 0, Spray 0, Damping 0.
- **Octave cloud:** Time 500 ms, Grain 80 ms, Density 4, Feedback 50%; Pitch +12, Spray 70 ms, Damping 35.
- **Sparse fragments:** Time 120 ms, Grain 15 ms, Density 1, Feedback 20%; Pitch −7, Spray 40 ms, Damping 15.

## Implementation and validation

Scratch: 524288 bytes per send (65536 stereo frames, 1.49 s). State fits the
SDK's 472-byte buffered-effect slot. Reset invalidates history without clearing
the large buffer; unwritten reads return zero. No allocation, I/O or platform
code is used in audio callbacks. Both sends have independent state and scratch.

Built against official SDK revision `7fd8fdf1757e11ab6fdca915b09f1773d7611a31`.
From this repository, with the SDK checked out beside it:

```sh
python ../open-xy/sdk.py test --machine .
python ../open-xy/sdk.py compile --machine . --machine ../open-xy-core-machines/machines/fm2op --out /tmp/graindelay-native
```

The official SDK currently needs a synth in a native collection; the second
machine is the public 2op FM baseline. Numerical tests cover poisoned/reset
scratch, exact neutral delay, stereo isolation/balance, overlap gain through
ring wrap, capped feedback decay, pitch extremes and missing scratch. Effects
have no browser audition in this SDK. Host CPU estimates are not measured device
CPU; listening and revision-bound hardware profiling remain pending. Do not
install firmware from this source repository; use a separately verified build.

The original plan is retained in [docs/DESIGN-BRIEF.md](docs/DESIGN-BRIEF.md).

The generated CI template is kept in `ci/machine.yml.example`. Activate it in
`.github/workflows/` when publishing credentials permit workflow changes.
