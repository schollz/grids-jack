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

#include "pattern_generator_wrapper.h"

#include <stdlib.h>
#include <time.h>
#include <stdio.h>

#include "avrlib/random.h"

namespace grids_jack {

PatternGeneratorWrapper::PatternGeneratorWrapper()
    : sample_player_(nullptr),
      sample_rate_(0),
      bpm_(120.0f),
      frames_since_last_tick_(0),
      frames_per_pulse_(0) {
}

PatternGeneratorWrapper::~PatternGeneratorWrapper() {
}

void PatternGeneratorWrapper::Init(SamplePlayer* sample_player,
                                   uint32_t sample_rate,
                                   float bpm) {
  sample_player_ = sample_player;
  sample_rate_ = sample_rate;
  bpm_ = bpm;
  frames_since_last_tick_ = 0;
  
  // Initialize the Grids pattern generator
  grids::PatternGenerator::Init();
  
  // Initialize random seed with current time
  srand(static_cast<unsigned int>(time(nullptr)));
  avrlib::Random::Seed(static_cast<uint32_t>(rand()));
  
  // Set default pattern parameters (center of map)
  grids::PatternGeneratorSettings* settings =
      grids::PatternGenerator::mutable_settings();
  settings->options.drums.x = 128;
  settings->options.drums.y = 128;
  settings->options.drums.randomness = 0;
  
  // Set all drum densities to middle value
  for (int i = 0; i < grids::kNumParts; ++i) {
    settings->density[i] = 128;
  }
  
  UpdateFramesPerPulse();
}

void PatternGeneratorWrapper::AssignSamplesToParts(
    const std::vector<uint8_t>& midi_notes) {
  sample_mappings_.clear();
  
  fprintf(stderr, "Assigning %zu samples to drum parts:\n", midi_notes.size());
  
  for (size_t i = 0; i < midi_notes.size(); ++i) {
    SampleMapping mapping;
    mapping.midi_note = midi_notes[i];
    
    // Randomly assign to a drum part (BD, SD, or HH)
    mapping.drum_part = static_cast<DrumPart>(rand() % DRUM_PART_COUNT);
    
    // Assign random X/Y position on the Grids map
    mapping.x = static_cast<uint8_t>(rand() % 256);
    mapping.y = static_cast<uint8_t>(rand() % 256);
    
    sample_mappings_.push_back(mapping);
    
    const char* part_name = "BD";
    if (mapping.drum_part == DRUM_PART_SD) {
      part_name = "SD";
    } else if (mapping.drum_part == DRUM_PART_HH) {
      part_name = "HH";
    }
    
    fprintf(stderr, "  Note %3u -> %s (x=%3u, y=%3u)\n",
            mapping.midi_note, part_name, mapping.x, mapping.y);
  }
  
  fprintf(stderr, "\n");
}

void PatternGeneratorWrapper::SetTempo(float bpm) {
  bpm_ = bpm;
  UpdateFramesPerPulse();
}

void PatternGeneratorWrapper::UpdateFramesPerPulse() {
  // Grids uses 24 PPQN (pulses per quarter note)
  // At 120 BPM: 1 beat = 0.5 seconds = sample_rate * 0.5 frames
  // 24 pulses per beat, so frames_per_pulse = (sample_rate * 60.0) / (bpm * 24)
  
  float pulses_per_second = (bpm_ * 24.0f) / 60.0f;
  frames_per_pulse_ = static_cast<uint32_t>(
      static_cast<float>(sample_rate_) / pulses_per_second);
}

void PatternGeneratorWrapper::Process(uint32_t num_frames) {
  if (sample_player_ == nullptr) {
    return;
  }
  
  for (uint32_t i = 0; i < num_frames; ++i) {
    frames_since_last_tick_++;
    
    // Check if it's time for the next pulse
    if (frames_since_last_tick_ >= frames_per_pulse_) {
      frames_since_last_tick_ -= frames_per_pulse_;
      
      // Advance the pattern generator by 1 pulse
      grids::PatternGenerator::TickClock(1);
      
      // Get the current state (trigger bits)
      uint8_t state = grids::PatternGenerator::state();
      
      // Process triggers
      ProcessTriggers(state);
      
      // Increment pulse duration counter (for gate timing)
      grids::PatternGenerator::IncrementPulseCounter();
    }
  }
}

void PatternGeneratorWrapper::ProcessTriggers(uint8_t state) {
  // Check each drum part trigger bit
  for (int part = 0; part < grids::kNumParts; ++part) {
    if (state & (1 << part)) {
      // This part has a trigger, find all samples assigned to it
      for (size_t i = 0; i < sample_mappings_.size(); ++i) {
        if (sample_mappings_[i].drum_part == static_cast<DrumPart>(part)) {
          // Trigger the sample with default velocity
          sample_player_->Trigger(sample_mappings_[i].midi_note, 1.0f);
        }
      }
    }
  }
}

void PatternGeneratorWrapper::SetPatternX(uint8_t x) {
  grids::PatternGeneratorSettings* settings =
      grids::PatternGenerator::mutable_settings();
  settings->options.drums.x = x;
}

void PatternGeneratorWrapper::SetPatternY(uint8_t y) {
  grids::PatternGeneratorSettings* settings =
      grids::PatternGenerator::mutable_settings();
  settings->options.drums.y = y;
}

void PatternGeneratorWrapper::SetRandomness(uint8_t randomness) {
  grids::PatternGeneratorSettings* settings =
      grids::PatternGenerator::mutable_settings();
  settings->options.drums.randomness = randomness;
}

uint8_t PatternGeneratorWrapper::GetPatternX() const {
  return grids::PatternGenerator::mutable_settings()->options.drums.x;
}

uint8_t PatternGeneratorWrapper::GetPatternY() const {
  return grids::PatternGenerator::mutable_settings()->options.drums.y;
}

uint8_t PatternGeneratorWrapper::GetRandomness() const {
  return grids::PatternGenerator::mutable_settings()->options.drums.randomness;
}

}  // namespace grids_jack
