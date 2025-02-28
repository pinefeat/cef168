#!/bin/bash

# Check if at least one argument is provided
if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <image-sensor>"
    exit 2
fi

# Determine Raspberry Pi Firmware version
upstream_version=$(zgrep "raspi-firmware" "/usr/share/doc/raspi-firmware/changelog.Debian.gz"  | head -1 | awk -F'[ ():-]' '{print $5}')
if [ -z "$upstream_version" ]; then
    echo "Raspberry Pi Firmware version is not determined" >&2
    exit 1
else
    echo "Raspberry Pi Firmware version: $upstream_version"
fi

# Determine Raspberry Pi Linux kernel Git hash
commit=$(wget --quiet -O - "https://raw.githubusercontent.com/raspberrypi/firmware/refs/tags/$upstream_version/extra/git_hash")
if [ -z "$commit" ]; then
    echo "Raspberry Pi Linux kernel Git hash is not determined" >&2
    exit 1
else
    echo "Raspberry Pi Linux kernel Git hash: $commit"
fi

# Download device tree overlay files
declare -A downloaded_files

download_with_includes() {
    local file="$1"

    # Check if the file has already been processed
    if [[ ${downloaded_files["$file"]+_} ]]; then
        echo "Cycle detected or file already processed: $file. Skipping."
        return
    fi

    # Download the file
    wget --no-verbose --backups=5 "https://raw.githubusercontent.com/raspberrypi/linux/$commit/arch/arm/boot/dts/overlays/$file" || { return; }

    # Mark the file as processed
    downloaded_files["$file"]=1

    # Look for #include statements in the file
    includes=($(grep -E '#include "[^"]+\.dtsi"' "$file" | sed -E 's/\s*#include "([^"]+\.dtsi)"/\1/'))

    # Download each included file
    for included_file in "${includes[@]}"; do
        download_with_includes "$included_file"
    done
}

# Repeat for each command line argument
for sensor in "$@"; do
    echo "Device tree overlay files for $sensor:"
    download_with_includes "$sensor-overlay.dts"
done
