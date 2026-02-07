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

#ifndef SAMPLE_BANK_H_
#define SAMPLE_BANK_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace grids_jack {

// Represents a single audio sample loaded into memory
struct Sample {
    std::vector<float> data;  // Audio samples as floats (mono)
    uint32_t length;          // Sample length in frames
    uint8_t midi_note;        // MIDI note number (parsed from filename)
    std::string filename;     // Original filename (for logging)
    
    Sample() : length(0), midi_note(0) {}
};

// Manages loading and storage of all audio samples
class SampleBank {
public:
    SampleBank();
    ~SampleBank();
    
    // Load all WAV files from the specified directory
    // Returns true on success, false if no samples could be loaded
    bool LoadDirectory(const std::string& path, uint32_t target_sample_rate);
    
    // Get a sample by MIDI note number
    // Returns nullptr if note not found
    const Sample* GetSample(uint8_t midi_note) const;
    
    // Get list of all loaded MIDI note numbers
    std::vector<uint8_t> GetAllNotes() const;
    
    // Get total number of loaded samples
    size_t GetSampleCount() const { return samples_.size(); }
    
private:
    // Parse MIDI note number from filename (e.g., "60.1.1.1.0.wav" -> 60)
    // Returns true on success, false if parsing failed
    bool ParseMidiNote(const std::string& filename, uint8_t* out_note) const;
    
    // Load a single WAV file
    // Returns true on success, false on error
    bool LoadWavFile(const std::string& filepath, uint32_t target_sample_rate, Sample* out_sample);
    
    // Convert stereo to mono by averaging L+R channels
    void ConvertStereoToMono(const std::vector<float>& stereo, std::vector<float>* mono);
    
    // Resample audio using linear interpolation
    void ResampleLinear(const std::vector<float>& input, uint32_t input_rate,
                       std::vector<float>* output, uint32_t output_rate);
    
    // Storage for all loaded samples, keyed by MIDI note
    std::map<uint8_t, Sample> samples_;
};

}  // namespace grids_jack

#endif  // SAMPLE_BANK_H_
