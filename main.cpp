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

#include <jack/jack.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sample_bank.h"
#include "sample_player.h"
#include "pattern_generator_wrapper.h"

// Global flag for shutdown
static volatile bool g_should_exit = false;

// JACK client handle
static jack_client_t* g_jack_client = nullptr;

// Sample bank
static grids_jack::SampleBank g_sample_bank;

// Sample player
static grids_jack::SamplePlayer g_sample_player;

// Pattern generator wrapper
static grids_jack::PatternGeneratorWrapper g_pattern_generator;

// JACK output ports
static jack_port_t* g_output_port_left = nullptr;
static jack_port_t* g_output_port_right = nullptr;

// Configuration
struct Config {
    const char* sample_directory;
    float bpm;
    const char* client_name;
    bool verbose;
    size_t num_parts;
    size_t num_velocity_steps;
    bool lfo_enabled;
    float output_gain;

    Config() : sample_directory("data"), bpm(120.0f), client_name("grids-jack"),
               verbose(false), num_parts(4), num_velocity_steps(32),
               lfo_enabled(false), output_gain(1.0f) {}
};

static Config g_config;

// Signal handler for graceful shutdown
void signal_handler(int sig) {
    (void)sig;
    fprintf(stderr, "\nReceived shutdown signal, exiting...\n");
    g_should_exit = true;
}

// JACK process callback
int jack_process_callback(jack_nframes_t nframes, void* arg) {
    (void)arg;
    
    // Get output port buffers
    float* out_left = (float*)jack_port_get_buffer(g_output_port_left, nframes);
    float* out_right = (float*)jack_port_get_buffer(g_output_port_right, nframes);
    
    if (out_left == nullptr || out_right == nullptr) {
        return 0;
    }
    
    // Process pattern generator to generate triggers
    g_pattern_generator.Process(nframes);
    
    // Process audio through sample player (generates mono output)
    g_sample_player.Process(out_left, nframes);

    // Apply global output gain
    if (g_config.output_gain != 1.0f) {
        for (jack_nframes_t i = 0; i < nframes; i++) {
            out_left[i] *= g_config.output_gain;
        }
    }

    // Duplicate mono to both channels for stereo output
    // Using memcpy as it's optimized and realtime-safe
    memcpy(out_right, out_left, nframes * sizeof(float));
    
    return 0;
}

// JACK shutdown callback
void jack_shutdown_callback(void* arg) {
    (void)arg;
    fprintf(stderr, "JACK server shut down, exiting...\n");
    g_should_exit = true;
}

// Initialize JACK client
bool init_jack() {
    jack_status_t status;
    
    // Open JACK client
    g_jack_client = jack_client_open(g_config.client_name, JackNullOption, &status);
    if (g_jack_client == nullptr) {
        fprintf(stderr, "Failed to open JACK client: status = 0x%x\n", status);
        if (status & JackServerFailed) {
            fprintf(stderr, "Unable to connect to JACK server\n");
        }
        return false;
    }
    
    if (status & JackNameNotUnique) {
        const char* actual_name = jack_get_client_name(g_jack_client);
        fprintf(stderr, "Unique name '%s' assigned\n", actual_name);
    }
    
    // Get and log sample rate and buffer size
    jack_nframes_t sample_rate = jack_get_sample_rate(g_jack_client);
    jack_nframes_t buffer_size = jack_get_buffer_size(g_jack_client);
    fprintf(stderr, "JACK sample rate: %u Hz\n", sample_rate);
    fprintf(stderr, "JACK buffer size: %u frames\n", buffer_size);
    
    if (g_config.verbose) {
        fprintf(stderr, "JACK buffer duration: %.2f ms\n", 
                (float)buffer_size / sample_rate * 1000.0f);
    }
    
    // Set process callback
    if (jack_set_process_callback(g_jack_client, jack_process_callback, nullptr) != 0) {
        fprintf(stderr, "Failed to set JACK process callback\n");
        return false;
    }
    
    // Register shutdown callback
    jack_on_shutdown(g_jack_client, jack_shutdown_callback, nullptr);
    
    // Register stereo output ports
    g_output_port_left = jack_port_register(g_jack_client, "output_L",
                                           JACK_DEFAULT_AUDIO_TYPE,
                                           JackPortIsOutput, 0);
    if (g_output_port_left == nullptr) {
        fprintf(stderr, "Failed to register left output port\n");
        return false;
    }
    
    g_output_port_right = jack_port_register(g_jack_client, "output_R",
                                            JACK_DEFAULT_AUDIO_TYPE,
                                            JackPortIsOutput, 0);
    if (g_output_port_right == nullptr) {
        fprintf(stderr, "Failed to register right output port\n");
        return false;
    }
    
    fprintf(stderr, "Registered stereo output ports\n");
    
    return true;
}

// Cleanup JACK client
void cleanup_jack() {
    if (g_jack_client != nullptr) {
        jack_client_close(g_jack_client);
        g_jack_client = nullptr;
    }
}

