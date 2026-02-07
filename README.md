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
-d <path>    Sample directory (default: data)
-b <bpm>     Tempo in BPM (default: 120)
-n <name>    JACK client name (default: grids-jack)
-v           Verbose output
-h           Show help
```

Press `Ctrl+C` to stop.

## Samples

Place `.wav` files in `data/`. Filenames must start with a MIDI note number as the first dot-delimited field:

```
60.kick.wav       -> note 60
72.1.1.1.0.wav    -> note 72
```

Samples are loaded into memory at startup and play to completion with no voice cutting (up to 256 simultaneous voices).

## How it works

Grids uses a 2D map of drum patterns where X/Y coordinates blend between rhythmic styles. At startup, grids-jack assigns each loaded sample to one of three drum parts (BD/SD/HH) with a random X/Y position on this map. The pattern generator then triggers samples at the configured BPM according to the resulting pattern. Mono output is sent to both stereo JACK output ports.

## License

Based on Mutable Instruments Grids firmware by Emilie Gillet.
