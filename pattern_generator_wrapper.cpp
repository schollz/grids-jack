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
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "avrlib/random.h"

namespace grids_jack {

PatternGeneratorWrapper::PatternGeneratorWrapper()
    : sample_player_(nullptr),
      sample_rate_(0),
      bpm_(120.0f),
      lfo_enabled_(false),
      num_steps_(32),
      frames_since_last_tick_(0),
      frames_per_pulse_(0),
      pending_pattern_x_(0),
      pending_pattern_y_(0),
      pattern_changed_(false) {
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

  // Initialize pattern tracking with sentinel values so first check detects change
  for (int i = 0; i < DRUM_PART_COUNT; ++i) {
    prev_pattern_bits_[i] = 0xFFFFFFFF;
    pending_pattern_bits_[i] = 0;
  }
  pending_pattern_x_ = 0;
  pending_pattern_y_ = 0;
  pattern_changed_ = false;

  UpdateFramesPerPulse();
}

void PatternGeneratorWrapper::AssignSamplesToParts(
    const std::vector<uint8_t>& midi_notes,
    size_t num_parts, size_t num_velocity_steps) {
  sample_mappings_.clear();
  num_steps_ = num_velocity_steps > 32 ? 32 : static_cast<uint8_t>(num_velocity_steps);
  if (num_steps_ < 1) num_steps_ = 1;

  // Select num_parts random samples (or fewer if less available)
  size_t num_samples = midi_notes.size() < num_parts ? midi_notes.size() : num_parts;
  std::vector<uint8_t> selected_notes;

  // Create a shuffled copy of midi_notes
  std::vector<uint8_t> shuffled = midi_notes;
  for (size_t i = shuffled.size() - 1; i > 0; --i) {
    size_t j = rand() % (i + 1);
    std::swap(shuffled[i], shuffled[j]);
  }

  // Take first num_samples from shuffled list
  for (size_t i = 0; i < num_samples; ++i) {
    selected_notes.push_back(shuffled[i]);
  }

  for (size_t i = 0; i < selected_notes.size(); ++i) {
    SampleMapping mapping;
    mapping.midi_note = selected_notes[i];

    // Randomly assign to a drum part (BD, SD, or HH)
    mapping.drum_part = static_cast<DrumPart>(rand() % DRUM_PART_COUNT);

    // Assign random X/Y position on the Grids map for triggering
    mapping.x = static_cast<uint8_t>(rand() % 256);
    mapping.y = static_cast<uint8_t>(rand() % 256);

    // Generate a random velocity pattern (num_velocity_steps steps)
    // Each step is either 0 (low velocity) or 1 (high velocity)
    mapping.velocity_pattern.resize(num_velocity_steps);
    for (size_t step = 0; step < num_velocity_steps; ++step) {
      mapping.velocity_pattern[step] = static_cast<uint8_t>(rand() % 2);
    }

    // Initialize velocity step counter
    mapping.velocity_step = 0;

    // Initialize LFO parameters with random periods (60-120 seconds)
    float x_period = 60.0f + (float)(rand() % 6001) / 100.0f;
    float y_period = 60.0f + (float)(rand() % 6001) / 100.0f;
    mapping.lfo_x_freq = 2.0f * (float)M_PI / (x_period * sample_rate_);
    mapping.lfo_y_freq = 2.0f * (float)M_PI / (y_period * sample_rate_);
    mapping.lfo_x_phase = (float)(rand() % 10000) / 10000.0f * 2.0f * (float)M_PI;
    mapping.lfo_y_phase = (float)(rand() % 10000) / 10000.0f * 2.0f * (float)M_PI;

    sample_mappings_.push_back(mapping);
  }
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

      // Update LFO-modulated x/y before ticking
      if (lfo_enabled_ && !sample_mappings_.empty()) {
        float avg_x = 0, avg_y = 0;
        for (size_t j = 0; j < sample_mappings_.size(); ++j) {
          SampleMapping& m = sample_mappings_[j];
          m.lfo_x_phase += m.lfo_x_freq * frames_per_pulse_;
          m.lfo_y_phase += m.lfo_y_freq * frames_per_pulse_;
          while (m.lfo_x_phase > 2.0f * (float)M_PI)
            m.lfo_x_phase -= 2.0f * (float)M_PI;
          while (m.lfo_y_phase > 2.0f * (float)M_PI)
            m.lfo_y_phase -= 2.0f * (float)M_PI;
          m.x = static_cast<uint8_t>(127.5f + 127.5f * sinf(m.lfo_x_phase));
          m.y = static_cast<uint8_t>(127.5f + 127.5f * sinf(m.lfo_y_phase));
          avg_x += m.x;
          avg_y += m.y;
        }
        avg_x /= sample_mappings_.size();
        avg_y /= sample_mappings_.size();
        SetPatternX(static_cast<uint8_t>(avg_x));
        SetPatternY(static_cast<uint8_t>(avg_y));
        DetectPatternChange();
      }

      // Advance the pattern generator by 1 pulse
      grids::PatternGenerator::TickClock(1);

      // Wrap pattern at num_steps_
      if (num_steps_ < grids::kStepsPerPattern &&
          grids::PatternGenerator::step() >= num_steps_) {
        grids::PatternGenerator::set_step(0);
      }

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
          // Evaluate velocity pattern at current step for this sample
          bool high_velocity = EvaluateVelocityPattern(sample_mappings_[i]);
          float velocity = high_velocity ? 1.0f : 0.1f;
          
          // Trigger the sample with computed velocity
          sample_player_->Trigger(sample_mappings_[i].midi_note, velocity);
          
          // Step the velocity pattern forward (only when triggered)
          sample_mappings_[i].velocity_step =
              (sample_mappings_[i].velocity_step + 1) %
              sample_mappings_[i].velocity_pattern.size();
        }
      }
    }
  }
}

