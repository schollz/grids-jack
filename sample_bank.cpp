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

#include "sample_bank.h"

#include <dirent.h>
#include <sndfile.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <cmath>

namespace grids_jack {

SampleBank::SampleBank() {}

SampleBank::~SampleBank() {}

bool SampleBank::LoadDirectory(const std::string& path, uint32_t target_sample_rate) {
    fprintf(stderr, "Loading samples from directory: %s\n", path.c_str());
    
    DIR* dir = opendir(path.c_str());
    if (dir == nullptr) {
        fprintf(stderr, "Error: Could not open directory: %s\n", path.c_str());
        return false;
    }
    
    int loaded_count = 0;
    int failed_count = 0;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // Skip directories and hidden files
        if (entry->d_type == DT_DIR || entry->d_name[0] == '.') {
            continue;
        }
        
        // Check if file ends with .wav
        const char* name = entry->d_name;
        size_t len = strlen(name);
        if (len < 4 || strcasecmp(name + len - 4, ".wav") != 0) {
            continue;
        }
        
        // Parse MIDI note from filename
        uint8_t midi_note;
        if (!ParseMidiNote(name, &midi_note)) {
            fprintf(stderr, "Warning: Could not parse MIDI note from filename: %s\n", name);
            failed_count++;
            continue;
        }
        
        // Build full path
        std::string filepath = path + "/" + name;
        
        // Load the WAV file
        Sample sample;
        sample.filename = name;
        sample.midi_note = midi_note;
        
        if (!LoadWavFile(filepath, target_sample_rate, &sample)) {
            fprintf(stderr, "Warning: Failed to load WAV file: %s\n", name);
            failed_count++;
            continue;
        }
        
        // Store in map (overwrites if MIDI note already exists)
        if (samples_.count(midi_note) > 0) {
            fprintf(stderr, "Warning: MIDI note %u already loaded, replacing with: %s\n",
                   midi_note, name);
        }
        
        samples_[midi_note] = sample;
        loaded_count++;
        
        fprintf(stderr, "Loaded sample: %s (MIDI note %u, %u frames)\n",
               name, midi_note, sample.length);
    }
    
    closedir(dir);
    
    fprintf(stderr, "Sample loading complete: %d loaded, %d failed\n",
           loaded_count, failed_count);
    
    return loaded_count > 0;
}

const Sample* SampleBank::GetSample(uint8_t midi_note) const {
    auto it = samples_.find(midi_note);
    if (it == samples_.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<uint8_t> SampleBank::GetAllNotes() const {
    std::vector<uint8_t> notes;
    notes.reserve(samples_.size());
    
    for (const auto& pair : samples_) {
        notes.push_back(pair.first);
    }
    
    return notes;
}

bool SampleBank::ParseMidiNote(const std::string& filename, uint8_t* out_note) const {
    // Expected format: "60.1.1.1.0.wav" where first field is MIDI note
    size_t dot_pos = filename.find('.');
    if (dot_pos == std::string::npos || dot_pos == 0) {
        return false;
    }
    
    std::string note_str = filename.substr(0, dot_pos);
    
    // Convert to integer
    char* endptr;
    long note = strtol(note_str.c_str(), &endptr, 10);
    
    // Check if conversion was successful and value is in valid MIDI range
    if (*endptr != '\0' || note < 0 || note > 127) {
        return false;
    }
    
    *out_note = static_cast<uint8_t>(note);
    return true;
}

bool SampleBank::LoadWavFile(const std::string& filepath, uint32_t target_sample_rate,
                             Sample* out_sample) {
    // Open WAV file with libsndfile
    SF_INFO sf_info;
    memset(&sf_info, 0, sizeof(sf_info));
    
    SNDFILE* sf = sf_open(filepath.c_str(), SFM_READ, &sf_info);
    if (sf == nullptr) {
        fprintf(stderr, "Error: Could not open audio file: %s (%s)\n",
               filepath.c_str(), sf_strerror(nullptr));
        return false;
    }
    
    // Check if we need to handle stereo or sample rate conversion
    bool is_stereo = (sf_info.channels == 2);
    bool needs_resample = (static_cast<uint32_t>(sf_info.samplerate) != target_sample_rate);
    
    // Read all samples as float
    std::vector<float> raw_data;
    sf_count_t total_frames = sf_info.frames;
    raw_data.resize(total_frames * sf_info.channels);
    
    sf_count_t read_count = sf_readf_float(sf, raw_data.data(), total_frames);
    if (read_count != total_frames) {
        fprintf(stderr, "Warning: Expected %lld frames, read %lld frames from: %s\n",
               (long long)total_frames, (long long)read_count, filepath.c_str());
        raw_data.resize(read_count * sf_info.channels);
        total_frames = read_count;
    }
    
    sf_close(sf);
    
    // Convert stereo to mono if needed
    std::vector<float> mono_data;
    if (is_stereo) {
        ConvertStereoToMono(raw_data, &mono_data);
    } else if (sf_info.channels == 1) {
        mono_data = raw_data;
    } else {
        fprintf(stderr, "Error: Unsupported channel count: %d\n", sf_info.channels);
        return false;
    }
    
    // Resample if needed
    if (needs_resample) {
        std::vector<float> resampled_data;
        ResampleLinear(mono_data, sf_info.samplerate, &resampled_data, target_sample_rate);
        out_sample->data = resampled_data;
        out_sample->length = resampled_data.size();
    } else {
        out_sample->data = mono_data;
        out_sample->length = mono_data.size();
    }
    
    return true;
}

void SampleBank::ConvertStereoToMono(const std::vector<float>& stereo,
                                    std::vector<float>* mono) {
    size_t frame_count = stereo.size() / 2;
    mono->resize(frame_count);
    
    for (size_t i = 0; i < frame_count; i++) {
        // Average left and right channels
        (*mono)[i] = (stereo[i * 2] + stereo[i * 2 + 1]) * 0.5f;
    }
}

void SampleBank::ResampleLinear(const std::vector<float>& input, uint32_t input_rate,
                               std::vector<float>* output, uint32_t output_rate) {
    if (input.empty() || input_rate == 0 || output_rate == 0) {
        output->clear();
        return;
    }
    
    // Calculate output length
    double ratio = static_cast<double>(output_rate) / static_cast<double>(input_rate);
    size_t output_length = static_cast<size_t>(std::ceil(input.size() * ratio));
    output->resize(output_length);
    
    // Linear interpolation
    for (size_t i = 0; i < output_length; i++) {
        double src_pos = i / ratio;
        size_t src_index = static_cast<size_t>(src_pos);
        double frac = src_pos - src_index;
        
        if (src_index + 1 < input.size()) {
            // Interpolate between two samples
            (*output)[i] = input[src_index] * (1.0 - frac) + input[src_index + 1] * frac;
        } else if (src_index < input.size()) {
            // Last sample, no interpolation
            (*output)[i] = input[src_index];
        } else {
            // Past end, use silence
            (*output)[i] = 0.0f;
        }
    }
}

}  // namespace grids_jack
