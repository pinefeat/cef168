#!/bin/bash

# Define the options
options=("imx219" "imx258" "imx290" "imx296" "imx327" "imx378" "imx462" "imx477"
         "imx500" "imx519" "imx708" "ov5647" "ov64a40" "ov7251" "ov9281")

# Display the menu
echo "Please select an image sensor:"
select choice in "${options[@]}" "Quit"; do
    # Check if a valid option is selected
    if [[ -n "$choice" ]]; then
        if [[ "$choice" == "Quit" ]]; then
            exit 0
        else
            sensor="$choice"
            break
        fi
    else
        echo "Invalid option. Please try again."
    fi
done

source download.sh $sensor
source vcm.sh

# Check if at least one file is downloaded
if [ ${#downloaded_files[@]} -eq 0 ]; then
  exit 1
fi

declare -i vcm_node_replaced=1
declare -i vcm_node_added=1

iterate() {
    local func=$1      # The function name to be applied to each element
    shift              # Remove the first argument (function name)
    for file in "${!downloaded_files[@]}"; do
      $func "$file"
      if [[ $? -eq 0 ]]; then
        return 0
      fi
    done
    return 1
}

vcm_replaced() {
    echo "VCM Node replaced successfully."
    vcm_node_replaced=1
}

vcm_added() {
    echo "VCM Node added successfully."
    vcm_node_added=1
}

modify() {
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
        vcm_added                                     # Done
        return
    fi
}

modify

# Exit if no vcm node added or replaced
if (( !vcm_node_replaced && !vcm_node_added)); then
    echo "VCM Node was not added or replaced." >&2
    exit 1
fi

# Create Makefile from template
sed "s/sensor.dtbo/"$sensor".dtbo/" Makefile.template > Makefile