bool PatternGeneratorWrapper::EvaluateVelocityPattern(
    const SampleMapping& mapping) const {
  // Read the velocity pattern value at the current step
  // Returns true for high velocity (pattern value is non-zero), false for low
  return mapping.velocity_pattern[mapping.velocity_step] != 0;
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

void PatternGeneratorWrapper::ComputePatternBits(
    uint32_t bits[DRUM_PART_COUNT], uint8_t x, uint8_t y) const {
  grids::PatternGeneratorSettings* settings =
      grids::PatternGenerator::mutable_settings();
  for (int i = 0; i < DRUM_PART_COUNT; ++i) bits[i] = 0;
  for (uint8_t step = 0; step < num_steps_; ++step) {
    for (uint8_t inst = 0; inst < DRUM_PART_COUNT; ++inst) {
      uint8_t level = grids::PatternGenerator::GetDrumMapLevel(
          step, inst, x, y);
      uint8_t threshold = ~settings->density[inst];
      if (level > threshold) {
        bits[inst] |= (1u << step);
      }
    }
  }
}

void PatternGeneratorWrapper::PrintPatternLine(const char* name,
                                                uint32_t bits) const {
  fprintf(stderr, "  %s: ", name);
  for (uint8_t step = 0; step < num_steps_; ++step) {
    fprintf(stderr, "%c", (bits & (1u << step)) ? 'x' : '-');
  }
  fprintf(stderr, "\n");
}

// Realtime-safe: only compares and copies data, no allocation or I/O
void PatternGeneratorWrapper::DetectPatternChange() {
  grids::PatternGeneratorSettings* settings =
      grids::PatternGenerator::mutable_settings();
  uint8_t x = settings->options.drums.x;
  uint8_t y = settings->options.drums.y;

  uint32_t bits[DRUM_PART_COUNT];
  ComputePatternBits(bits, x, y);

  // Only check parts that have samples assigned
  bool changed = false;
  for (int i = 0; i < DRUM_PART_COUNT; ++i) {
    bool has_mapping = false;
    for (size_t j = 0; j < sample_mappings_.size(); ++j) {
      if (sample_mappings_[j].drum_part == static_cast<DrumPart>(i)) {
        has_mapping = true;
        break;
      }
    }
    if (has_mapping && bits[i] != prev_pattern_bits_[i]) {
      changed = true;
      break;
    }
  }

  if (changed) {
    for (int i = 0; i < DRUM_PART_COUNT; ++i) {
      prev_pattern_bits_[i] = bits[i];
      pending_pattern_bits_[i] = bits[i];
    }
    pending_pattern_x_ = x;
    pending_pattern_y_ = y;
    pattern_changed_ = true;
  }
}

// Called from main thread to print pending pattern changes
void PatternGeneratorWrapper::PrintPendingPattern() {
  if (!pattern_changed_) return;
  pattern_changed_ = false;

  const char* names[] = {"BD", "SD", "HH"};
  fprintf(stderr, "Pattern changed (x=%3u, y=%3u):\n",
          pending_pattern_x_, pending_pattern_y_);
  // Only print patterns for drum parts that have samples assigned
  for (int inst = 0; inst < DRUM_PART_COUNT; ++inst) {
    bool has_mapping = false;
    for (size_t j = 0; j < sample_mappings_.size(); ++j) {
      if (sample_mappings_[j].drum_part == static_cast<DrumPart>(inst)) {
        has_mapping = true;
        break;
      }
    }
    if (has_mapping) {
      PrintPatternLine(names[inst], pending_pattern_bits_[inst]);
    }
  }
}

// Print the current pattern (call from main thread, e.g. at startup)
void PatternGeneratorWrapper::PrintCurrentPattern() {
  grids::PatternGeneratorSettings* settings =
      grids::PatternGenerator::mutable_settings();
  uint8_t x = settings->options.drums.x;
  uint8_t y = settings->options.drums.y;

  uint32_t bits[DRUM_PART_COUNT];
  ComputePatternBits(bits, x, y);

  const char* names[] = {"BD", "SD", "HH"};
  fprintf(stderr, "Pattern (x=%3u, y=%3u):\n", x, y);
  // Only print patterns for drum parts that have samples assigned
  for (int inst = 0; inst < DRUM_PART_COUNT; ++inst) {
    bool has_mapping = false;
    for (size_t j = 0; j < sample_mappings_.size(); ++j) {
      if (sample_mappings_[j].drum_part == static_cast<DrumPart>(inst)) {
        has_mapping = true;
        break;
      }
    }
    if (has_mapping) {
      PrintPatternLine(names[inst], bits[inst]);
    }
  }

  // Update prev so DetectPatternChange knows the baseline
  for (int i = 0; i < DRUM_PART_COUNT; ++i) {
    prev_pattern_bits_[i] = bits[i];
  }
}

}  // namespace grids_jack
