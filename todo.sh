#!/bin/sh
#
# This script will run grep searches and print out all of the places in the
# code that have a TODO or FIXME marker on them. The script will recursively
# search all non-svn files in the src directory. Any additional files that
# ought to be searched should be added to the FILES list below.

FILES="README"
SEARCH="`find src/ -not \( -name \*.svn\* \) -type f` $FILES"
for file in $SEARCH
do
        COUNT=`grep -c -E "TODO|FIXME" $file`
        if [ "$COUNT" == 0 ]
        then
                continue
        fi
        echo -e "\n\033[37;7m$file ($COUNT)\033[m"
        grep --color=always "FIXME" $file
        grep "TODO" $file
done

