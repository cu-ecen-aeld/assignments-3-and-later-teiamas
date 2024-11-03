#!/bin/bash

# Write a shell script finder-app/finder.sh as described below:
# 
# Accepts the following runtime arguments: 
#    - the first argument is a path to a directory on the filesystem, referred to below as filesdir;
#    - the second argument is a text string which will be searched within these files, referred to below as searchstr
# Exits with return value 1 error and print statements if any of the parameters above were not specified
# Exits with return value 1 error and print statements if filesdir does not represent a directory on the filesystem
# Prints a message "The number of files are X and the number of matching lines are Y" where
#    - X is the number of files in the directory and all subdirectories and 
#    - Y is the number of matching lines found in respective files, where a matching line refers to a line which contains 
#        searchstr (and may also contain additional content).
#

# check params number
if [ "$#" -ne 2 ]; then
    echo "missing parameter\s"
    echo "usage ./finder.sh filesdir searchstr"
    exit 1
fi

# check if the first param is a existing directory
if [ ! -d $1 ]; then
   echo "$1 is not a directory"
   exit 1
fi
# assign parameters to script variables
filesdir=$1
searchstr=$2

# Find the number of files in the directory and subdirectories
num_files=$(find "$filesdir" -type f | wc -l)

# Find the number of matching lines in the files
num_matching_lines=$(grep -r "$searchstr" "$filesdir" | wc -l)

# Print the result
echo "The number of files are $num_files and the number of matching lines are $num_matching_lines."

   



