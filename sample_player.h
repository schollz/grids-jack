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

#ifndef SAMPLE_PLAYER_H_
#define SAMPLE_PLAYER_H_

#include <array>
#include <cstdint>

#include "sample_bank.h"

namespace grids_jack {

// Maximum number of simultaneously playing voices
constexpr size_t kMaxVoices = 256;

// Represents a single playing voice
struct Voice {
    const float* sample_data;  // Pointer to sample data (non-owning)
    uint32_t sample_length;    // Length of sample in frames
    uint32_t position;         // Current playback position in frames
    float gain;                // Volume (default 1.0)
    float pan_left;            // Left channel gain from panning
    float pan_right;           // Right channel gain from panning
    bool active;               // Whether this voice is currently playing

    Voice() : sample_data(nullptr), sample_length(0), position(0),
              gain(1.0f), pan_left(0.70710678f), pan_right(0.70710678f),
              active(false) {}

    // Reset voice to inactive state
    void Reset() {
        sample_data = nullptr;
        sample_length = 0;
        position = 0;
        gain = 1.0f;
        pan_left = 0.70710678f;
        pan_right = 0.70710678f;
        active = false;
    }

    // Initialize voice with sample data and pan gains
    void Init(const float* data, uint32_t length, float velocity,
              float left = 0.70710678f, float right = 0.70710678f) {
        sample_data = data;
        sample_length = length;
        position = 0;
        gain = velocity;
        pan_left = left;
        pan_right = right;
        active = true;
    }
    
    // Check if voice has finished playing
    bool IsFinished() const {
        return position >= sample_length;
    }
};

// Manages a pool of voices for playing samples with infinite polyphony
class SamplePlayer {
public:
    SamplePlayer();
    ~SamplePlayer();
    
    // Initialize the sample player with a sample bank
    // Must be called before Trigger() or Process()
    void Init(const SampleBank* bank, uint32_t sample_rate);
    
    // Trigger a sample to play
    // This is realtime-safe and can be called from the audio callback
    // velocity: 0.0 to 1.0, pan: -1.0 (left) to 1.0 (right)
    void Trigger(uint8_t midi_note, float velocity, float pan = 0.0f);

    // Process audio for one buffer (mono)
    // This is realtime-safe and should be called from the audio callback
    // Mixes all active voices into the output buffer
    void Process(float* output, uint32_t num_frames);

    // Process audio for one buffer (stereo with panning)
    // This is realtime-safe and should be called from the audio callback
    void ProcessStereo(float* left, float* right, uint32_t num_frames);
    
    // Get number of currently active voices
    uint32_t GetActiveVoiceCount() const { return active_voice_count_; }
    
    // Get total number of voices triggered (for statistics)
    uint64_t GetTotalTriggersCount() const { return total_triggers_; }
    
private:
    // Pre-allocated voice pool
    std::array<Voice, kMaxVoices> voice_pool_;
    
    // Next voice to allocate (circular)
    size_t next_voice_index_;
    
    // Pointer to sample bank (non-owning)
    const SampleBank* sample_bank_;
    
    // Sample rate
    uint32_t sample_rate_;
    
    // Number of active voices (for statistics)
    uint32_t active_voice_count_;
    
    // Total number of triggers (for statistics)
    uint64_t total_triggers_;
};

}  // namespace grids_jack

#endif  // SAMPLE_PLAYER_H_
