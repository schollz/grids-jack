# grids-jack

A JACK audio client that plays oneshot WAV samples with infinite polyphony, driven by the Mutable Instruments Grids pattern generator.

## Overview

This project connects to JACK (Jack Audio Connection Kit) as a client, loads all WAV oneshot samples from `data/`, and plays them on demand. At startup, each sample is assigned a random position on the Mutable Instruments Grids X/Y drum map, and the pattern generator runs at a default **120 BPM**. Samples play to completion with no voice limit (infinite polyphony).

## Features

- **Infinite Polyphony**: No voice limit — every trigger spawns a new voice regardless of how many are already playing
- **Grids Pattern Generation**: Uses the legendary Mutable Instruments Grids drum pattern generator
- **Random Pattern Mapping**: Each sample is assigned a random position on the Grids X/Y map at startup, creating unique patterns every run
- **Realtime-Safe Audio**: Lock-free, allocation-free audio processing suitable for professional audio environments
- **Flexible Configuration**: Command-line options for sample directory, tempo, client name, and more
- **Auto-Connection**: Automatically connects to system playback ports
- **Verbose Diagnostics**: Optional detailed logging for debugging and analysis

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

- **SampleBank**: Loads all `.wav` files from `data/` at startup into memory. Each sample is keyed by MIDI note number (parsed from the filename).

- **SamplePlayer**: Owns a pool of active voices (256 pre-allocated). When a trigger fires, it creates a new voice for the corresponding sample. Each voice tracks its own playback position. On each JACK process callback, the SamplePlayer sums all active voices into the output buffer.

- **PatternGenerator**: Port of the Grids drum pattern generator. Runs in the JACK process callback, advancing on each audio block. Produces trigger events for three drum parts (BD, SD, HH). Each sample is assigned a random position on the Grids X/Y parameter map at startup.

## Dependencies

### Required

- **JACK** (Jack Audio Connection Kit): Audio server for low-latency audio routing
  - Ubuntu/Debian: `sudo apt-get install libjack-jackd2-dev jackd2`
  - Fedora/RHEL: `sudo dnf install jack-audio-connection-kit-devel`
  - Arch Linux: `sudo pacman -S jack2`
  - macOS: `brew install jack`

- **libsndfile**: Library for reading and writing audio files
  - Ubuntu/Debian: `sudo apt-get install libsndfile1-dev`
  - Fedora/RHEL: `sudo dnf install libsndfile-devel`
  - Arch Linux: `sudo pacman -S libsndfile`
  - macOS: `brew install libsndfile`

- **CMake**: Build system (version 3.10 or higher)
  - Ubuntu/Debian: `sudo apt-get install cmake`
  - Fedora/RHEL: `sudo dnf install cmake`
  - Arch Linux: `sudo pacman -S cmake`
  - macOS: `brew install cmake`

- **pkg-config**: Tool for managing library compile and link flags
  - Ubuntu/Debian: `sudo apt-get install pkg-config`
  - Fedora/RHEL: `sudo dnf install pkgconfig`
  - Arch Linux: `sudo pacman -S pkgconf`
  - macOS: `brew install pkg-config`

### Optional

- **QjackCtl**: GUI for controlling JACK server
  - Ubuntu/Debian: `sudo apt-get install qjackctl`
  - Fedora/RHEL: `sudo dnf install qjackctl`
  - Arch Linux: `sudo pacman -S qjackctl`
  - macOS: `brew install qjackctl`

## Building

### Clone the Repository

```bash
git clone https://github.com/schollz/grids-jack.git
cd grids-jack
```

### Build with CMake

```bash
mkdir build
cd build
cmake ..
make
```

This will create:
- `grids-jack` — The main executable
- `test_sample_bank` — Unit tests for sample loading
- `test_sample_player` — Unit tests for voice management
- `test_sample_player_integration` — Integration tests for sample playback
- `test_pattern_generator` — Tests for pattern generation timing

