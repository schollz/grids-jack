# grids-jack Implementation Strategy

This document outlines a multiphase approach to implementing the grids-jack JACK audio client that plays oneshot WAV samples with infinite polyphony, driven by the Mutable Instruments Grids pattern generator.

## Overview

The implementation will connect to JACK as a client, load WAV samples from `data/`, and play them on demand with infinite polyphony at 120 BPM. Each sample is assigned a random position on the Grids X/Y drum map at startup.

---

## Phase 1: Foundation and Build System

### Goal
Set up the project structure, build system, and basic dependencies.

### Tasks
1. **Create CMakeLists.txt**
   - Add project definition with C++11 or higher
   - Find and link JACK library (`libjack`)
   - Find and link libsndfile for WAV loading
   - Set compiler flags for realtime safety warnings (`-Wall -Wextra`)
   - Create executable target linking all components

2. **Create main.cpp**
   - Basic program entry point
   - Command-line argument parsing (optional: sample directory, BPM)
   - JACK client initialization stub
   - Graceful shutdown handling (SIGINT, SIGTERM)

3. **Verify build**
   - Test that project compiles
   - Verify JACK and libsndfile dependencies are found
   - Ensure clean build with no warnings

### Deliverables
- `CMakeLists.txt` with all dependencies configured
- `main.cpp` with basic structure
- Successful compilation

---

## Phase 2: Sample Bank Implementation

### Goal
Create a component that loads all WAV files from `data/` directory into memory at startup.

### Tasks
1. **Create sample_bank.h**
   - Define `Sample` struct:
     - `std::vector<float> data` - audio samples as floats
     - `uint32_t length` - sample length in frames
     - `uint8_t midi_note` - MIDI note number (parsed from filename)
   - Define `SampleBank` class:
     - `bool LoadDirectory(const std::string& path)` - load all WAV files
     - `const Sample* GetSample(uint8_t midi_note) const` - lookup by note
     - `std::vector<uint8_t> GetAllNotes() const` - get list of loaded notes
   - Storage: `std::map<uint8_t, Sample>` for samples by MIDI note

2. **Create sample_bank.cpp**
   - Implement directory scanning (using `<dirent.h>` or `<filesystem>`)
   - Parse MIDI note from filename (first dot-delimited field)
   - Load WAV files using libsndfile:
     - Open file with `sf_open()`
     - Read metadata (sample rate, channels, length)
     - Allocate buffer and read samples as float
     - If stereo, convert to mono (average L+R)
     - If sample rate differs from JACK rate, resample (linear interpolation)
   - Handle errors gracefully (missing files, corrupt WAV, etc.)
   - Log loaded samples (MIDI note, length, filename)

3. **Test Sample Bank**
   - Unit test loading samples from `data/`
   - Verify MIDI note parsing
   - Verify sample data is loaded correctly
   - Test error cases

### Deliverables
- `sample_bank.h` and `sample_bank.cpp`
- All samples from `data/` loaded successfully
- MIDI note mapping validated

---

## Phase 3: Voice Pool and Sample Player

### Goal
Create a realtime-safe voice system for playing samples with infinite polyphony.

### Tasks
1. **Create sample_player.h**
   - Define `Voice` struct:
     - `const float* sample_data` - pointer to sample data (non-owning)
     - `uint32_t sample_length` - length in frames
     - `uint32_t position` - current playback position
     - `float gain` - volume (default 1.0)
   - Define `SamplePlayer` class:
     - `void Init(const SampleBank* bank, uint32_t sample_rate)`
     - `void Trigger(uint8_t midi_note, float velocity)` - spawn new voice
     - `void Process(float* output, uint32_t num_frames)` - mix active voices
     - Pre-allocated voice pool (e.g., 256 voices using `std::array`)
     - Active voice tracking with indices

2. **Create sample_player.cpp**
   - Implement voice allocation:
     - Use a free list or circular buffer for voice pool
     - On trigger, find free voice slot and initialize
     - If pool full, reuse oldest voice (voice stealing)
   - Implement `Process()`:
     - Zero output buffer
     - For each active voice:
       - Read samples from current position
       - Add to output buffer (mixing)
       - Increment position
       - If position >= length, mark voice as finished
     - Remove finished voices from active list
   - Ensure no allocations in `Process()` and `Trigger()`

