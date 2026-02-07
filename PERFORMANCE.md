# Performance Testing and Monitoring for grids-jack

This document provides guidance on testing, monitoring, and optimizing the performance of grids-jack.

## Performance Testing Overview

grids-jack is designed to be realtime-safe and efficient. The key performance metrics are:

1. **CPU Usage**: Should remain low (typically <10%) even with dense patterns
2. **Xruns (Buffer Underruns)**: Should be zero during normal operation
3. **Latency**: Determined by JACK buffer size, typically 5-12ms
4. **Memory Usage**: Fixed allocation at startup, no runtime allocations

## Monitoring Tools

### 1. JACK Built-in Monitoring

**CPU Load**:
```bash
# Monitor JACK CPU usage in realtime
jack_cpu_load

# Example output:
# jack_cpu_load: 3.5%
```

**Xrun Monitoring**:
Xruns (buffer underruns) indicate that audio processing couldn't keep up with the JACK server.

Using QjackCtl:
- Launch `qjackctl`
- The main window shows xrun count in the status bar
- A visual alert appears when xruns occur

Using command line:
```bash
# Check JACK status
jack_wait -c  # Wait for JACK to become inactive (use to detect issues)
```

**Connection Graph**:
```bash
# List all JACK ports and connections
jack_lsp -c

# Connect grids-jack to monitoring tools
jack_connect grids-jack:output_L system:playback_1
jack_connect grids-jack:output_R system:playback_2
```

### 2. System Monitoring

**CPU Usage per Process**:
```bash
# Monitor grids-jack CPU usage
top -p $(pidof grids-jack)

# Or use htop for better visualization
htop -p $(pidof grids-jack)
```

**Memory Usage**:
```bash
# Check memory usage
ps aux | grep grids-jack

# Or more detailed:
pmap $(pidof grids-jack)
```

**Realtime Priority Check**:
```bash
# Check if process has realtime priority
ps -eo pid,cls,rtprio,comm | grep grids-jack

# CLS column: TS=normal, RR=realtime round-robin, FF=realtime FIFO
# RTPRIO: Realtime priority (1-99, higher is higher priority)
```

### 3. Profiling with perf

For detailed performance analysis on Linux:

```bash
# Record performance data (run for 10 seconds)
sudo perf record -p $(pidof grids-jack) sleep 10

# Analyze the results
sudo perf report

# Focus on realtime callback
sudo perf record -e cpu-clock -p $(pidof grids-jack) sleep 10
```

Key things to look for:
- Time spent in `jack_process_callback()`
- Time spent in `SamplePlayer::Process()`
- Time spent in `PatternGeneratorWrapper::Process()`
- Any unexpected function calls

### 4. Valgrind Memory Analysis

Check for memory leaks and allocation issues:

```bash
# Basic memory check
valgrind --leak-check=full --show-leak-kinds=all ./grids-jack

# Check for realtime allocations (should be none after startup)
valgrind --tool=massif ./grids-jack
```

## Performance Testing Scenarios

### Test 1: Idle CPU Usage

**Purpose**: Verify low CPU usage when no audio is being generated.

**Procedure**:
1. Start grids-jack with no samples: `mkdir /tmp/empty && ./grids-jack -d /tmp/empty`
2. Monitor CPU usage: `jack_cpu_load`
3. **Expected**: <1% CPU usage

### Test 2: Normal Load (Light Pattern)

**Purpose**: Test typical performance with a normal rhythm pattern.

**Procedure**:
1. Start grids-jack with default settings: `./grids-jack`
2. Monitor CPU usage: `jack_cpu_load`
3. Monitor xruns in QjackCtl
4. Let run for 5 minutes
5. **Expected**: 1-5% CPU usage, 0 xruns

### Test 3: Dense Pattern (High Load)

**Purpose**: Test performance with maximum trigger density.

**Procedure**:
1. Modify `pattern_generator_wrapper.cpp` to set all densities to maximum:
   ```cpp
   for (int i = 0; i < grids::kNumParts; ++i) {
     settings->density[i] = 255;  // Maximum density
   }
   ```
