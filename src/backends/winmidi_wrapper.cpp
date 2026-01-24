// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT
// Windows native MIDI backend C++ wrapper implementation

#ifdef USE_WINMIDI

#include "winmidi_wrapper.h"

// Windows headers
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// C++/WinRT headers
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Midi.h>
#include <winrt/Windows.Storage.Streams.h>

#include <SDL3/SDL.h>

#include <string>
#include <vector>
#include <mutex>
#include <atomic>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Devices::Midi;
using namespace Windows::Storage::Streams;

// Structure to hold device information
struct MidiDeviceInfo {
    std::wstring id;
    std::wstring name;
};

// Global state
static std::vector<MidiDeviceInfo> g_input_devices;
static std::vector<MidiDeviceInfo> g_output_devices;
static MidiInPort g_midi_in{nullptr};
static MidiOutPort g_midi_out{nullptr};
static int g_opened_device_index = -1;
static std::wstring g_opened_device_id;
static winmidi_callback_fn g_callback = nullptr;
static void *g_callback_user_data = nullptr;
static winrt::event_token g_message_received_token;
static std::atomic<bool> g_initialized{false};
static std::mutex g_mutex;

// Helper to convert wide string to UTF-8
static std::string wstring_to_utf8(const std::wstring &wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &result[0], size_needed, nullptr, nullptr);
    return result;
}

// Helper to convert UTF-8 to wide string
static std::wstring utf8_to_wstring(const char *str) {
    if (!str || !*str) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
    std::wstring result(size_needed - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str, -1, &result[0], size_needed);
    return result;
}

// Enumerate MIDI devices
static void enumerate_devices() {
    std::lock_guard<std::mutex> lock(g_mutex);

    g_input_devices.clear();
    g_output_devices.clear();

    try {
        // Get input devices
        auto input_selector = MidiInPort::GetDeviceSelector();
        auto input_devices = DeviceInformation::FindAllAsync(input_selector).get();

        for (const auto &device : input_devices) {
            MidiDeviceInfo info;
            info.id = device.Id().c_str();
            info.name = device.Name().c_str();
            g_input_devices.push_back(info);
        }

        // Get output devices
        auto output_selector = MidiOutPort::GetDeviceSelector();
        auto output_devices = DeviceInformation::FindAllAsync(output_selector).get();

        for (const auto &device : output_devices) {
            MidiDeviceInfo info;
            info.id = device.Id().c_str();
            info.name = device.Name().c_str();
            g_output_devices.push_back(info);
        }
    } catch (const winrt::hresult_error &ex) {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to enumerate MIDI devices: %s",
                     wstring_to_utf8(ex.message().c_str()).c_str());
    }
}

// Message received callback handler
static void on_message_received(const MidiInPort &sender, const MidiMessageReceivedEventArgs &args) {
    (void)sender;

    if (!g_callback) return;

    try {
        auto message = args.Message();
        auto raw_data = message.RawData();

        if (raw_data.Length() == 0) return;

        // Read the data from the buffer
        auto reader = DataReader::FromBuffer(raw_data);
        std::vector<uint8_t> data(raw_data.Length());
        reader.ReadBytes(data);

        // Call the callback
        g_callback(data.data(), data.size(), g_callback_user_data);
    } catch (const winrt::hresult_error &ex) {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error in MIDI message callback: %s",
                     wstring_to_utf8(ex.message().c_str()).c_str());
    }
}

extern "C" {

int winmidi_init(void) {
    if (g_initialized.exchange(true)) {
        return 1; // Already initialized
    }

    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        SDL_Log("Windows MIDI subsystem initialized");
        return 1;
    } catch (const winrt::hresult_error &ex) {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to initialize WinRT: %s",
                     wstring_to_utf8(ex.message().c_str()).c_str());
        g_initialized = false;
        return 0;
    }
}

void winmidi_cleanup(void) {
    if (!g_initialized) return;

    winmidi_close();

    std::lock_guard<std::mutex> lock(g_mutex);
    g_input_devices.clear();
    g_output_devices.clear();

    // Note: We don't uninitialize WinRT apartment as it may be used elsewhere
    g_initialized = false;
    SDL_Log("Windows MIDI subsystem cleaned up");
}

int winmidi_list_devices(void) {
    enumerate_devices();

    std::lock_guard<std::mutex> lock(g_mutex);

    SDL_Log("Number of MIDI in ports: %zu", g_input_devices.size());
    for (size_t i = 0; i < g_input_devices.size(); i++) {
        SDL_Log("MIDI IN port %zu, name: %s", i, wstring_to_utf8(g_input_devices[i].name).c_str());
    }

    SDL_Log("Number of MIDI out ports: %zu", g_output_devices.size());
    for (size_t i = 0; i < g_output_devices.size(); i++) {
        SDL_Log("MIDI OUT port %zu, name: %s", i, wstring_to_utf8(g_output_devices[i].name).c_str());
    }

    return (int)g_input_devices.size();
}