3. **Test Sample Player**
   - Test voice allocation and deallocation
   - Test mixing multiple voices
   - Verify realtime safety (no allocations)
   - Test voice stealing when pool is full

### Deliverables
- `sample_player.h` and `sample_player.cpp`
- Realtime-safe voice management
- Proper sample mixing with infinite polyphony

---

## Phase 4: Pattern Generator Integration

### Goal
Adapt the existing Grids pattern generator to work with JACK and trigger samples.

### Tasks
1. **Create pattern_generator_wrapper.h**
   - Wrapper class for `grids::PatternGenerator`
   - Convert BPM to JACK sample rate timing
   - Map samples to Grids drum parts (BD, SD, HH)
   - Random X/Y position assignment for each sample

2. **Create pattern_generator_wrapper.cpp**
   - Initialize Grids pattern generator
   - Implement `Process(uint32_t num_frames)`:
     - Calculate elapsed time since last tick
     - Advance pattern generator by appropriate pulses
     - Check for triggers from Grids (state bits)
     - Call `SamplePlayer::Trigger()` for each active drum part
   - Map loaded samples to drum parts:
     - Assign each sample a random X/Y position (0-255)
     - Randomly distribute samples across BD, SD, HH parts
     - Store mapping at initialization
   - Handle tempo changes (default 120 BPM)

3. **Remove AVR dependencies**
   - Replace AVR-specific types (`prog_uint8_t`, `PROGMEM`)
   - Replace `<avr/eeprom.h>`, `<avr/pgmspace.h>` with standard alternatives
   - Replace `pgm_read_byte()` with direct memory access
   - Replace `avrlib` dependencies with standard C++ equivalents
   - Keep pattern generation logic intact

4. **Test Pattern Generator**
   - Verify pattern generation matches expected behavior
   - Test trigger generation at different tempos
   - Verify sample-to-part mapping

### Deliverables
- `pattern_generator_wrapper.h` and `pattern_generator_wrapper.cpp`
- Modified `grids/pattern_generator.cc` and `grids/pattern_generator.h` (if needed)
- Pattern generator working without AVR dependencies
- Correct timing at 120 BPM

---

## Phase 5: JACK Integration

### Goal
Connect all components to JACK and implement the realtime audio callback.

### Tasks
1. **Implement JACK client setup in main.cpp**
   - Open JACK client with `jack_client_open()`
   - Get sample rate with `jack_get_sample_rate()`
   - Register stereo output ports (or mono)
   - Set process callback
   - Activate client with `jack_activate()`
   - Auto-connect to system playback ports (optional)

2. **Implement JACK process callback**
   - Get output port buffers
   - Calculate current tempo and advance pattern generator
   - Call `PatternGenerator::Process()` for triggers
   - Call `SamplePlayer::Process()` to mix audio
   - Write output to JACK buffers
   - Ensure callback is realtime-safe (no locks, no allocations)

3. **Implement shutdown handling**
   - Register JACK shutdown callback
   - Handle JACK disconnection gracefully
   - Clean up resources on exit

4. **Test JACK integration**
   - Start JACK server
   - Run grids-jack client
   - Verify audio output
   - Test with different buffer sizes
   - Verify realtime safety (no xruns)

### Deliverables
- Complete `main.cpp` with JACK integration
- Realtime-safe process callback
- Clean shutdown and error handling
- Working audio output to JACK

---

## Phase 6: Polishing and Testing

### Goal
Final refinements, testing, and documentation.

### Tasks
1. **Add logging and diagnostics**
   - Log loaded samples at startup
   - Log JACK connection status
   - Log sample rate and buffer size
   - Optional: verbose mode for debugging

2. **Performance testing**
   - Test with maximum polyphony (many simultaneous voices)
   - Monitor CPU usage
   - Verify no buffer underruns (xruns)
   - Profile realtime callback performance

3. **Add configuration options**
   - Command-line arguments for:
     - Sample directory path
     - BPM (tempo)
     - JACK client name
     - Pattern X/Y parameters
   - Optional: runtime controls (MIDI input, OSC)

4. **Documentation**
   - Update README.md with:
     - Build instructions
     - Usage examples
     - Dependencies
     - Configuration options
   - Add comments to complex code sections
   - Document realtime safety considerations

