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

#include <cmath>
#include <cstring>  // for memset

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace grids_jack {

SamplePlayer::SamplePlayer()
    : next_voice_index_(0),
      sample_bank_(nullptr),
      sample_rate_(0),
      active_voice_count_(0),
      total_triggers_(0) {
    // Initialize all voices as inactive
    for (auto& voice : voice_pool_) {
        voice.Reset();
    }
}

SamplePlayer::~SamplePlayer() {
    // Nothing to clean up - we don't own any allocated memory
}

void SamplePlayer::Init(const SampleBank* bank, uint32_t sample_rate) {
    sample_bank_ = bank;
    sample_rate_ = sample_rate;
    next_voice_index_ = 0;
    active_voice_count_ = 0;
    total_triggers_ = 0;
    
    // Reset all voices
    for (auto& voice : voice_pool_) {
        voice.Reset();
    }
}

void SamplePlayer::Trigger(uint8_t midi_note, float velocity, float pan) {
    // REALTIME-SAFE: No allocations, no locks, no system calls

    if (sample_bank_ == nullptr) {
        return;  // Not initialized
    }

    // Look up the sample
    const Sample* sample = sample_bank_->GetSample(midi_note);
    if (sample == nullptr || sample->data.empty()) {
        return;  // Sample not found or empty
    }

    // Clamp velocity to valid range
    if (velocity < 0.0f) velocity = 0.0f;
    if (velocity > 1.0f) velocity = 1.0f;

    // Compute equal-power pan gains
    // theta maps pan [-1,1] to [0, PI/2]
    float theta = (pan + 1.0f) * 0.25f * static_cast<float>(M_PI);
    float left_gain = cosf(theta);
    float right_gain = sinf(theta);

    // Find a voice slot (circular allocation with voice stealing)
    Voice& voice = voice_pool_[next_voice_index_];

    // Check if we're stealing an active voice (for statistics)
    bool was_active = voice.active;

    // Initialize the voice with the sample
    voice.Init(sample->data.data(), sample->length, velocity, left_gain, right_gain);
    
    // Update statistics (only increment if this wasn't already active)
    if (!was_active) {
        active_voice_count_++;
    }
    total_triggers_++;
    
    // Advance to next voice slot (circular)
    next_voice_index_ = (next_voice_index_ + 1) % kMaxVoices;
}

void SamplePlayer::Process(float* output, uint32_t num_frames) {
    // REALTIME-SAFE: No allocations, no locks, no system calls
    
    if (output == nullptr || num_frames == 0) {
        return;
    }
    
    // Clear output buffer
    memset(output, 0, num_frames * sizeof(float));
    
    // Reset active voice count
    active_voice_count_ = 0;
    
    // Mix all active voices
    for (auto& voice : voice_pool_) {
        if (!voice.active) {
            continue;  // Skip inactive voices
        }
        
        // Check if voice has finished
        if (voice.IsFinished()) {
            voice.Reset();
            continue;
        }
        
        // Count active voice
        active_voice_count_++;
        
        // Mix this voice into the output buffer
        uint32_t frames_to_render = num_frames;
        uint32_t frames_remaining = voice.sample_length - voice.position;
        
        // Don't read past the end of the sample
        if (frames_to_render > frames_remaining) {
            frames_to_render = frames_remaining;
        }
        
        // Add samples to output buffer
        for (uint32_t i = 0; i < frames_to_render; i++) {
            output[i] += voice.sample_data[voice.position + i] * voice.gain;
        }
        
        // Advance playback position
        voice.position += frames_to_render;
        
        // If voice finished during this buffer, mark as inactive
        if (voice.IsFinished()) {
            voice.Reset();
        }
    }
}

void SamplePlayer::ProcessStereo(float* left, float* right, uint32_t num_frames) {
    // REALTIME-SAFE: No allocations, no locks, no system calls

    if (left == nullptr || right == nullptr || num_frames == 0) {
        return;
    }

    // Clear output buffers
    memset(left, 0, num_frames * sizeof(float));
    memset(right, 0, num_frames * sizeof(float));

    // Reset active voice count
    active_voice_count_ = 0;

    // Mix all active voices with panning
    for (auto& voice : voice_pool_) {
        if (!voice.active) {
            continue;
        }

        if (voice.IsFinished()) {
            voice.Reset();
            continue;
        }

        active_voice_count_++;

        uint32_t frames_to_render = num_frames;
        uint32_t frames_remaining = voice.sample_length - voice.position;

        if (frames_to_render > frames_remaining) {
            frames_to_render = frames_remaining;
        }

        float gl = voice.gain * voice.pan_left;
        float gr = voice.gain * voice.pan_right;

        for (uint32_t i = 0; i < frames_to_render; i++) {
            float s = voice.sample_data[voice.position + i];
            left[i] += s * gl;
            right[i] += s * gr;
        }

        voice.position += frames_to_render;

        if (voice.IsFinished()) {
            voice.Reset();
        }
    }
}

}  // namespace grids_jack
