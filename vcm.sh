#!/bin/bash

# New vcm_node to replace the existing one
new_vcm_node=$(cat cef168.dtsi)

# Device tree fragment to set lens-focus property of the camera node
lens_focus_fragment=$(cat <<EOF
fragment@0 {
\ttarget = <&cam_node>;
\t__overlay__ {
\t\tlens-focus = <&vcm_node>;
\t};
};
EOF
)

# Snippets to set vcm node status
vcm_status=$(cat <<EOF
\tvcm = <&vcm_node>, "status",
\t      <0>, "=0";
EOF
)

vcm_status_ok=$(cat <<EOF
&vcm_node {
\tstatus = "okay";
};
EOF
)

# This will match any device of the form: <device_name>@<address> {
device_pattern="[ \t]*[a-z0-9_-]+@[a-f0-9]+[ \t]*{"

replace_existing_vcm_node() {
    # Check if the required argument is provided
    if [ "$#" -ne 1 ]; then
        return 2
    fi

    local dts_file="$1"
    temp_file=$(mktemp)

    # Use awk to replace the block
    awk -v pattern="vcm(_node)?:$device_pattern" -v new_block="$new_vcm_node" '
    BEGIN {flag=0}
    {
        if (tolower($0) ~ pattern) {
            flag = 1
            match($0, /^[ \t]*/)                       # Capture leading spaces
            indent = substr($0, RSTART, RLENGTH)       # Extract leading spaces
            str = substr($0, length(indent) + 1)
            match(str, /[^ :]+/)                       # Capture node name
            name = substr(str, RSTART, RLENGTH)        # Extract node name
            sub(/vcm_node/, name, new_block)           # Replace node name in the new block
            split(new_block, lines, "\n")
            for (i = 1; i <= length(lines); i++) {     # Print the new block with the same
                print indent lines[i]                  # indentation as in the old one
            }
        } else if (flag == 1 && $0 ~ /}/) {
            flag = 0                                   # Stop when end of block reached
        } else if (flag == 0) {
            print $0                                   # Print existing line
        }
    }
    ' "$dts_file" > "$temp_file"

    # Check if the replacement took place by comparing the files
    if ! cmp -s "$dts_file" "$temp_file"; then
        mv "$temp_file" "$dts_file" --update --backup=numbered --verbose
        return 0
    else
        rm -f "$temp_file"
        return 1
    fi
}

remove_vcm_node_supply() {
    # Check if the required argument is provided
    if [ "$#" -ne 1 ]; then
        return 2
    fi

    local dts_file="$1"
    temp_file=$(mktemp)

    # Use awk to add the block
    awk -v overrides="__overrides__[ \t]*{" \
        -v supply="<&vcm(_node)?>[ \t]*,[ \t]*\"[a-z0-9_]+-supply:[0-9]+=\"[ \t]*,[ \t]*<&[a-z0-9_]+>[ \t]*[,]?" '
    BEGIN {flag=0;bracket=0;count=0}
    {
        if (flag == 0 && tolower($0) ~ overrides) {
            flag = 1                                                        # We are inside __overrides__ block
            bracket = 1
        } else if (flag == 1 && $0 ~ /{/) {
            bracket += 1                                                    # Mind opening bracket
        } else if (flag == 1 && $0 ~ /}/) {
            bracket -= 1                                                    # Mind closing bracket
            if (bracket == 0) {
                flag = 2                                                    # End of override block
            }
        } else if (flag == 1 && bracket == 1) {
            if (match(tolower($0), supply)) {
                before = substr($0, 1, RSTART - 1)
                after = substr($0, RSTART + RLENGTH)
                $0 = before after                                          # Cut matching piece from the line
                if ($0 ~ /^[ \t]*$/ || $0 ~ /=[ \t]*;$/) {
                    next                                                   # Skip if line is empty or has no parameters
                } else if ($0 ~ /^[ \t]*;[ \t]*$/) {
                    sub(/[ \t]*,[ \t]*$/, ";", lastLine)                   # Replace end of statement in the previous one
                    next
                }
            }
        }
        if (count > 0)
          print lastLine
        lastLine = $0
        count++
    }
    END { print lastLine }
    ' "$dts_file" > "$temp_file"

    # Check if the replacement took place by comparing the files
    if ! cmp -s "$dts_file" "$temp_file"; then
        mv "$temp_file" "$dts_file" --update --backup=numbered --verbose
        return 0
    else
        rm -f "$temp_file"
        return 1
    fi
}

