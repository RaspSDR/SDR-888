#!/bin/sh
set -e

# ========================= Boilerplate =========================
BUILD_DIR=$1
BUNDLE=$2

source macos/bundle_utils.sh

# ========================= Prepare dotapp structure =========================

# Clear .app
rm -rf $BUNDLE

# Create .app structure
bundle_create_struct $BUNDLE

# Add resources
cp -R root/res/* $BUNDLE/Contents/Resources/

# Create the icon file
bundle_create_icns root/res/icons/sdr888.macos.png $BUNDLE/Contents/Resources/sdrpp

# Create the property list
bundle_create_plist sdrpp SDR-888 com.rx-888.sdr 1.2.1 sdrp sdrpp sdrpp $BUNDLE/Contents/Info.plist

# ========================= Install binaries =========================

# Core
bundle_install_binary $BUNDLE $BUNDLE/Contents/MacOS $BUILD_DIR/sdrpp 
bundle_install_binary $BUNDLE $BUNDLE/Contents/Frameworks $BUILD_DIR/core/libsdrpp_core.dylib

# Source modules
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/source_modules/sddc_source/sddc_source.dylib
bundle_install_binary $BUNDLE $BUNDLE/Contents/Frameworks $BUILD_DIR/source_modules/sddc_source/libsddc.dylib

bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/source_modules/file_source/file_source.dylib

# Sink modules
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/sink_modules/audio_sink/audio_sink.dylib

# Decoder modules
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/decoder_modules/radio/radio.dylib
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/decoder_modules/ft8_decoder/ft8_decoder.dylib

# Misc modules
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/misc_modules/recorder/recorder.dylib

# ========================= Finalize =========================

# Clean hidden attributes/metadata first
echo "Cleaning metadata from $BUNDLE..."
xattr -cr "$BUNDLE"

# Sign the app
bundle_sign $BUNDLE