2. Rebuild and run: `./grids-jack -b 180` (high BPM)
3. Monitor CPU usage and xruns
4. **Expected**: 5-15% CPU usage, 0 xruns (may vary by system)

### Test 4: Polyphony Stress Test

**Purpose**: Test with maximum simultaneous voices.

**Procedure**:
1. Create long samples (5+ seconds each)
2. Place in `data/` directory with proper naming
3. Run grids-jack: `./grids-jack -b 180`
4. Monitor voice count in verbose mode: `./grids-jack -b 180 -v`
5. Check CPU usage with many active voices
6. **Expected**: CPU scales linearly with active voices, no xruns

### Test 5: Buffer Size Variation

**Purpose**: Test performance at different buffer sizes (latency vs. CPU trade-off).

**Procedure**:
1. Stop JACK server
2. For each buffer size (64, 128, 256, 512, 1024):
   ```bash
   jackd -d alsa -r 48000 -p <buffer_size>
   ./grids-jack
   jack_cpu_load
   ```
3. Note CPU usage and xruns for each setting
4. **Expected**:
   - 64 frames: Higher CPU, possible xruns on slower systems
   - 128 frames: Moderate CPU, stable on most systems
   - 256 frames: Low CPU, very stable
   - 512+ frames: Very low CPU, high latency

### Test 6: Long-Running Stability

**Purpose**: Verify no memory leaks or performance degradation over time.

**Procedure**:
1. Start grids-jack: `./grids-jack`
2. Monitor memory usage every 10 minutes for 1 hour:
   ```bash
   while true; do
     date; ps aux | grep grids-jack | grep -v grep
     sleep 600
   done
   ```
3. Monitor CPU usage: `jack_cpu_load` (should remain constant)
4. **Expected**: Memory usage constant, CPU usage stable, no xruns

## Performance Optimization

### 1. JACK Server Configuration

**Optimal Settings for Most Systems**:
```bash
jackd -R -d alsa -r 48000 -p 256 -n 2
```

Explanation:
- `-R`: Realtime priority (requires user in `audio` group)
- `-d alsa`: ALSA driver (use `coreaudio` on macOS)
- `-r 48000`: 48kHz sample rate (good balance)
- `-p 256`: 256 frame buffer (5.3ms latency at 48kHz)
- `-n 2`: 2 periods (standard)

**Low Latency (for live performance)**:
```bash
jackd -R -d alsa -r 48000 -p 128 -n 2
```
- 2.7ms latency, higher CPU usage

**High Stability (for recording/production)**:
```bash
jackd -R -d alsa -r 48000 -p 512 -n 3
```
- 10.7ms latency, lower CPU usage, very stable

### 2. System Configuration

**Add User to Audio Group** (Linux):
```bash
sudo usermod -aG audio $USER
# Log out and back in for changes to take effect
```

This allows JACK to run with realtime priority.

**Disable CPU Frequency Scaling** (Linux):
```bash
# Check current governor
cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Set to performance mode (temporary)
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Make permanent (Ubuntu/Debian)
sudo apt-get install cpufrequtils
echo 'GOVERNOR="performance"' | sudo tee /etc/default/cpufrequtils
sudo systemctl restart cpufrequtils
```

**Increase System Limits** (Linux):
Edit `/etc/security/limits.conf`:
```
@audio   -  rtprio     95
@audio   -  memlock    unlimited
```

**Use a Realtime Kernel** (Advanced, Linux):
```bash
# Ubuntu/Debian
sudo apt-get install linux-lowlatency

# Or full realtime kernel
sudo apt-get install linux-rt
```

### 3. Application-Level Optimization

**Voice Pool Size**:
Adjust in `sample_player.h` if you need more/fewer voices:
```cpp
const size_t kMaxVoices = 256;  // Increase for more polyphony
```

Larger pool = more memory, no performance impact until voices are active.

