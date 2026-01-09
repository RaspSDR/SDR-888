#!/bin/bash
set -e
cd /root

# Install dependencies and tools
apt update
apt install -y build-essential cmake git libfftw3-dev libglfw3-dev libvolk-dev libzstd-dev \
            librtaudio-dev p7zip-full wget portaudio19-dev libfdk-aac-dev \
            libcodec2-dev autoconf libtool xxd libspdlog-dev libcurl4-openssl-dev

cd SDRPlusPlus
mkdir build
cd build
cmake ..
make VERBOSE=1 -j2

cd ..
sh make_debian_package.sh ./build 'libfftw3-dev, libglfw3-dev, libvolk-dev, librtaudio-dev, libzstd-dev'