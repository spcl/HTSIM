#!/bin/bash

# Path to the awk script
AWK_SCRIPT="extract_max_qs.awk"

# Find all .out files in subdirectories and process each one
find . -type f -name "*.out" | while read -r file; do
#    echo "Processing file: $file"
    awk -f "$AWK_SCRIPT" "$file"
done

