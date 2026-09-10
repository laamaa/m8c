# Repository Guidelines

## Project Structure & Module Organization

This repository builds `m8c`, a C/SDL3 remote display client for the Dirtywave M8. Core application code lives in `src/`, with backend-specific transport and audio code in `src/backends/` and bitmap font data in `src/fonts/`. Packaging files are under `package/` (`macos`, `appimage`, `rpm`, and desktop metadata). Top-level build and environment files include `Makefile`, `CMakeLists.txt`, `flake.nix`, `shell.nix`, and `default.nix`. User-facing docs are in `README.md` and `AUDIOGUIDE.md`; shared runtime assets include `gamecontrollerdb.txt` and shader examples such as `shaders/example_plasma.metal`.

## Build, Test, and Development Commands

- `make`: builds the default `m8c` binary with SDL3 and libserialport.
- `make libusb`: builds with the libusb backend (`USE_LIBUSB=1`).
- `make rtmidi`: builds with the RtMidi backend (`USE_RTMIDI`).
- `make clean`: removes object files and the local `m8c` binary.
- `cmake -S . -B build && cmake --build build`: alternative CMake build path.
- `./m8c --list`: lists detected M8 devices for a quick runtime smoke check.
- `./m8c --dev /dev/cu.usbmodem...`: runs against a specific device.

Install dependencies via the platform instructions in `README.md` before building.

## Coding Style & Naming Conventions

Use C with SDL3 APIs and keep changes close to existing module boundaries. Follow the existing brace style: function braces on the same line, `switch` cases indented one level, and short explanatory comments where behavior is not obvious. Prefer `snake_case` for functions and variables, `UPPER_CASE` for macros/constants, and `_s` suffixes for struct typedefs already using that pattern. Keep includes local and minimal; prefer SDL allocation/logging helpers where surrounding code already uses them.

## Testing Guidelines

There is no checked-in automated test suite. Validate changes by building the affected backend (`make`, `make libusb`, or `make rtmidi`) and performing a focused smoke test with `./m8c --list` or a real/headless M8 connection when hardware behavior is touched. For CMake or packaging changes, verify the matching CMake/package build path.

## Commit & Pull Request Guidelines

Do not submit AI generated pull requests. Troubleshooting and showing example implementations is fine, but changes are
preferably done manually.