### Run Tests

```bash
# From the build directory
./test_sample_bank
./test_sample_player
./test_sample_player_integration
./test_pattern_generator
```

All tests should pass before running the main application.

## Usage

### Starting JACK Server

Before running grids-jack, you need to start a JACK server. You can use QjackCtl (GUI) or the command line:

**Using QjackCtl** (recommended for beginners):
```bash
qjackctl
```
Click "Start" to start the JACK server.

**Using command line**:
```bash
# Start JACK with default settings
jackd -d alsa -r 48000 -p 256

# Or on macOS:
jackd -d coreaudio -r 48000 -p 256
```

Common JACK options:
- `-r 48000`: Sample rate (48kHz is standard, 44100 also common)
- `-p 256`: Buffer size (smaller = lower latency, higher CPU; 256-512 is typical)

### Running grids-jack

#### Basic Usage

```bash
./grids-jack
```

This will:
- Load samples from the `data/` directory
- Connect to JACK as client "grids-jack"
- Run at 120 BPM
- Auto-connect to system playback ports
- Run until you press Ctrl+C

#### Command-Line Options

```bash
./grids-jack [OPTIONS]

Options:
  -d <path>    Sample directory (default: data)
  -b <bpm>     Tempo in BPM (default: 120, range: 0.1-300)
  -n <name>    JACK client name (default: grids-jack)
  -v           Verbose mode - show detailed diagnostic information
  -h           Show help message
```

#### Examples

**Custom BPM**:
```bash
./grids-jack -b 140
```

**Custom sample directory**:
```bash
./grids-jack -d /path/to/my/samples
```

**Verbose mode with custom client name**:
```bash
./grids-jack -v -n my-grids
```

**Multiple options**:
```bash
./grids-jack -d samples -b 128 -n drums -v
```

### Verbose Mode

When you run with the `-v` flag, grids-jack provides detailed diagnostic information:

- JACK buffer duration in milliseconds
- Detailed sample-to-drum-part assignments with X/Y positions
- Pattern generator parameters (X, Y, Randomness)

This is useful for:
- Debugging audio issues
- Understanding pattern generation
- Monitoring performance
- Analyzing sample assignments

## Sample Format

Samples should be placed in the `data/` directory (or a custom directory specified with `-d`). 

### Requirements

- **Format**: 16-bit WAV files
- **Channels**: Mono (stereo files are automatically converted to mono by averaging L+R)
- **Sample Rate**: Any rate (automatically resampled to match JACK sample rate if needed)
- **Naming**: Filenames must encode MIDI note number as the first dot-delimited field

### Naming Convention

```
<MIDI_NOTE>.<anything>.wav

Examples:
60.1.1.1.0.wav  →  MIDI note 60 (C4)
72.kick.wav     →  MIDI note 72 (C5)
48.bass.wav     →  MIDI note 48 (C3)
```

The MIDI note number is used as a unique identifier for each sample. The rest of the filename is ignored.

### Included Samples

The repository includes 15 samples in the `data/` directory:
- MIDI notes 56, 58, 60, 61, 63, 65, 67, 68, 70, 72, 73, 75, 77, 79, 80
- Various drum sounds suitable for rhythm patterns

## How It Works

### Startup Sequence

1. **Parse command-line arguments**: Read configuration (BPM, sample directory, etc.)
2. **Initialize JACK client**: Connect to JACK server, register output ports
3. **Load samples**: Scan sample directory, parse MIDI notes from filenames, load WAV data
4. **Initialize components**: Set up sample player (256-voice pool), pattern generator
5. **Assign samples to drum parts**: Each sample is randomly assigned to BD (Bass Drum), SD (Snare Drum), or HH (Hi-Hat)
6. **Assign X/Y positions**: Each sample gets a random position on the Grids 2D parameter map (0-255 for both X and Y)
7. **Activate JACK client**: Begin audio processing
8. **Auto-connect**: Connect output ports to system playback

