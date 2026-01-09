#!/bin/bash
set -e
cd /root

# Install dependencies and tools
sed -i 's/main$/main contrib non-free non-free-firmware/' /etc/apt/sources.list
apt update
apt install -y build-essential cmake git libfftw3-dev libglfw3-dev libvolk2-dev libzstd-dev\
            librtaudio-dev p7zip-full wget portaudio19-dev libfdk-aac-dev \
            libcodec2-dev autoconf libtool xxd libspdlog-dev libcurl4-openssl-dev

cd SDRPlusPlus
mkdir build
cd build
cmake ..
make VERBOSE=1 -j2

cd ..
sh make_debian_package.sh ./build 'libfftw3-single3, libglfw3, libvolk2.5, librtaudio6, libzstd1, libfdk-aac2'