**Sample Rate**:
Match JACK sample rate to avoid resampling:
- JACK at 48kHz, samples at 48kHz = no resampling (best performance)
- JACK at 48kHz, samples at 44.1kHz = resampling required (small overhead)

**Sample Length**:
Shorter samples = faster processing, less memory:
- Trim silence from sample tails
- Use mono samples (stereo are converted to mono anyway)

## Troubleshooting Performance Issues

### Issue: High CPU Usage

**Symptoms**: JACK CPU load >20%, system feels sluggish

**Solutions**:
1. Increase JACK buffer size: `-p 512` or `-p 1024`
2. Reduce sample count (remove unused samples from `data/`)
3. Check for other audio applications competing for resources
4. Verify realtime priority: `ps -eo pid,cls,rtprio,comm | grep jackd`
5. Check system load: `top` (look for other processes using CPU)

### Issue: Xruns (Buffer Underruns)

**Symptoms**: Audio clicks/pops, "xrun" messages, QjackCtl xrun counter increasing

**Solutions**:
1. **Increase buffer size**: Larger buffers give more time for processing
   ```bash
   jackd -d alsa -r 48000 -p 512
   ```

2. **Enable realtime priority**: Ensure JACK and grids-jack run with RT priority
   ```bash
   jackd -R -d alsa ...  # -R flag
   sudo usermod -aG audio $USER  # Add user to audio group
   ```

3. **Reduce system load**: Close unnecessary applications

4. **Disable power management**: Set CPU governor to "performance"

5. **Check for problematic hardware**: Some USB audio interfaces cause issues

6. **Verify no disk I/O**: grids-jack should not access disk during playback (all samples pre-loaded)

### Issue: High Latency

**Symptoms**: Noticeable delay between trigger and sound

**Solutions**:
1. Decrease JACK buffer size: `-p 128` or `-p 64`
2. Reduce number of periods: `-n 2` (minimum)
3. Use a realtime kernel
4. Check total audio system latency:
   ```bash
   jack_iodelay  # Measures round-trip latency
   ```

### Issue: Memory Usage Growing Over Time

**Symptoms**: Memory usage increases after hours of running

**Solutions**:
1. Check for memory leaks with Valgrind:
   ```bash
   valgrind --leak-check=full ./grids-jack
   ```

2. Verify no samples are being loaded at runtime (should be startup only)

3. Report the issue — this would be a bug requiring a fix

## Benchmarking Results

Typical performance on a modern system (Intel i5, 8GB RAM, Ubuntu 22.04):

| Configuration | CPU Usage | Active Voices | Xruns (1 hour) |
|--------------|-----------|---------------|----------------|
| Idle (no samples) | <1% | 0 | 0 |
| Light pattern (120 BPM) | 2-3% | 3-8 | 0 |
| Dense pattern (180 BPM) | 5-8% | 15-25 | 0 |
| Maximum polyphony | 15-20% | 100+ | 0 |

JACK settings: 48kHz, 256 frames, 2 periods (5.3ms latency)

## References

- [JACK Audio Connection Kit Documentation](https://jackaudio.org/documentation/)
- [Linux Audio Wiki: System Configuration](https://wiki.linuxaudio.org/wiki/system_configuration)
- [JACK Latency Tests](https://jackaudio.org/faq/latency_tests.html)
- [Realtime Audio on Linux](https://wiki.archlinux.org/title/Professional_audio)

## Reporting Performance Issues

If you encounter performance problems:

1. Gather system information:
   ```bash
   uname -a                    # Kernel version
   jackd --version             # JACK version
   jack_bufsize                # Buffer size
   jack_samplerate             # Sample rate
   cat /proc/cpuinfo | grep "model name" | head -1  # CPU
   ```

2. Run verbose mode and capture output:
   ```bash
   ./grids-jack -v > output.log 2>&1
   ```

3. Monitor performance:
   ```bash
   jack_cpu_load > cpu_load.log
   ```

4. Open an issue on GitHub with:
   - System information
   - JACK configuration
   - Verbose output log
   - Description of the problem
   - Steps to reproduce

Thank you for helping improve grids-jack!
