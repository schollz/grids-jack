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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sample_bank.h"
#include "sample_player.h"

// Global flag for shutdown
static volatile bool g_should_exit = false;

// JACK client handle
static jack_client_t* g_jack_client = nullptr;

// Sample bank
static grids_jack::SampleBank g_sample_bank;

// Sample player
static grids_jack::SamplePlayer g_sample_player;

// JACK output ports
static jack_port_t* g_output_port_left = nullptr;
static jack_port_t* g_output_port_right = nullptr;

// Configuration
struct Config {
    const char* sample_directory;
    float bpm;
    const char* client_name;
    
    Config() : sample_directory("data"), bpm(120.0f), client_name("grids-jack") {}
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
    
    // Process audio through sample player (generates mono output)
    g_sample_player.Process(out_left, nframes);
    
    // Copy mono to both channels for stereo output
    memcpy(out_right, out_left, nframes * sizeof(float));
    
    return 0;
}

// JACK shutdown callback
void jack_shutdown_callback(void* arg) {
    (void)arg;
    fprintf(stderr, "JACK server shut down, exiting...\n");
    g_should_exit = true;
}

// Initialize JACK client (stub for Phase 1)
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
    
    // Get sample rate
    jack_nframes_t sample_rate = jack_get_sample_rate(g_jack_client);
    fprintf(stderr, "JACK sample rate: %u Hz\n", sample_rate);
    
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
    fprintf(stderr, "  -h           Show this help message\n");
}

// Parse command-line arguments
bool parse_args(int argc, char* argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "d:b:n:h")) != -1) {
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
    fprintf(stderr, "Phase 3: Voice Pool and Sample Player\n\n");
    
    // Parse command-line arguments
    if (!parse_args(argc, argv)) {
        return 1;
    }
    
    // Display configuration
    fprintf(stderr, "Configuration:\n");
    fprintf(stderr, "  Sample directory: %s\n", g_config.sample_directory);
    fprintf(stderr, "  BPM: %.1f\n", g_config.bpm);
    fprintf(stderr, "  JACK client name: %s\n\n", g_config.client_name);
    
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
    
    // Activate JACK client
    if (jack_activate(g_jack_client) != 0) {
        fprintf(stderr, "Failed to activate JACK client\n");
        cleanup_jack();
        return 1;
    }
    
    fprintf(stderr, "JACK client activated\n");
    
    fprintf(stderr, "\nPress Ctrl+C to exit\n\n");
    
    // Main loop - wait for shutdown signal
    while (!g_should_exit) {
        sleep(1);
    }
    
    // Cleanup
    fprintf(stderr, "Shutting down...\n");
    cleanup_jack();
    
    fprintf(stderr, "Goodbye!\n");
    return 0;
}
