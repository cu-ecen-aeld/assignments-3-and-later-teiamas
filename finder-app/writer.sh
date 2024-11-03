#!/bin/bash

# Write a shell script finder-app/writer.sh as described below
# Accepts the following arguments: 
#    the first argument is a full path to a file (including filename) on the filesystem, referred to below as writefile; 
#    the second argument is a text string which will be written within this file, referred to below as writestr
# Exits with value 1 error and print statements if any of the arguments above were not specified
# Creates a new file with name and path writefile with content writestr, overwriting any existing file and creating the path 
#         if it doesn’t exist. Exits with value 1 and error print statement if the file could not be created.

# check params number
if [ "$#" -ne 2 ]; then
    echo "missing parameter\s"
    echo "usage ./finder.sh filesdir searchstr"
    exit 1
fi

writefile=$1
writestr=$2

# Create the directory path if it doesn't exist
mkdir -p "$(dirname "$writefile")"
if [ "$?" -ne 0 ]; then
   echo "Could not create the directory $(dirname "$writefile")"
   exit 1
fi

# write the string to file
echo $writestr > $writefile
if [ "$?" -ne 0 ]; then
   echo "Could not create or write on $writefile"
   exit 1
fi

echo "File $writefile created successfully with content: $writestr"