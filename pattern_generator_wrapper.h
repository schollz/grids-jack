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
  uint8_t x;  // X position on Grids map (0-255)
  uint8_t y;  // Y position on Grids map (0-255)
};

class PatternGeneratorWrapper {
 public:
  PatternGeneratorWrapper();
  ~PatternGeneratorWrapper();
  
  // Initialize with sample player, sample rate, and BPM
  void Init(SamplePlayer* sample_player, uint32_t sample_rate, float bpm);
  
  // Assign samples to drum parts with random X/Y positions
  void AssignSamplesToParts(const std::vector<uint8_t>& midi_notes);
  
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
  
 private:
  SamplePlayer* sample_player_;
  uint32_t sample_rate_;
  float bpm_;
  
  // Timing state
  uint32_t frames_since_last_tick_;
  uint32_t frames_per_pulse_;
  
  // Sample mappings
  std::vector<SampleMapping> sample_mappings_;
  
  // Calculate frames per pulse based on BPM
  void UpdateFramesPerPulse();
  
  // Process triggers from pattern generator
  void ProcessTriggers(uint8_t state);
};

}  // namespace grids_jack

#endif  // PATTERN_GENERATOR_WRAPPER_H_
