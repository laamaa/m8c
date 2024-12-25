#include <stdint.h>
#include <m8emu.h>
#include <m8usb-host.h>
#include <m8audio.h>
#include <sdcard.h>
#include <config.h>
#include <thread>

using namespace m8;

std::string firmware_path;
std::string sdcard_path;
std::shared_ptr<M8Emulator> m8emu;
std::shared_ptr<M8AudioProcessor> m8audio;
std::shared_ptr<M8USBHost> host;
std::atomic<bool> running = false;
std::thread loop;

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
