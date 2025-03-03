#!/bin/bash
set -xe

# Ubuntu 20.04

APP=m8c
VERSION=2.0.0

if [ "$1" == "build-sdl" ]; then

    ## Build SDL
    SDL_VERSION=3.2.6
    SDL_SHA256="096a0b843dd1124afda41c24bd05034af75af37e9a1b9d205cc0a70193b27e1a  SDL3-3.2.6.tar.gz"

    sudo apt-get install build-essential git make \
    pkg-config cmake ninja-build gnome-desktop-testing libasound2-dev libpulse-dev \
    libaudio-dev libjack-dev libsndio-dev libx11-dev libxext-dev \
    libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev \
    libxkbcommon-dev libdrm-dev libgbm-dev libgl1-mesa-dev libgles2-mesa-dev \
    libegl1-mesa-dev libdbus-1-dev libibus-1.0-dev libudev-dev libserialport-dev python2

    if [ ! -d SDL3-$SDL_VERSION ]; then

        curl -O https://www.libsdl.org/release/SDL3-$SDL_VERSION.tar.gz

        if ! echo $SDL_SHA256 | sha256sum -c --status -; then
            echo "SDL archive checksum failed" >&2
            exit 1
        fi

        tar zxvf SDL3-$SDL_VERSION.tar.gz
        pushd SDL3-$SDL_VERSION
        mkdir build_x86_64
        cmake -S . -B build_x86_64
        cmake --build build_x86_64
        sudo cmake --install build_x86_64
        popd

    fi

fi

if [ "$2" == "build-m8c" ]; then
make
fi

mkdir -p $APP.AppDir/usr/bin
cp m8c $APP.AppDir/usr/bin/
cp gamecontrollerdb.txt $APP.AppDir

mkdir -p $APP.AppDir/usr/share/applications/ $APP.AppDir/usr/share/metainfo/
cp package/appimage/m8c.desktop $APP.AppDir/usr/share/applications/
#appstreamcli seems to crash
#cp package/appimage/m8c.appdata.xml $APP.AppDir/usr/share/metainfo/
cp package/appimage/icon.svg $APP.AppDir

wget -c https://github.com/$(wget -q https://github.com/probonopd/go-appimage/releases/expanded_assets/continuous -O - | grep "appimagetool-.*-x86_64.AppImage" | head -n 1 | cut -d '"' -f 2)
chmod +x ./appimagetool-*.AppImage
./appimagetool-*.AppImage deploy ./$APP.AppDir/usr/share/applications/m8c.desktop
./appimagetool-*.AppImage ./$APP.AppDir


