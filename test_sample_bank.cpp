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
#include <stdio.h>

int main(int argc, char* argv[]) {
    const char* sample_dir = (argc > 1) ? argv[1] : "data";
    uint32_t sample_rate = 48000;  // Typical JACK sample rate
    
    fprintf(stderr, "SampleBank Test Utility\n");
    fprintf(stderr, "=======================\n\n");
    
    grids_jack::SampleBank bank;
    
    fprintf(stderr, "Loading samples from: %s\n", sample_dir);
    fprintf(stderr, "Target sample rate: %u Hz\n\n", sample_rate);
    
    if (!bank.LoadDirectory(sample_dir, sample_rate)) {
        fprintf(stderr, "\nERROR: Failed to load samples\n");
        return 1;
    }
    
    fprintf(stderr, "\n");
    fprintf(stderr, "=================================\n");
    fprintf(stderr, "Sample Bank Summary\n");
    fprintf(stderr, "=================================\n");
    fprintf(stderr, "Total samples loaded: %zu\n\n", bank.GetSampleCount());
    
    std::vector<uint8_t> notes = bank.GetAllNotes();
    fprintf(stderr, "MIDI Notes: ");
    for (size_t i = 0; i < notes.size(); i++) {
        fprintf(stderr, "%u", notes[i]);
        if (i < notes.size() - 1) {
            fprintf(stderr, ", ");
        }
    }
    fprintf(stderr, "\n\n");
    
    // Test retrieving each sample
    fprintf(stderr, "Sample Details:\n");
    fprintf(stderr, "---------------\n");
    for (uint8_t note : notes) {
        const grids_jack::Sample* sample = bank.GetSample(note);
        if (sample != nullptr) {
            fprintf(stderr, "  MIDI %3u: %s (%u frames, %.2f seconds at %u Hz)\n",
                   note, sample->filename.c_str(), sample->length,
                   (float)sample->length / sample_rate, sample_rate);
        } else {
            fprintf(stderr, "  MIDI %3u: ERROR - sample not found\n", note);
        }
    }
    
    // Test invalid note lookup
    fprintf(stderr, "\n");
    fprintf(stderr, "Testing invalid note lookup (note 127)...\n");
    const grids_jack::Sample* invalid = bank.GetSample(127);
    if (invalid == nullptr) {
        fprintf(stderr, "  Correctly returned nullptr for non-existent note\n");
    } else {
        fprintf(stderr, "  ERROR: Should have returned nullptr\n");
    }
    
    fprintf(stderr, "\n");
    fprintf(stderr, "=================================\n");
    fprintf(stderr, "All tests passed!\n");
    fprintf(stderr, "=================================\n");
    
    return 0;
}
