// Copyright 2024 grids-jack authors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef PATTERN_GENERATOR_WRAPPER_H_
#define PATTERN_GENERATOR_WRAPPER_H_

#include <stdint.h>
#include <vector>

#include "sample_player.h"
#include "grids/pattern_generator.h"

namespace grids_jack {

static const size_t kMaxPendingTriggers = 64;

struct PendingTrigger {
  uint8_t midi_note;
  float velocity;
  int32_t delay_frames;
  bool active;
};

// Drum part types from Grids
enum DrumPart {
  DRUM_PART_BD = 0,  // Bass Drum
  DRUM_PART_SD = 1,  // Snare Drum
  DRUM_PART_HH = 2,  // Hi-Hat
  DRUM_PART_COUNT = 3
};

// Mapping of a sample to a drum part
struct SampleMapping {
  uint8_t midi_note;
  DrumPart drum_part;
  uint8_t x;  // X position on Grids map for triggering (0-255)
  uint8_t y;  // Y position on Grids map for triggering (0-255)
  std::vector<uint8_t> velocity_pattern;  // Binary pattern: 0=low velocity, non-zero=high velocity
  uint8_t velocity_step;  // Current step in velocity pattern
  // LFO state for x/y drift
  float lfo_x_phase;  // Current LFO phase for x (radians)
  float lfo_y_phase;  // Current LFO phase for y (radians)
  float lfo_x_freq;   // LFO frequency for x (radians per frame)
  float lfo_y_freq;   // LFO frequency for y (radians per frame)
};

class PatternGeneratorWrapper {
 public:
  PatternGeneratorWrapper();
  ~PatternGeneratorWrapper();
  
  // Initialize with sample player, sample rate, and BPM
  void Init(SamplePlayer* sample_player, uint32_t sample_rate, float bpm);
  
  // Assign samples to drum parts with random X/Y positions
  // num_parts: how many random samples to select
  // num_velocity_steps: length of random velocity pattern per sample
  void AssignSamplesToParts(const std::vector<uint8_t>& midi_notes,
                            size_t num_parts, size_t num_velocity_steps);
  
  // Process audio block and generate triggers
  // This should be called from the JACK process callback
  void Process(uint32_t num_frames);
  
  // Set tempo (BPM)
  void SetTempo(float bpm);
  
  // Get current tempo
  float GetTempo() const { return bpm_; }
  
  // Set pattern parameters
  void SetPatternX(uint8_t x);
  void SetPatternY(uint8_t y);
  void SetRandomness(uint8_t randomness);
  
  // Get pattern parameters
  uint8_t GetPatternX() const;
  uint8_t GetPatternY() const;
  uint8_t GetRandomness() const;
  
  // Enable/disable LFO modulation of x/y positions
  void SetLfoEnabled(bool enabled) { lfo_enabled_ = enabled; }
  bool GetLfoEnabled() const { return lfo_enabled_; }

  // Set humanization amount (0.0 = none, 1.0 = max jitter of half a step)
  void SetHumanize(float amount);
  float GetHumanize() const { return humanize_amount_; }

  // Print the current pattern to stderr
  void PrintCurrentPattern();

  // Check if pattern changed (call from main thread, prints if changed)
  void PrintPendingPattern();

  // Get sample mappings (for diagnostic output)
  const std::vector<SampleMapping>& GetSampleMappings() const {
    return sample_mappings_;
  }

 private:
  SamplePlayer* sample_player_;
  uint32_t sample_rate_;
  float bpm_;
  bool lfo_enabled_;
  uint8_t num_steps_;  // Pattern length (1-32)

  // Timing state
  uint32_t frames_since_last_tick_;
  uint32_t frames_per_pulse_;

  // Sample mappings
  std::vector<SampleMapping> sample_mappings_;

  // Pattern change detection (set in audio thread, read in main thread)
  uint32_t prev_pattern_bits_[DRUM_PART_COUNT];
  uint32_t pending_pattern_bits_[DRUM_PART_COUNT];
  uint8_t pending_pattern_x_;
  uint8_t pending_pattern_y_;
  volatile bool pattern_changed_;

  // Compute pattern bitmasks for current x/y settings
  void ComputePatternBits(uint32_t bits[DRUM_PART_COUNT],
                          uint8_t x, uint8_t y) const;

  // Detect pattern change (realtime-safe, called from audio thread)
  void DetectPatternChange();

  // Print a pattern line to stderr
  void PrintPatternLine(const char* name, uint32_t bits) const;

  // Calculate frames per pulse based on BPM
  void UpdateFramesPerPulse();

  // Process triggers from pattern generator
  void ProcessTriggers(uint8_t state);

  // Evaluate velocity pattern for a sample at a specific step
  // Returns true for high velocity (1.0), false for low velocity (0.1)
  bool EvaluateVelocityPattern(const SampleMapping& mapping) const;

  // Humanization
  float humanize_amount_;
  uint32_t humanize_max_frames_;
  PendingTrigger pending_triggers_[kMaxPendingTriggers];
  uint32_t humanize_rng_state_;

  uint32_t HumanizeRand();
  void QueueHumanizedTrigger(uint8_t midi_note, float velocity);
  void ProcessPendingTriggers();
};

}  // namespace grids_jack

#endif  // PATTERN_GENERATOR_WRAPPER_H_
