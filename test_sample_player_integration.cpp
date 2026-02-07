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

#include "sample_player.h"
#include "sample_bank.h"
#include <stdio.h>
#include <cmath>

using namespace grids_jack;

// Integration test with real samples
bool TestIntegrationWithRealSamples() {
    fprintf(stderr, "\nIntegration Test: Real Samples\n");
    fprintf(stderr, "===============================\n");
    
    const char* sample_dir = "data";
    uint32_t sample_rate = 48000;
    
    // Load samples from directory
    SampleBank bank;
    if (!bank.LoadDirectory(sample_dir, sample_rate)) {
        fprintf(stderr, "  SKIP: No samples found in '%s' directory\n", sample_dir);
        fprintf(stderr, "  (This is expected when running without sample files)\n");
        return true;
    }
    
    std::vector<uint8_t> notes = bank.GetAllNotes();
    fprintf(stderr, "  Loaded %zu samples\n", notes.size());
    
    // Initialize sample player
    SamplePlayer player;
    player.Init(&bank, sample_rate);
    
    // Trigger multiple samples
    fprintf(stderr, "  Triggering samples: ");
    for (size_t i = 0; i < std::min(notes.size(), size_t(5)); i++) {
        player.Trigger(notes[i], 0.8f);
        fprintf(stderr, "%u ", notes[i]);
    }
    fprintf(stderr, "\n");
    
    // Process audio buffers
    const uint32_t buffer_size = 256;
    float output[buffer_size];
    
    fprintf(stderr, "  Processing audio buffers...\n");
    
    uint32_t active_count = 0;
    int buffers_processed = 0;
    
    // Process until all voices finish or max iterations
    while (buffers_processed < 1000) {
        player.Process(output, buffer_size);
        active_count = player.GetActiveVoiceCount();
        
        // Check if output is non-zero (audio is playing)
        bool has_audio = false;
        for (uint32_t i = 0; i < buffer_size; i++) {
            if (output[i] != 0.0f) {
                has_audio = true;
                break;
            }
        }
        
        if (buffers_processed < 5 && !has_audio) {
            fprintf(stderr, "  WARNING: Expected audio output in first few buffers\n");
        }
        
        buffers_processed++;
        
        // Stop when all voices have finished
        if (active_count == 0 && buffers_processed > 10) {
            break;
        }
    }
    
    fprintf(stderr, "  Processed %d buffers\n", buffers_processed);
    fprintf(stderr, "  Total triggers: %lu\n", (unsigned long)player.GetTotalTriggersCount());
    fprintf(stderr, "  Final active voices: %u\n", active_count);
    
    fprintf(stderr, "  PASS: Integration test completed\n");
    return true;
}

// Test polyphony - many simultaneous voices
bool TestHighPolyphony() {
    fprintf(stderr, "\nTest: High Polyphony\n");
    fprintf(stderr, "====================\n");
    
    const char* sample_dir = "data";
    uint32_t sample_rate = 48000;
    
    SampleBank bank;
    if (!bank.LoadDirectory(sample_dir, sample_rate)) {
        fprintf(stderr, "  SKIP: No samples found\n");
        return true;
    }
    
    std::vector<uint8_t> notes = bank.GetAllNotes();
    if (notes.empty()) {
        fprintf(stderr, "  SKIP: No samples loaded\n");
        return true;
    }
    
    SamplePlayer player;
    player.Init(&bank, sample_rate);
    
    // Trigger many voices
    const size_t num_triggers = 50;
    fprintf(stderr, "  Triggering %zu voices...\n", num_triggers);
    
    for (size_t i = 0; i < num_triggers; i++) {
        uint8_t note = notes[i % notes.size()];
        player.Trigger(note, 0.5f);
    }
    
    uint32_t peak_voices = player.GetActiveVoiceCount();
    fprintf(stderr, "  Active voices after triggers: %u\n", peak_voices);
    
    // Process a few buffers
    const uint32_t buffer_size = 256;
    float output[buffer_size];
    
    for (int i = 0; i < 10; i++) {
        player.Process(output, buffer_size);
        
        // Find peak amplitude
        float peak = 0.0f;
        for (uint32_t j = 0; j < buffer_size; j++) {
            float abs_val = fabsf(output[j]);
            if (abs_val > peak) {
                peak = abs_val;
            }
        }
        
        if (i == 0) {
            fprintf(stderr, "  Peak amplitude in first buffer: %.3f\n", peak);
        }
    }
    
    fprintf(stderr, "  PASS: High polyphony test completed\n");
    return true;
}

// Test voice stealing
bool TestVoiceStealingWithRealSamples() {
    fprintf(stderr, "\nTest: Voice Stealing (Real Samples)\n");
    fprintf(stderr, "====================================\n");
    
    const char* sample_dir = "data";
    uint32_t sample_rate = 48000;
    
    SampleBank bank;
    if (!bank.LoadDirectory(sample_dir, sample_rate)) {
        fprintf(stderr, "  SKIP: No samples found\n");
        return true;
    }
    
    std::vector<uint8_t> notes = bank.GetAllNotes();
    if (notes.empty()) {
        fprintf(stderr, "  SKIP: No samples loaded\n");
        return true;
    }
    
    SamplePlayer player;
    player.Init(&bank, sample_rate);
    
    // Trigger more voices than the pool can hold
    const size_t triggers = kMaxVoices + 50;
    fprintf(stderr, "  Triggering %zu voices (pool size: %zu)\n", triggers, kMaxVoices);
    
    for (size_t i = 0; i < triggers; i++) {
        uint8_t note = notes[i % notes.size()];
        player.Trigger(note, 0.5f);
    }
    
    fprintf(stderr, "  Total triggers: %lu\n", (unsigned long)player.GetTotalTriggersCount());
    fprintf(stderr, "  Active voices: %u (should be <= %zu)\n", 
            player.GetActiveVoiceCount(), kMaxVoices);
    
    if (player.GetActiveVoiceCount() > kMaxVoices) {
        fprintf(stderr, "  FAIL: Active voices exceeds pool size\n");
        return false;
    }
    
    fprintf(stderr, "  PASS: Voice stealing working correctly\n");
    return true;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    fprintf(stderr, "SamplePlayer Integration Test Suite\n");
    fprintf(stderr, "====================================\n\n");
    
    fprintf(stderr, "Testing sample player with real WAV samples from data/\n");
    
    int passed = 0;
    int failed = 0;
    
    // Run tests
    if (TestIntegrationWithRealSamples()) passed++; else failed++;
    if (TestHighPolyphony()) passed++; else failed++;
    if (TestVoiceStealingWithRealSamples()) passed++; else failed++;
    
    // Print summary
    fprintf(stderr, "\n");
    fprintf(stderr, "=================================\n");
    fprintf(stderr, "Test Summary\n");
    fprintf(stderr, "=================================\n");
    fprintf(stderr, "Tests passed: %d\n", passed);
    fprintf(stderr, "Tests failed: %d\n", failed);
    fprintf(stderr, "=================================\n");
    
    if (failed > 0) {
        fprintf(stderr, "FAILED: Some tests did not pass\n");
        return 1;
    }
    
    fprintf(stderr, "SUCCESS: All integration tests passed!\n");
    return 0;
}