### Realtime Operation

In the JACK process callback (called thousands of times per second):

1. **Pattern Generation**: 
   - Advance internal clock based on BPM and buffer size
   - When a pulse occurs (24 pulses per quarter note), consult the Grids pattern
   - Generate triggers for BD, SD, and HH based on current pattern state and density

2. **Sample Triggering**:
   - For each trigger, find all samples assigned to that drum part
   - Create a new voice for each sample (from 256-voice pre-allocated pool)
   - If pool is full, steal oldest voice (voice stealing)

3. **Audio Mixing**:
   - For each active voice:
     - Read samples from current playback position
     - Add to output buffer (mixing)
     - Advance playback position
     - If sample finished (position >= length), mark voice as done
   - Remove finished voices from active list
   - Duplicate mono output to both stereo channels

All of this happens with **zero allocations**, **no locks**, and **no system calls** — ensuring realtime-safe operation.

### Pattern Randomization

Every time you start grids-jack, the sample-to-drum-part assignments and X/Y positions are randomized. This means:

- The same set of samples will produce different patterns each run
- Drum part distribution varies (some runs may have more samples on BD, others on HH, etc.)
- X/Y positions affect trigger probability and density
- Pattern evolution changes based on the Grids algorithm and current parameters

This creates endless variation from the same sample set.

## Performance Considerations

### Voice Pool

grids-jack pre-allocates a pool of **256 voices**. Each trigger creates a new voice, which plays until the sample completes. With a typical drum pattern at 120 BPM, you'll have:

- 3-5 simultaneous voices on average
- 10-20 voices during dense sections
- Up to 256 voices in extreme cases (unlikely but handled)

If the pool is exhausted, the oldest voice is stolen to make room for new triggers.

### CPU Usage

Typical CPU usage (measured on a modern system):
- **Idle**: <1% CPU (pattern generation only, no active voices)
- **Light pattern**: 1-3% CPU (5-10 voices)
- **Dense pattern**: 3-8% CPU (20-30 voices)
- **Maximum polyphony**: 10-20% CPU (100+ voices)

Performance scales linearly with active voices. The realtime-safe design ensures no audio glitches even under high load.

### JACK Configuration

For optimal performance:
- **Sample Rate**: 48000 Hz recommended (44100 also fine)
- **Buffer Size**: 256-512 frames (balance between latency and CPU)
- **Period**: 2 periods is standard

Lower buffer sizes (128 frames) work but require more CPU. Higher buffer sizes (1024 frames) reduce CPU but increase latency.

### Monitoring Performance

**Check for xruns** (buffer underruns):
```bash
# Using JACK tools
jack_cpu_load
```

If you see xruns:
- Increase JACK buffer size
- Close other audio applications
- Check system load (stop background processes)
- Disable CPU frequency scaling (if applicable)

**Monitor with QjackCtl**:
- Real-time CPU load meter
- Xrun counter
- Connection graph

## Troubleshooting

### "Unable to connect to JACK server"

**Problem**: JACK server is not running.

**Solution**:
1. Start JACK server: `qjackctl` (click Start) or `jackd -d alsa`
2. Verify JACK is running: `jack_lsp` (should list ports)

### "No samples could be loaded"

**Problem**: No valid WAV files found in sample directory.

**Solution**:
1. Check sample directory path: `ls -l data/`
2. Verify WAV files have MIDI note in filename: `<NOTE>.*.wav`
3. Try verbose mode: `./grids-jack -v` (shows detailed loading info)
4. Check file permissions (files must be readable)

### "Failed to register output port"

**Problem**: JACK port registration failed (rare).

**Solution**:
1. Restart JACK server
2. Check if another client is using the same name: `jack_lsp | grep grids-jack`
3. Use a different client name: `./grids-jack -n my-grids`

### No audio output

**Problem**: Audio is being generated but you can't hear it.

