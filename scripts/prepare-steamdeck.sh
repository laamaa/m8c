#!/bin/bash
# This script will configure your Steam Deck to allow you to compile the m8c application.  Steam Deck runs an Arch Linux.

# NOTE: To use this script you need to set a sudo password using the passwd command.

# Disable readonly file system (https://help.steampowered.com/en/faqs/view/671A-4453-E8D2-323C)
echo "Disabling Steam Deck readonly file system"
sudo steamos-readonly disable

# Initialize and populate the pacman keyring
echo "Initializing and populating pacman keyring"
sudo pacman-key --init
sudo pacman-key --populate archlinux

# Install libserialport
echo "Installing libSerialPort"
sudo pacman -S --noconfirm libserialport

# Install sdl2
echo "Installing SDL2"
sudo pacman -S --noconfirm sdl2

# Install glibc
echo "Installing glibC"
sudo pacman -S noconfirm glibc

# Add deck user to uucp group to allow access to m8 serial
echo "Enabling serial port access"
sudo gpasswd -a deck uucp


echo -e "\nPreparation complete.  Please compile m8c using 'make' and then REBOOT your Steam Deck to pick up new serial port permissions."