add_new_vcm_node() {
    # Check if the required argument is provided
    if [ "$#" -ne 1 ]; then
        return 2
    fi

    local dts_file="$1"
    temp_file=$(mktemp)

    # Use awk to add the block
    cam_node_pattern="(cam(_node)?|$sensor):$device_pattern"
    awk -v pattern="$cam_node_pattern" -v new_block="$new_vcm_node" '
    BEGIN {IGNORECASE=1;flag=0;bracket=0}
    {
        print $0                                            # Print existing line
        if (tolower($0) ~ pattern) {
            flag = 1                                        # camera node found
            bracket = 1
            match($0, /^[ \t]*/)                            # Capture leading spaces
            indent = substr($0, RSTART, RLENGTH)            # Extract leading spaces
        } else if (flag == 1 && $0 ~ /{/) {
            bracket += 1                                    # Mind opening bracket
        } else if (flag == 1 && $0 ~ /}/) {
            bracket -= 1                                    # Mind closing bracket
        }
        if (flag == 1 && bracket == 0) {
            flag = 0                                        # Start adding the new block right after the end of cam node
            print ""                                        # Print separator after cam node
            split(new_block, lines, "\n")
            for (i = 1; i <= length(lines); i++) {          # Print the new block with the same
                print indent lines[i]                       # indentation as in the old one
            }
        }
    }
    ' "$dts_file" > "$temp_file"

    # Check if the replacement took place by comparing the files
    if ! cmp -s "$dts_file" "$temp_file"; then
        cam_node_name=$(cat "$temp_file" | awk -v pattern="$cam_node_pattern" '
        {
            if (match(tolower($0), pattern)) {
                match($0, /^[ \t]*/)                        # Capture leading spaces
                indent = substr($0, RSTART, RLENGTH)        # Extract leading spaces
                str = substr($0, length(indent) + 1)
                match(str, /[^ :]+/)                        # Capture node name
                name = substr(str, RSTART, RLENGTH)         # Extract node name
                print name
            }
        }')

        mv "$temp_file" "$dts_file" --update --backup=numbered --verbose
        return 0
    else
        rm -f "$temp_file"
        return 1
    fi
}

add_vcm_node_fragment() {
    # Check if the required argument is provided
    if [ "$#" -ne 1 ]; then
        return 2
    fi

    local dts_file="$1"
    temp_file=$(mktemp)

    # Use awk to add the block
    awk -v pattern="fragment@[0-9]+[ \t]*{" \
        -v overrides="__overrides__[ \t]*{" \
        -v new_block="$lens_focus_fragment" \
        -v status_block="$vcm_status" \
        -v status_ok_block="$vcm_status_ok" \
        -v cam_node_name="$cam_node_name" '
    BEGIN {IGNORECASE=1;flag=0;bracket=0;count=0}
    {
        line = tolower($0)
        if (flag == 0 && line ~ pattern) {
            flag = 1
            bracket = 1
            match(line, /fragment@[0-9]+/)                            # Capture fragment index
            fragment = substr(line, RSTART + 9, RLENGTH) + 0          # Extract fragment index
            if (count == 0 && fragment != 0) {
                flag = 9                                              # Stop if zero-index fragment is absent
            } else if (fragment > count) {
                flag = 2                                              # Add fragment in the index gap
            }
            count += 1                                                # Count number of fragments
        } else if (flag == 1 && $0 ~ /{/) {
            bracket += 1                                              # Mind opening bracket
        } else if (flag == 1 && $0 ~ /}/) {
            bracket -= 1                                              # Mind closing bracket
            if (bracket == 0) {
                flag = 0
            }
        } else if (flag == 0 && count > 0 && $0 !~ /^[ \t]*$/) {
            flag = 2                                                  # Add fragment as last if no gaps found
            count += 1                                                # Bump count up as a new fragment index
        }
        if (flag == 2) {
            match($0, /^[ \t]*/)                                      # Capture leading spaces
            indent = substr($0, RSTART, RLENGTH)                      # Extract leading spaces
            if (length(cam_node_name) > 0) {
                sub(/cam_node/, cam_node_name, new_block)             # Replace node name in the new block
            }
            sub(/@0/, "@" count-1, new_block)                         # Replace fragment index in the new block
            split(new_block, lines, "\n")
            for (i = 1; i <= length(lines); i++) {                    # Print the new block with the same
                print indent lines[i]                                 # indentation as in the old one
            }
            print ""                                                  # Print separator between the next block
            flag = 3                                                  # Stop when one fragment is added
        }
        if (flag == 3 && line ~ overrides) {
            flag = 4                                                  # We are inside __overrides__ block
            bracket = 1
        } else if (flag == 4 && $0 ~ /{/) {
            bracket += 1                                              # Mind opening bracket
        } else if (flag == 4 && $0 ~ /}/) {
            bracket -= 1                                              # Mind closing bracket
            if (bracket == 0) {
                flag = 5                                              # End of override block, add status
            }
        }
        if (flag == 5) {
            match($0, /^[ \t]*/)                                      # Capture leading spaces
            indent = substr($0, RSTART, RLENGTH)                      # Extract leading spaces
            sub(/=0/, "=" count-1, status_block)                      # Replace fragment index in the status block
            split(status_block, lines, "\n")
            for (i = 1; i <= length(lines); i++) {                    # Print the new block with the same
                print indent lines[i]                                 # indentation as in the old one
            }
            flag = 6
        }
        print $0                                                      # Print existing line
    }
    END { if (flag == 6) { print ""; print status_ok_block } }
    ' "$dts_file" > "$temp_file"

    # Check if the replacement took place by comparing the files
    if ! cmp -s "$dts_file" "$temp_file"; then
        mv "$temp_file" "$dts_file" --update --backup=numbered --verbose
        return 0
    else
        rm -f "$temp_file"
        return 1
    fi
}