// Print usage information
void print_usage(const char* program_name) {
    fprintf(stderr, "Usage: %s [OPTIONS]\n", program_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -d <path>    Sample directory (default: data)\n");
    fprintf(stderr, "  -b <bpm>     Tempo in BPM (default: 120)\n");
    fprintf(stderr, "  -n <name>    JACK client name (default: grids-jack)\n");
    fprintf(stderr, "  -s <steps>   Velocity pattern steps per sample (default: 32)\n");
    fprintf(stderr, "  -p <parts>   Number of random samples to select (default: 4)\n");
    fprintf(stderr, "  -o <gain>    Global output volume scaling (default: 1.0)\n");
    fprintf(stderr, "  -l           Enable LFO drift of x/y pattern positions\n");
    fprintf(stderr, "  -v           Verbose mode - show detailed diagnostic information\n");
    fprintf(stderr, "  -h           Show this help message\n");
}

// Parse command-line arguments
bool parse_args(int argc, char* argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "d:b:n:s:p:o:lvh")) != -1) {
        switch (opt) {
            case 'd':
                g_config.sample_directory = optarg;
                break;
            case 'b':
                g_config.bpm = atof(optarg);
                if (g_config.bpm <= 0.0f || g_config.bpm > 300.0f) {
                    fprintf(stderr, "Error: BPM must be greater than 0 and at most 300\n");
                    return false;
                }
                break;
            case 'n':
                g_config.client_name = optarg;
                break;
            case 's': {
                int val = atoi(optarg);
                if (val <= 0) {
                    fprintf(stderr, "Error: Steps must be greater than 0\n");
                    return false;
                }
                g_config.num_velocity_steps = static_cast<size_t>(val);
                break;
            }
            case 'p': {
                int val = atoi(optarg);
                if (val <= 0) {
                    fprintf(stderr, "Error: Parts must be greater than 0\n");
                    return false;
                }
                g_config.num_parts = static_cast<size_t>(val);
                break;
            }
            case 'o':
                g_config.output_gain = atof(optarg);
                if (g_config.output_gain < 0.0f) {
                    fprintf(stderr, "Error: Output gain must be >= 0\n");
                    return false;
                }
                break;
            case 'l':
                g_config.lfo_enabled = true;
                break;
            case 'v':
                g_config.verbose = true;
                break;
            case 'h':
                print_usage(argv[0]);
                exit(0);
            default:
                print_usage(argv[0]);
                return false;
        }
    }
    return true;
}

