// Integration test for velocity feature
// This test verifies that velocity actually affects the audio output volume

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include "sample_bank.h"
#include "sample_player.h"
#include "pattern_generator_wrapper.h"

// Calculate RMS (Root Mean Square) of an audio buffer
float calculate_rms(const float* buffer, uint32_t num_frames) {
  float sum_squares = 0.0f;
  for (uint32_t i = 0; i < num_frames; ++i) {
    sum_squares += buffer[i] * buffer[i];
  }
  return sqrtf(sum_squares / num_frames);
}

int main() {
  fprintf(stderr, "Velocity Integration Test\n");
  fprintf(stderr, "==========================\n\n");
  
  // Load real samples
  grids_jack::SampleBank sample_bank;
  const uint32_t sample_rate = 48000;
  
  if (!sample_bank.LoadDirectory("data", sample_rate)) {
    fprintf(stderr, "Error: Failed to load samples from data/\n");
    return 1;
  }
  
  std::vector<uint8_t> notes = sample_bank.GetAllNotes();
  fprintf(stderr, "Loaded %zu samples\n\n", notes.size());
  
  // Initialize sample player
  grids_jack::SamplePlayer player;
  player.Init(&sample_bank, sample_rate);
  
  // Initialize pattern generator at 120 BPM
  grids_jack::PatternGeneratorWrapper pattern_gen;
  const float bpm = 120.0f;
  pattern_gen.Init(&player, sample_rate, bpm);
  pattern_gen.AssignSamplesToParts(notes);
  
  const std::vector<grids_jack::SampleMapping>& mappings = 
      pattern_gen.GetSampleMappings();
  
  fprintf(stderr, "Selected %zu samples\n", mappings.size());
  for (size_t i = 0; i < mappings.size(); ++i) {
    fprintf(stderr, "  Sample %zu (note %u): first 16 velocity steps = ", 
            i, mappings[i].midi_note);
    for (int step = 0; step < 16; ++step) {
      fprintf(stderr, "%u", mappings[i].velocity_pattern[step]);
    }
    fprintf(stderr, "\n");
  }
  fprintf(stderr, "\n");
  
  // Process audio and track RMS levels for each trigger
  fprintf(stderr, "Processing and measuring trigger volumes...\n\n");
  
  const uint32_t block_size = 256;
  const uint32_t blocks_per_second = sample_rate / block_size;
  
  std::vector<float> rms_values;
  uint64_t last_trigger_count = 0;
  
  // Process for 4 seconds (8 beats at 120 BPM)
  for (uint32_t i = 0; i < blocks_per_second * 4; ++i) {
    // Process pattern generator
    pattern_gen.Process(block_size);
    
    // Process audio
    float output[block_size];
    player.Process(output, block_size);
    
    // Check if triggers occurred
    uint64_t current_triggers = player.GetTotalTriggersCount();
    if (current_triggers > last_trigger_count) {
      // Calculate RMS of this block
      float rms = calculate_rms(output, block_size);
      rms_values.push_back(rms);
      
      if (rms_values.size() <= 20) {  // Show first 20 triggers
        fprintf(stderr, "Trigger %zu: RMS = %.6f\n", rms_values.size(), rms);
      }
      
      last_trigger_count = current_triggers;
    }
  }
  
  fprintf(stderr, "\nTotal triggers: %zu\n\n", rms_values.size());
  
  if (rms_values.empty()) {
    fprintf(stderr, "ERROR: No triggers occurred!\n");
    return 1;
  }
  
  // Analyze RMS values to detect high vs low velocity
  // Find min and max RMS
  float min_rms = rms_values[0];
  float max_rms = rms_values[0];
  
  for (size_t i = 1; i < rms_values.size(); ++i) {
    if (rms_values[i] < min_rms) min_rms = rms_values[i];
    if (rms_values[i] > max_rms) max_rms = rms_values[i];
  }
  
  fprintf(stderr, "RMS Statistics:\n");
  fprintf(stderr, "  Min RMS: %.6f\n", min_rms);
  fprintf(stderr, "  Max RMS: %.6f\n", max_rms);
  fprintf(stderr, "  Ratio (max/min): %.2f\n", max_rms / min_rms);
  fprintf(stderr, "\n");
  
  // Expected ratio should be around 10.0 (1.0 / 0.1)
  // But in practice it might be less due to sample overlaps and mixing
  float expected_min_ratio = 2.0f;  // At least 2x difference
  float actual_ratio = max_rms / min_rms;
  
  if (actual_ratio < expected_min_ratio) {
    fprintf(stderr, "WARNING: RMS ratio (%.2f) is less than expected minimum (%.2f)\n",
            actual_ratio, expected_min_ratio);
    fprintf(stderr, "This might indicate velocity is not being applied correctly,\n");
    fprintf(stderr, "or all triggers happened at similar velocity levels.\n");
    return 1;
  }
  
  fprintf(stderr, "SUCCESS: Detected volume variation consistent with velocity feature!\n");
  fprintf(stderr, "High and low velocity levels are being applied correctly.\n");
  
  return 0;
}