int winmidi_detect_m8(int verbose, const char *preferred_device) {
    enumerate_devices();

    std::lock_guard<std::mutex> lock(g_mutex);

    std::wstring preferred;
    if (preferred_device) {
        preferred = utf8_to_wstring(preferred_device);
    }

    int m8_port = -1;

    if (verbose) {
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Number of MIDI in ports: %zu", g_input_devices.size());
    }

    for (size_t i = 0; i < g_input_devices.size(); i++) {
        const auto &device = g_input_devices[i];
        std::string name_utf8 = wstring_to_utf8(device.name);

        if (verbose) {
            SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "MIDI IN port %zu, name: %s", i, name_utf8.c_str());
        }

        // Check if device name starts with "M8"
        if (device.name.length() >= 2 && device.name.substr(0, 2) == L"M8") {
            m8_port = (int)i;
            if (verbose) {
                SDL_Log("Found M8 Input in MIDI port %zu", i);
            }

            // Check if this is the preferred device
            if (!preferred.empty() && device.name == preferred) {
                SDL_Log("Found preferred device, breaking");
                break;
            }
        }
    }

    return m8_port;
}

int winmidi_open(int device_index) {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (device_index < 0 || (size_t)device_index >= g_input_devices.size() ||
        (size_t)device_index >= g_output_devices.size()) {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Invalid device index: %d", device_index);
        return 0;
    }

    try {
        // Open input port
        auto input_id = winrt::hstring(g_input_devices[device_index].id);
        g_midi_in = MidiInPort::FromIdAsync(input_id).get();

        if (!g_midi_in) {
            SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to open MIDI input port");
            return 0;
        }

        // Open output port
        auto output_id = winrt::hstring(g_output_devices[device_index].id);
        auto out_device = MidiOutPort::FromIdAsync(output_id).get();
        g_midi_out = out_device.as<MidiOutPort>();

        if (!g_midi_out) {
            g_midi_in.Close();
            g_midi_in = nullptr;
            SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to open MIDI output port");
            return 0;
        }

        g_opened_device_index = device_index;
        g_opened_device_id = g_input_devices[device_index].id;

        // Register message received callback
        g_message_received_token = g_midi_in.MessageReceived(on_message_received);

        SDL_Log("Opened MIDI ports for device: %s",
                wstring_to_utf8(g_input_devices[device_index].name).c_str());

        return 1;
    } catch (const winrt::hresult_error &ex) {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to open MIDI ports: %s",
                     wstring_to_utf8(ex.message().c_str()).c_str());
        g_midi_in = nullptr;
        g_midi_out = nullptr;
        return 0;
    }
}

void winmidi_close(void) {
    std::lock_guard<std::mutex> lock(g_mutex);

    try {
        if (g_midi_in) {
            g_midi_in.MessageReceived(g_message_received_token);
            g_midi_in.Close();
            g_midi_in = nullptr;
        }

        if (g_midi_out) {
            g_midi_out.Close();
            g_midi_out = nullptr;
        }
    } catch (const winrt::hresult_error &ex) {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error closing MIDI ports: %s",
                     wstring_to_utf8(ex.message().c_str()).c_str());
    }

    g_opened_device_index = -1;
    g_opened_device_id.clear();
    g_callback = nullptr;
    g_callback_user_data = nullptr;

    SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "MIDI ports closed");
}

int winmidi_send_sysex(const uint8_t *data, size_t length) {
    if (!g_midi_out || !data || length == 0) {
        return 0;
    }

    try {
        // Create a buffer with the SysEx data using DataWriter
        auto writer = DataWriter();
        writer.WriteBytes(winrt::array_view<const uint8_t>(data, data + length));
        auto buffer = writer.DetachBuffer();

        // Create SysEx message and send
        auto message = MidiSystemExclusiveMessage(buffer);
        g_midi_out.SendMessage(message);

        return 1;
    } catch (const winrt::hresult_error &ex) {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to send SysEx: %s",
                     wstring_to_utf8(ex.message().c_str()).c_str());
        return 0;
    }
}

void winmidi_set_callback(winmidi_callback_fn callback, void *user_data) {
    g_callback = callback;
    g_callback_user_data = user_data;
}

int winmidi_device_exists(void) {
    if (g_opened_device_index < 0 || g_opened_device_id.empty()) {
        return 0;
    }

    enumerate_devices();

    std::lock_guard<std::mutex> lock(g_mutex);

    // Check if the device with the same ID still exists
    for (const auto &device : g_input_devices) {
        if (device.id == g_opened_device_id) {
            return 1;
        }
    }

    return 0;
}

} // extern "C"

#endif // USE_WINMIDI