int main(int argc, char* argv[]) {
    fprintf(stderr, "grids-jack: JACK audio client with Grids pattern generator\n");
    fprintf(stderr, "Version 1.0 - Phase 6: Polishing and Testing Complete\n\n");
    
    // Read environment variables first (CLI flags override these)
    const char* env_parts = getenv("PARTS");
    if (env_parts != nullptr) {
        int val = atoi(env_parts);
        if (val > 0) {
            g_config.num_parts = static_cast<size_t>(val);
        }
    }
    const char* env_steps = getenv("STEPS");
    if (env_steps != nullptr) {
        int val = atoi(env_steps);
        if (val > 0) {
            g_config.num_velocity_steps = static_cast<size_t>(val);
        }
    }
    const char* env_lfo = getenv("LFO");
    if (env_lfo != nullptr && atoi(env_lfo) == 1) {
        g_config.lfo_enabled = true;
    }
    const char* env_verbose = getenv("VERBOSE");
    if (env_verbose != nullptr && atoi(env_verbose) == 1) {
        g_config.verbose = true;
    }

    // Parse command-line arguments (overrides env vars)
    if (!parse_args(argc, argv)) {
        return 1;
    }

    // Display configuration
    fprintf(stderr, "Configuration:\n");
    fprintf(stderr, "  Sample directory: %s\n", g_config.sample_directory);
    fprintf(stderr, "  BPM: %.1f\n", g_config.bpm);
    fprintf(stderr, "  JACK client name: %s\n", g_config.client_name);
    fprintf(stderr, "  Random parts: %zu\n", g_config.num_parts);
    fprintf(stderr, "  Velocity steps: %zu\n", g_config.num_velocity_steps);
    fprintf(stderr, "  Output gain: %.2f\n", g_config.output_gain);
    fprintf(stderr, "  LFO drift: %s\n", g_config.lfo_enabled ? "enabled" : "disabled");
    fprintf(stderr, "  Verbose mode: %s\n\n", g_config.verbose ? "enabled" : "disabled");
    
    // Setup signal handlers for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize JACK client
    if (!init_jack()) {
        fprintf(stderr, "Failed to initialize JACK client\n");
        return 1;
    }
    
    fprintf(stderr, "JACK client initialized successfully\n");
    
    // Get JACK sample rate for sample loading
    jack_nframes_t sample_rate = jack_get_sample_rate(g_jack_client);
    
    // Load samples from directory
    fprintf(stderr, "\n");
    if (!g_sample_bank.LoadDirectory(g_config.sample_directory, sample_rate)) {
        fprintf(stderr, "Error: No samples could be loaded\n");
        cleanup_jack();
        return 1;
    }
    
    // Display loaded samples
    std::vector<uint8_t> notes = g_sample_bank.GetAllNotes();
    fprintf(stderr, "\nLoaded %zu samples with MIDI notes: ", notes.size());
    for (size_t i = 0; i < notes.size(); i++) {
        fprintf(stderr, "%u", notes[i]);
        if (i < notes.size() - 1) {
            fprintf(stderr, ", ");
        }
    }
    fprintf(stderr, "\n\n");
    
    // Initialize sample player
    g_sample_player.Init(&g_sample_bank, sample_rate);
    fprintf(stderr, "Sample player initialized with %zu voice pool\n", grids_jack::kMaxVoices);
    
    // Initialize pattern generator
    g_pattern_generator.Init(&g_sample_player, sample_rate, g_config.bpm);
    fprintf(stderr, "Pattern generator initialized at %.1f BPM\n", g_config.bpm);
    
    // Enable LFO if configured
    g_pattern_generator.SetLfoEnabled(g_config.lfo_enabled);

    // Assign samples to drum parts with random X/Y positions
    g_pattern_generator.AssignSamplesToParts(notes, g_config.num_parts,
                                               g_config.num_velocity_steps);
    const std::vector<grids_jack::SampleMapping>& mappings = 
        g_pattern_generator.GetSampleMappings();
    fprintf(stderr, "Selected and assigned %zu samples to drum parts (BD, SD, HH)\n", 
            mappings.size());
    
    // In verbose mode, show detailed assignments
    if (g_config.verbose) {
        fprintf(stderr, "Sample assignments (verbose):\n");
        for (size_t i = 0; i < mappings.size(); ++i) {
            const char* part_name = "BD";
            if (mappings[i].drum_part == grids_jack::DRUM_PART_SD) {
                part_name = "SD";
            } else if (mappings[i].drum_part == grids_jack::DRUM_PART_HH) {
                part_name = "HH";
            }
            fprintf(stderr, "  Note %3u -> %s (x=%3u, y=%3u) velocity pattern: ",
                    mappings[i].midi_note, part_name, 
                    mappings[i].x, mappings[i].y);
            // Show first 16 steps of velocity pattern (0=low, 1=high)
            for (int step = 0; step < 16; ++step) {
                fprintf(stderr, "%u", mappings[i].velocity_pattern[step]);
            }
            fprintf(stderr, "...\n");
        }
        fprintf(stderr, "Pattern parameters: X=%u, Y=%u, Randomness=%u\n",
                g_pattern_generator.GetPatternX(),
                g_pattern_generator.GetPatternY(),
                g_pattern_generator.GetRandomness());
    }

    // Print initial pattern
    g_pattern_generator.PrintCurrentPattern();
    fprintf(stderr, "\n");

    // Activate JACK client
    if (jack_activate(g_jack_client) != 0) {
        fprintf(stderr, "Failed to activate JACK client\n");
        cleanup_jack();
        return 1;
    }
    
    fprintf(stderr, "JACK client activated\n");
    
    // Auto-connect to system playback ports (optional)
    const char** ports = jack_get_ports(g_jack_client, nullptr, nullptr,
                                       JackPortIsPhysical | JackPortIsInput);
    if (ports != nullptr) {
        // Connect left channel
        if (ports[0] != nullptr) {
            int result = jack_connect(g_jack_client,
                                     jack_port_name(g_output_port_left),
                                     ports[0]);
            if (result == 0) {
                fprintf(stderr, "Auto-connected output_L to %s\n", ports[0]);
            } else if (result == EEXIST) {
                fprintf(stderr, "output_L already connected to %s\n", ports[0]);
            } else {
                fprintf(stderr, "Failed to auto-connect output_L to %s (error %d)\n", ports[0], result);
            }
        }
        // Connect right channel
        if (ports[1] != nullptr) {
            int result = jack_connect(g_jack_client,
                                     jack_port_name(g_output_port_right),
                                     ports[1]);
            if (result == 0) {
                fprintf(stderr, "Auto-connected output_R to %s\n", ports[1]);
            } else if (result == EEXIST) {
                fprintf(stderr, "output_R already connected to %s\n", ports[1]);
            } else {
                fprintf(stderr, "Failed to auto-connect output_R to %s (error %d)\n", ports[1], result);
            }
        }
        jack_free(ports);
    } else {
        fprintf(stderr, "No physical playback ports found - skipping auto-connection\n");
        fprintf(stderr, "You may need to manually connect ports using qjackctl or jack_connect\n");
    }
    
    fprintf(stderr, "\nPress Ctrl+C to exit\n\n");
    
    // Main loop - wait for shutdown signal, print pattern changes
    while (!g_should_exit) {
        g_pattern_generator.PrintPendingPattern();
        usleep(100000);  // 100ms
    }
    
    // Cleanup
    fprintf(stderr, "Shutting down...\n");
    cleanup_jack();
    
    fprintf(stderr, "Goodbye!\n");
    return 0;
}
