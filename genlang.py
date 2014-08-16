#!/usr/bin/python
#******************************************************************************#
# Plutocracy Translation - Copyright (C) 2008 - Andrei "Garoth" Thorp
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#******************************************************************************#
import re
import sys
import os
import copy

patternOne = re.compile(r'C_str\("(.*?)",.*?"(.*?)"\)', re.S)
patternTwo = re.compile(r'C_register_.*?\(.*?,.*?"(.*?)",.*?,.*?"(.*?)"\)', \
        re.S)

comments = []
visibleStrings = []

header = """/*****************************************************************\
*************\\
 Plutocracy Translation - Copyright (C) 2008 - YOUR NAME HERE

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\\****************************************************************************\
**/
"""

#------------------------------------------------
# Returns the inputList without duplicate entries
#------------------------------------------------
def removeDuplicates(inputList):
    workingList = copy.deepcopy(inputList)
    for entry in workingList: 
        if workingList.count(entry) > 1:
            workingList.remove(entry)
    return workingList

#----------------------
# Outputs the help menu
#----------------------
def printHelp():
        print "Usage: %s <directory> <output file>" % sys.argv[0]
        print "      <directory>: toplevel source directory with files to parse"
        print "      <output file>: Where to store the output"

#------------------------------------------------------------------------
# Generates a list of lines delimited by semi colons at the ends of lines
#------------------------------------------------------------------------
def getCLines(filereader):
        lines = []
        clines = []
        buildString = ""
        for line in filereader:
                lines.append(line.strip())
        
        for line in lines:
                if line.endswith(";"):
                        buildString += line
                        clines.append(buildString)
                        buildString = ""
                else:
                        buildString += line+"\n"
        
        return clines

#-------------------------------------------------------------------------
# Read's the file's lines, split by ; not by \n and apply the regex to it.
# Store the result in lists for processing and writing to file
#-------------------------------------------------------------------------
def matchAndStore(fileReader):
        matched = False
        cLines = getCLines(fileReader)

        for line in cLines:
                matchesOne = re.findall(patternOne, line)
                matchesTwo = re.findall(patternTwo, line)

                if matchesOne != []:
                        print "C_str Matches: ", matchesOne
                        matched = True
                        visibleStrings.append(matchesOne[0])

                if matchesTwo != []:
                        print "C_register_* Matches: ", matchesTwo
                        matched = True
                        comments.append(matchesTwo[0])

        if matched == True:
                print "Found in file %s" % fileReader.name
                print ""

#Some Basic usage handling
if len(sys.argv) != 3:
        printHelp()
        sys.exit(1)
if not os.path.isdir(os.path.join(os.getcwd(), sys.argv[1])):
        print "ERROR: input is not a directory!"
        sys.exit(1)

outputWriter = open(sys.argv[2], "w")

# Recurse through the giver directory structure and call matchAndStore
for root, dirs, files in os.walk(sys.argv[1]):
        if ".svn" not in root:
                for file in files:
                        if file.endswith(".c"):
                                matchAndStore(open(os.path.join(root, file)))

outputWriter.write(header)
visibleStrings.sort()
visibleStrings = removeDuplicates(visibleStrings)
comments.sort()
comments = removeDuplicates(comments)

#Print visible strings
for pair in visibleStrings:
        outputWriter.write('/* EN: "' + pair[1] + '" */' + os.linesep +
                           pair[0] + ' ""' + os.linesep * 2)

#Print comments
for pair in comments:
        outputWriter.write('/* EN: "' + pair[1] + '" */' + os.linesep +
                           pair[0] + '-comment ""' + os.linesep * 2)

print "Data written to output file: " + outputWriter.name

