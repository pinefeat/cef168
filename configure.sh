#!/bin/bash

# Usage: ./configure.sh <image-sensor1> <image-sensor2> <image-sensor3> ...
# If no arguments are specified, the list of options is taken from sensors.txt.

if [ "$#" -lt 1 ]; then
    # Define the options
    mapfile -t options < <(grep -vE '^\s*#|^\s*$' sensors.txt)

    # Display the menu
    PS3="Please select an image sensor (Q to quit): "
    select choice in "${options[@]}"; do
        if [[ "$REPLY" == "Q" || "$REPLY" == "q" ]]; then
            exit 0
        # Check if a valid option is selected
        elif [[ -n "$choice" ]]; then
            set -- "$choice"
            break
        else
            echo "Invalid option. Please try again."
        fi
    done
fi

cd "$(dirname "${BASH_SOURCE[0]}")"

source "download.sh"
source "vcm.sh"

# Check if at least one file is downloaded
if [ ${#downloaded_files[@]} -eq 0 ]; then
  exit 1
fi

declare -i vcm_node_replaced=1
declare -i vcm_node_added=1

vcm_replaced() {
    echo "$sensor: VCM Node replaced successfully."
    vcm_node_replaced=1
}

vcm_added() {
    echo "$sensor: VCM Node added successfully."
    vcm_node_added=1
}

modify_sensor_overlay() {
    vcm_node_replaced=0
    vcm_node_added=0
    iterate replace_existing_vcm_node                 # 1. Firstly try to replace the existing vcm node.
    if [[ $? -eq 0 ]]; then                           # 2. For existing vcm node:
        iterate remove_vcm_node_supply                #  remove power supply regulator.
        vcm_replaced                                  # Done
        return
    fi
    iterate add_new_vcm_node                          # 3. Secondly add a new vcm node.
    if [[ $? -eq 0 ]]; then                           # 4. For the new vcm node:
        iterate add_vcm_node_fragment                 #  add a new fragment
        if [[ $? -eq 0 ]]; then
            vcm_added                                 # Done
            return
        fi
    fi
}

check() {
    local file="$1"
    if [ -r "$file" ]; then
        return 0
    else
        return 1
    fi
}

# Create Makefile from template
cp "Makefile.template" "Makefile"

# Repeat for each command line argument
success=true
for sensor in "$@"; do
    unset downloaded_files
    declare -A downloaded_files

    echo "$sensor: Updating overlay files"
    iterate_with_includes check "$sensor-overlay.dts"
    modify_sensor_overlay

    # Continue if no vcm node added or replaced
    if (( !vcm_node_replaced && !vcm_node_added)); then
        echo "$sensor: VCM Node was not added or replaced." >&2
        success=false
        continue
    fi

    add_sensor_to_makefile "Makefile" "$sensor"
done

if [ "$success" = true ] ; then
    echo 'All overlays files were updated successfully'
    exit 0
else
    echo 'Some overlay files could not be updated. Please check the log above and fix the files manually.'
    exit 1
fi