5. **Final testing**
   - End-to-end test: build → run → verify audio
   - Test with all samples in `data/`
   - Test error cases (missing JACK, missing samples)
   - Verify pattern randomization on each run

### Deliverables
- Comprehensive logging
- Performance validation
- Complete documentation
- Final tested release

---

## Implementation Guidelines

### Realtime Safety Rules
- **No allocations** in JACK process callback or anything it calls
- **No locks** in realtime code (use lock-free data structures if needed)
- **No system calls** (file I/O, printf, etc.) in realtime path
- Pre-allocate all memory at startup
- Use atomic operations for cross-thread communication if needed

### Code Style
- Match existing Grids code style: descriptive names, minimal abstraction
- Prefer C-style data structures for performance-critical code
- Use modern C++ (C++11+) for non-realtime code (file loading, setup)
- Keep functions small and focused
- Document realtime vs. non-realtime code

### Testing Strategy
- Unit test each component in isolation
- Integration test with actual JACK server
- Test with various audio configurations
- Profile realtime performance
- Verify no memory leaks (valgrind)

### Dependencies
- **JACK**: Audio server (required)
- **libsndfile**: WAV file loading (required)
- **C++11 or higher**: Modern C++ features
- **CMake**: Build system

---

## Appendix: File Structure

```
grids-jack/
├── CMakeLists.txt               # Build configuration
├── main.cpp                     # Entry point and JACK setup
├── sample_bank.h                # Sample loading interface
├── sample_bank.cpp              # Sample loading implementation
├── sample_player.h              # Voice pool and mixing interface
├── sample_player.cpp            # Voice pool and mixing implementation
├── pattern_generator_wrapper.h  # Grids integration interface
├── pattern_generator_wrapper.cpp# Grids integration implementation
├── grids/                       # Existing Grids code (modified)
│   ├── pattern_generator.h
│   ├── pattern_generator.cc
│   ├── resources.h
│   ├── resources.cc
│   └── ...
├── data/                        # WAV samples
│   ├── 60.1.1.1.0.wav
│   ├── 72.1.1.1.0.wav
│   └── ...
└── README.md                    # Documentation
```

---

## Success Criteria

The implementation is complete when:
1. ✅ All WAV samples load from `data/` directory
2. ✅ JACK client connects and registers output ports
3. ✅ Pattern generator runs at 120 BPM
4. ✅ Samples trigger according to Grids patterns
5. ✅ Infinite polyphony works (voices don't cut each other)
6. ✅ Audio plays cleanly with no xruns
7. ✅ Realtime code is allocation-free and lock-free
8. ✅ Each run produces different patterns (random X/Y positions)
9. ✅ Build system works cleanly
10. ✅ Documentation is complete

---

## Timeline Estimate

- **Phase 1**: 2-4 hours (build system and skeleton)
- **Phase 2**: 4-6 hours (sample loading and parsing)
- **Phase 3**: 6-8 hours (voice system and mixing)
- **Phase 4**: 6-8 hours (pattern generator adaptation)
- **Phase 5**: 4-6 hours (JACK integration)
- **Phase 6**: 4-6 hours (testing and polish)

**Total**: 26-38 hours

---

## Risk Mitigation

### Risk: AVR dependencies in Grids code
**Mitigation**: Create wrapper layer that isolates AVR-specific code. Replace PROGMEM tables with regular const arrays.

### Risk: Realtime safety violations
**Mitigation**: Use static analysis tools, review all realtime code paths, test with JACK's `-R` flag to catch violations.

### Risk: Sample rate mismatches
**Mitigation**: Implement simple linear interpolation resampler during sample loading.

### Risk: Audio glitches under heavy load
**Mitigation**: Pre-allocate generous voice pool, optimize mixing loop, profile with realistic test cases.

### Risk: Pattern generator timing inaccuracies
**Mitigation**: Use JACK's transport time for precise timing, test against reference tempo.

---

## Future Enhancements (Out of Scope)

- MIDI input for manual triggers
- OSC control for parameters
- Real-time tempo changes
- Save/load pattern presets
- GUI for visualization
- Multi-output routing
- Sample hot-reloading
- LADSPA/LV2 plugin version
