FROM mcr.microsoft.com/devcontainers/cpp
RUN apt-get update && apt-get install -y libsdl2-dev libserialport-dev
