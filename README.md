# grids-jack

A JACK audio client that plays WAV samples as generative drum patterns using the [Mutable Instruments Grids](https://mutable-instruments.net/modules/grids/) pattern generator. Every run randomizes sample assignments, so it sounds different each time.

## Dependencies

Ubuntu/Debian:

```bash
sudo apt-get install cmake pkg-config libjack-jackd2-dev libsndfile1-dev
```

macOS:

```bash
brew install cmake pkg-config jack libsndfile
```

## Build

```bash
make
```

## Usage

Start a JACK server first (e.g. `jackd -d alsa` or via QjackCtl), then:

```bash
./build/grids-jack
```

Or simply `make run` to build and run in one step.

### Options

```
-d <path>      Sample directory (default: data)
-b <bpm>       Tempo in BPM (default: 120)
-n <name>      JACK client name (default: grids-jack)
-s <steps>     Velocity pattern steps per sample (default: 32)
-p <parts>     Number of random samples to select (default: 4)
-o <gain>      Global output volume scaling (default: 1.0)
-u <amt>       Humanize timing, 0.0-1.0 (default: 0.0)
-r <spread>    Stereo spread, 0.0-1.0 (default: 0.0)
-l             Enable LFO drift of x/y pattern positions
-v             Verbose output
-h             Show help
```

`-u` adds random timing jitter to note triggers. At 1.0, notes can shift up to half a pattern step early or late. At lower values the displacement is proportionally smaller.

`-r` distributes instruments evenly across the stereo field using equal-power panning. At 1.0, instruments span the full left-to-right range. For example, 3 parts at `-r 0.5` are panned at -0.5, 0.0, and +0.5.

Press `Ctrl+C` to stop.

## Samples

Place `.wav` files in `data/`. Filenames must start with a MIDI note number as the first dot-delimited field:

```
60.kick.wav       -> note 60
72.1.1.1.0.wav    -> note 72
```

Samples are loaded into memory at startup and play to completion with no voice cutting (up to 256 simultaneous voices).

## How it works

Grids uses a 2D map of drum patterns where X/Y coordinates blend between rhythmic styles. At startup, grids-jack assigns each loaded sample to one of three drum parts (BD/SD/HH) with a random X/Y position on this map. The pattern generator then triggers samples at the configured BPM according to the resulting pattern. Output is sent to stereo JACK ports (mono center by default, or panned with `-r`).

## License

Based on Mutable Instruments Grids firmware by Emilie Gillet.
