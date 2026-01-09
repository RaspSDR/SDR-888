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
cmake .. -DOPT_BUILD_BLADERF_SOURCE=ON -DOPT_BUILD_LIMESDR_SOURCE=ON -DOPT_BUILD_SDRPLAY_SOURCE=ON -DOPT_BUILD_NEW_PORTAUDIO_SINK=ON -DOPT_BUILD_M17_DECODER=ON -DOPT_BUILD_PERSEUS_SOURCE=ON -DOPT_BUILD_RFNM_SOURCE=ON -DOPT_BUILD_FOBOSSDR_SOURCE=ON -DOPT_BUILD_HYDRASDR_SOURCE=ON
make VERBOSE=1 -j2

cd ..
sh make_debian_package.sh ./build 'libfftw3-dev, libglfw3-dev, libvolk-dev, librtaudio-dev, libzstd-dev'