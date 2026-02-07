// Test for pattern generator wrapper
// This test verifies the pattern generator can generate triggers without JACK

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include "sample_bank.h"
#include "sample_player.h"
#include "pattern_generator_wrapper.h"

int main() {
  fprintf(stderr, "Pattern Generator Wrapper Test\n");
  fprintf(stderr, "===============================\n\n");
  
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
  fprintf(stderr, "Sample player initialized\n\n");
  
  // Initialize pattern generator at 120 BPM
  grids_jack::PatternGeneratorWrapper pattern_gen;
  const float bpm = 120.0f;
  
  pattern_gen.Init(&player, sample_rate, bpm);
  fprintf(stderr, "Pattern generator initialized at %.1f BPM\n\n", bpm);
  
  // Assign samples to drum parts
  pattern_gen.AssignSamplesToParts(notes, 4, 32);
  
  fprintf(stderr, "\nProcessing pattern for 2 seconds...\n");
  fprintf(stderr, "Expected: triggers should occur at regular intervals based on Grids patterns\n\n");
  
  // Process 2 seconds of audio (at 120 BPM, we should get 4 beats)
  const uint32_t block_size = 256;
  const uint32_t blocks_per_second = sample_rate / block_size;
  uint64_t last_trigger_count = 0;
  
  for (uint32_t i = 0; i < blocks_per_second * 2; ++i) {
    // Process pattern generator
    pattern_gen.Process(block_size);
    
    // Process audio (this also advances voices)
    float output[block_size];
    player.Process(output, block_size);
    
    // Check if triggers occurred in this block
    uint64_t current_triggers = player.GetTotalTriggersCount();
    if (current_triggers > last_trigger_count) {
      float time_sec = (float)(i * block_size) / sample_rate;
      uint64_t new_triggers = current_triggers - last_trigger_count;
      fprintf(stderr, "Time: %.3fs - %llu new trigger(s), total: %llu, active voices: %u\n", 
              time_sec, new_triggers, current_triggers, player.GetActiveVoiceCount());
      last_trigger_count = current_triggers;
    }
  }
  
  fprintf(stderr, "\nTest complete!\n");
  fprintf(stderr, "Total triggers: %llu\n", player.GetTotalTriggersCount());
  fprintf(stderr, "Final active voices: %u\n", player.GetActiveVoiceCount());
  
  if (player.GetTotalTriggersCount() == 0) {
    fprintf(stderr, "\nWARNING: No triggers occurred! Pattern generator may not be working.\n");
    return 1;
  }
  
  fprintf(stderr, "\nSUCCESS: Pattern generator is generating triggers!\n");
  return 0;
}
