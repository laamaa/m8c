#include <stdint.h>
#include <m8emu.h>
#include <m8usb-host.h>
#include <m8audio.h>
#include <sdcard.h>
#include <config.h>
#include <thread>
#include <SDL.h>

using namespace m8;

std::string firmware_path;
std::string sdcard_path;
std::shared_ptr<M8Emulator> m8emu;
std::shared_ptr<M8AudioProcessor> m8audio;
std::shared_ptr<M8USBHost> host;
std::atomic<bool> running = false;
std::thread loop;

static SDL_AudioDeviceID devid_out = 0;
static unsigned int audio_paused = 0;
static unsigned int audio_initialized = 0;

extern "C" int emu_init_config(const char *firmware, const char *sdcard) {
  firmware_path = firmware;
  sdcard_path = sdcard;
  return 1;
}

extern "C" int emu_init_serial(int verbose, const char *preferred_device) {
  if (m8emu) {
    return host->Initialized();
  } else {
    FirmwareConfig::GlobalConfig().LoadConfig({}, firmware_path);
    m8emu = std::make_shared<M8Emulator>();
    m8emu->LoadHEX(firmware_path);
    m8audio = std::make_shared<M8AudioProcessor>(*m8emu);
    m8audio->AttachUSBOutputCallback([](uint16_t* buffer, int length) {
      if (devid_out && audio_initialized && !audio_paused) {
        SDL_QueueAudio(devid_out, buffer, length*sizeof(*buffer));
      }
    });
    host = std::make_shared<M8USBHost>(m8emu->USBDevice());
    auto sd = std::make_shared<SDCard>(sdcard_path);
    m8emu->SDDevice().InsertSDCard(sd);
    m8emu->AttachInitializeCallback([&]() {
      m8audio->Setup();
      host->Setup();
    });
    running = true;
    loop = std::thread([]() {
      while (running) {
        m8emu->Run();
      }
    });
  }
  return 0;
}

extern "C" int emu_list_devices() {
  return 0;
}

extern "C" int emu_check_serial_port() {
  return 1;
}

extern "C" int emu_reset_display() {
  uint8_t buf[1] = {'R'};
  return host->SerialWrite(buf, 1) == 1;
}

extern "C" int emu_enable_and_reset_display() {
  uint8_t buf[1] = {'E'};
  return host->SerialWrite(buf, 1) == 1 && emu_reset_display();
}

extern "C" int emu_disconnect() {
  uint8_t buf[1] = {'D'};
  host->SerialWrite(buf, 1);
  running = false;
  loop.join();
  host = nullptr;
  m8audio = nullptr;
  m8emu = nullptr;
  return 1;
}

extern "C" int emu_serial_read(uint8_t *serial_buf, int count) {
  return host->SerialRead(serial_buf, count);
}

extern "C" int emu_send_msg_controller(uint8_t input) {
  uint8_t buf[2] = {'C', input};
  return host->SerialWrite(buf, 2) == 2;
}

extern "C" int emu_send_msg_keyjazz(uint8_t note, uint8_t velocity) {
  if (velocity > 0x7F)
    velocity = 0x7F;
  uint8_t buf[3] = {'K', note, velocity};
  return host->SerialWrite(buf, 3) == 3;
}


extern "C" int emu_audio_init(unsigned int audio_buffer_size, const char *output_device_name) {
  SDL_AudioSpec want_out, have_out;

  SDL_zero(want_out);
  want_out.freq = 44100;
  want_out.format = AUDIO_S16;
  want_out.channels = 2;
  want_out.samples = audio_buffer_size;
  devid_out = SDL_OpenAudioDevice(output_device_name, 0, &want_out, &have_out, SDL_AUDIO_ALLOW_ANY_CHANGE);

  if (devid_out == 0) {
    SDL_Log("Failed to open output: %s", SDL_GetError());
    return 0;
  }

  SDL_Log("Opening audio devices");
  SDL_PauseAudioDevice(devid_out, 0);
  audio_paused = 0;
  audio_initialized = 1;

  return 1;
}

extern "C" void emu_toggle_audio(unsigned int audio_buffer_size, const char *output_device_name) {
  if (!audio_initialized) {
    emu_audio_init(audio_buffer_size, output_device_name);
    return;
  }
  audio_paused = !audio_paused;
  SDL_PauseAudioDevice(devid_out, audio_paused);
  SDL_Log(audio_paused ? "Audio paused" : "Audio resumed");
}

extern "C" void emu_audio_destroy() {
  if (!audio_initialized)
    return;
  SDL_Log("Closing audio devices");
  SDL_PauseAudioDevice(devid_out, 1);
  SDL_CloseAudioDevice(devid_out);

  audio_initialized = 0;
}
