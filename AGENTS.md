# grids-jack

A JACK audio client that plays oneshot WAV samples with infinite polyphony, driven by the Mutable Instruments Grids pattern generator.

## Overview

This project connects to JACK (Jack Audio Connection Kit) as a client, loads all WAV oneshot samples from `data/`, and plays them on demand. At startup each sample is assigned a random position on the Mutable Instruments Grids X/Y drum map, and the pattern generator runs at a default **120 BPM**. Samples play to completion with no voice limit (infinite polyphony).

## Architecture

```
┌──────────────────────────────────────────────┐
│                  JACK Server                 │
│                                              │
│   audio out ◄── process callback             │
└──────────────────▲───────────────────────────┘
                   │
┌──────────────────┴───────────────────────────┐
│              grids-jack client               │
│                                              │
│  PatternGenerator ──trigger──► SamplePlayer  │
│  (decides when)                (mixes audio) │
│                                    │         │
│                              Voice pool      │
│                           (active voices)    │
│                                    │         │
│                              SampleBank      │
│                         (loaded WAV data)    │
└──────────────────────────────────────────────┘
```

### Key Components

- **SampleBank** — Loads all `.wav` files from `data/` at startup into memory. Each sample is keyed by MIDI note number (parsed from the filename, e.g. `60.1.1.1.0.wav` → note 60). Provides read-only access to sample data for voices.

- **SamplePlayer** — Owns a pool of active voices. When a trigger fires, it creates a new voice for the corresponding sample. Each voice tracks its own playback position. Finished voices (position >= sample length) are destroyed. On each JACK process callback the SamplePlayer sums all active voices into the output buffer. There is no voice limit — every trigger spawns a new voice regardless of how many are already playing.

- **PatternGenerator** — Port of the Grids drum pattern generator (`grids/pattern_generator.cc`). Runs in the JACK process callback, advancing on each audio block at a default tempo of **120 BPM**. Produces trigger events for three drum parts (BD, SD, HH). Each sample from `data/` is assigned a **random position** on the Grids X/Y parameter map at startup, so every run produces a unique pattern arrangement across all loaded samples.

## Data

`data/` contains 16-bit mono WAV oneshot samples. Filenames encode MIDI note number as the first dot-delimited field:

```
60.1.1.1.0.wav  →  MIDI note 60 (C4)
72.1.1.1.0.wav  →  MIDI note 72 (C5)
```

All samples are loaded at startup. No streaming from disk. Each sample is assigned a random Grids X/Y position and mapped to one of the three drum parts (BD, SD, HH), so every launch produces different rhythmic patterns.

## Implementation Notes

- **Language:** C++ (matching the existing Grids codebase).
- **Audio I/O:** JACK C API (`<jack/jack.h>`). Register as a client with a stereo output port pair (or mono, matching the sample format). All audio work happens in the realtime `process` callback — no allocations, no blocking.
- **SamplePlayer realtime safety:** Voice creation in the process callback should use a pre-allocated pool or lock-free structure. Destroying finished voices means returning them to the pool, not calling `delete`.
- **Sample loading:** Use `libsndfile` or a lightweight WAV parser to read files at startup. Convert to float and store at the JACK sample rate (resample if needed).
- **Build:** CMake or a simple Makefile. Link against `libjack` and `libsndfile`.

## Coding Conventions

- Keep realtime code (the JACK process callback and everything it calls) allocation-free and lock-free.
- Prefer simple C-style data structures for audio voices over deep class hierarchies.
- Match the existing code style: descriptive names, minimal abstraction.
