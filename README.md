# Granular Delay for Open-XY

**Status: planning only.** This repository currently contains a design brief, not an implemented or installable effect. No DSP, firmware, presets or measured hardware results are included.

## Goal

A stereo delay that reads overlapping grains from its own input history. Turn incoming drums, synths or vocals into rhythmic fragments and diffuse echoes. This is a live send effect, separate from the Granular sample instrument.

## Proposed MVP signal flow

Stereo input → bounded circular history → overlapping windowed grain readers → damped feedback → wet return.

Start with a fixed maximum grain count, no dynamic allocation, and a defined maximum delay based on the SDK scratch buffer. Grain pitch comes from read speed; interpolation and anti-aliasing quality must be evaluated. Keep feedback below a documented safe bound and guard unwritten buffer regions after reset. Freeze and reverse are later additions.

## Proposed controls

| Primary | Purpose |
|---|---|
| Time | Delay position / tempo division |
| Grain | Grain duration |
| Density | Overlap / grain launch rate |
| Feedback | Repeat amount |

Proposed alternate controls: pitch, position spray, damping and wet level. Tempo sync can use the SDK transport tempo; it does not imply access to an absolute transport position.

Control assignments are a starting design, not a published compatibility contract. Provide at least a neutral/default sound and a few useful settings when implementation begins.

## Routing and resources

Return wet audio only and preserve the normal dry path by default. Use the public per-send scratch buffer (up to 512 KiB); choose stereo history length and active grain count only after memory and CPU measurement. Scratch memory is not cleared by the SDK: track initialized history explicitly.

Target a standalone Open-XY `effect` machine. The SDK supplies the adapters; this repository owns the DSP, controls, artwork and sound tests. SDK changes, if needed, belong in separate upstream PRs.

The current SDK processes stereo Q31 blocks of 64 samples at 44.1 kHz. Unbuffered state is at most 476 bytes; buffered state at most 472 bytes, with an additional reduction when using a shift row. Audio callbacks cannot allocate, block, perform I/O, log or draw. Do not assume a host estimate proves device timing.

## Implementation handoff

1. Start from a current SDK checkout beside this repository and read its authoring/UX instructions.
2. Generate the official effect starter into a fresh sibling directory, then bring its scaffold into this repository while preserving this brief:
   ```sh
   python3 ../open-xy/sdk.py new --repository --kind effect --id graindelay --name "Granular Delay" --out ../open-xy-granular-delay-starter
   ```
3. Implement the smallest signal path, controls and default settings; keep firmware addresses and platform code out of the effect.
4. Run the machine's tests with `python3 ../open-xy/sdk.py test --machine .` once the scaffold is present. Follow the generated README if its layout changes.
5. Measure memory and worst-case device CPU, then test an explicitly authorized development build before catalog submission.

No catalog entry or upstream PR is part of this planning repository. No firmware or stock content may be committed here.

## Validation plan

Impulse timing; bounded feedback decay; grain-window continuity; circular wrap; extreme grain rates; pitch endpoints; buffer-empty behavior; stale-memory protection after reset; independent operation on both sends.

Host numerical tests, native build verification, listening tests and revision-bound hardware CPU measurements are separate requirements. No browser audition support is assumed for effects.

## SDK reference

Planning checked against Open-XY staging revision [`64bd090`](https://github.com/etcetc000/open-xy/tree/64bd0900a08a134e274a8b8decbc8b734ca534dd) on 2026-11-01:

- [Effect API and scratch buffers](https://github.com/etcetc000/open-xy/blob/64bd0900a08a134e274a8b8decbc8b734ca534dd/docs/api.md#effects)
- [Authoring journey](https://github.com/etcetc000/open-xy/blob/64bd0900a08a134e274a8b8decbc8b734ca534dd/docs/first-machine.md)
- [UX conventions](https://github.com/etcetc000/open-xy/blob/64bd0900a08a134e274a8b8decbc8b734ca534dd/docs/ux.md)
- [Hardware profiling](https://github.com/etcetc000/open-xy/blob/64bd0900a08a134e274a8b8decbc8b734ca534dd/docs/profiling.md)

## Next step

Assign a dedicated implementation agent after the brief is reviewed. Do not start other effects or modify the SDK as part of this repository's initial implementation.
