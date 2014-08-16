#!/usr/bin/python
#
################################################################################
# Plutocracy - Copyright (C) 2008 - Michael Levin
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
################################################################################
#
# This script scans all of the C source and header files for hard-to-find and
# common code-style mistakes. Run with no arguments from the root directory.

import glob, re;

# Track comments for all files
comments = []
comments_src = []

# Regex for matching the preamble
preamble_re = re.compile(" Plutocracy - Copyright \\(C\\) 2008 -.*\
\
 This program is free software; you can redistribute it and/or modify it under\
 the terms of the GNU General Public License as published by the Free Software\
 Foundation; either version 2, or \\(at your option\\) any later version.\
\
 This program is distributed in the hope that it will be useful, but WITHOUT\
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS\
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.\
")

# Scan all the source files
filenames = (glob.glob('src/*.c') + glob.glob('src/*.h') +
             glob.glob('src/*/*.c') + glob.glob('src/*/*.h'))
for filename in filenames:
        f = open(filename);
        lines = f.readlines();

        # Reset state for each file
        in_comment = False
        first_comment = True
        comment = ''
        line_no = 0;

        for line in lines:
                line_no += 1
                line = line.rstrip('\r\n')
                line_len = len(line)
                if (line_len < 1):
                        continue

                # Function to print found problems
                def error(string):
                        print '%s:%d: %s' % (filename, line_no, string)

                # Tabs used
                if (line.find('\t') >= 0):
                        error('Tabs used')

                # Trailing whitespace
                if (line.endswith(' ')):
                        error('Trailing whitespace')
                        line = line.rstrip(' ')

                # Line too long
                if (line_len > 80):
                        error('Line too long (%d chars)' % (line_len))

                # Ended a long comment
                if (in_comment and line.endswith('**/')):
                        in_comment = False
                        if (first_comment):
                                match = preamble_re.search(comment)
                                if (match == None):
                                        error('Invalid preamble')
                                first_comment = False
                                continue
                        try:
                                i = comments.index(comment)
                                error('Repeated comment from %s' %
                                      (comments_src[i]))
                        except:
                                comments += [comment]
                                comments_src += ['%s:%d' % (filename, line_no)]

                # If we are in a comment, add this line
                if (in_comment):
                        comment += line

                # Started a long comment
                if (line.startswith('/**')):
                        in_comment = True
                        comment = ''

        f.close();