**Solution**:
1. Check JACK connections: `jack_lsp -c` or use QjackCtl connection graph
2. Manually connect ports:
   ```bash
   jack_connect grids-jack:output_L system:playback_1
   jack_connect grids-jack:output_R system:playback_2
   ```
3. Check system volume (mute, volume level)
4. Verify JACK is routing to correct audio device

### Audio glitches / xruns

**Problem**: Crackling, pops, dropouts in audio.

**Solution**:
1. Increase JACK buffer size (256 → 512 → 1024)
2. Reduce system load (close other apps)
3. Disable CPU frequency scaling:
   ```bash
   # Linux
   sudo cpupower frequency-set -g performance
   ```
4. Check realtime priority:
   ```bash
   # Run JACK with realtime priority
   jackd -R -d alsa -r 48000 -p 512
   ```
5. Consider using a realtime kernel (Linux)

### Pattern doesn't change

**Problem**: Same pattern every time.

**Solution**: This is expected behavior — the pattern itself is deterministic, but **sample assignments** are randomized each run. To get completely different patterns:
1. Change BPM: `./grids-jack -b 140`
2. Restart grids-jack (new sample-to-part assignments)
3. Add/remove samples in `data/` directory

## Development

### Code Structure

```
grids-jack/
├── CMakeLists.txt                  # Build configuration
├── main.cpp                        # Entry point and JACK integration
├── sample_bank.{h,cpp}             # Sample loading and storage
├── sample_player.{h,cpp}           # Voice management and mixing
├── pattern_generator_wrapper.{h,cpp} # Grids integration
├── grids/                          # Grids pattern generator (ported from MI)
│   ├── pattern_generator.{h,cc}
│   ├── resources.{h,cc}
│   └── ...
├── avrlib/                         # Utilities (random number generation)
├── data/                           # Sample files
└── test_*.cpp                      # Unit and integration tests
```

### Realtime Safety

The audio callback path is **100% realtime-safe**:
- ✅ No dynamic allocations (`new`, `malloc`, STL allocations)
- ✅ No locks (`mutex`, `lock_guard`)
- ✅ No system calls (`printf`, file I/O, sleep)
- ✅ No blocking operations
- ✅ Pre-allocated memory for all voices
- ✅ Lock-free data structures

**Realtime code** (called from JACK callback):
- `jack_process_callback()`
- `PatternGeneratorWrapper::Process()`
- `SamplePlayer::Process()`
- `SamplePlayer::Trigger()`
- `Voice` management

**Non-realtime code** (setup/teardown):
- `SampleBank::LoadDirectory()`
- JACK client initialization
- Pattern generator initialization
- Sample assignment

### Modifying the Code

**Change pattern parameters**:
Edit `pattern_generator_wrapper.cpp`, `Init()` function:
```cpp
settings->options.drums.x = 128;       // X position (0-255)
settings->options.drums.y = 128;       // Y position (0-255)
settings->options.drums.randomness = 0; // Randomness (0-255)
```

**Change voice pool size**:
Edit `sample_player.h`:
```cpp
const size_t kMaxVoices = 256;  // Increase or decrease as needed
```

**Add runtime parameter control**:
You can extend `main.cpp` to add MIDI CC or OSC input for realtime parameter changes. Be sure to maintain realtime safety (use atomic operations or lock-free queues).

## License

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

## Credits

- **Grids Pattern Generator**: Based on Mutable Instruments Grids firmware by Emilie Gillet
- **grids-jack Implementation**: schollz and contributors

## Support

- **Issues**: https://github.com/schollz/grids-jack/issues
- **Discussions**: https://github.com/schollz/grids-jack/discussions

## See Also

- [Mutable Instruments Grids](https://mutable-instruments.net/modules/grids/)
- [JACK Audio Connection Kit](https://jackaudio.org/)
- [libsndfile](http://www.mega-nerd.com/libsndfile/)