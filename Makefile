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
# Wrapper around distutils for gdb and people who can't read.

all:
	python setup.py build

dist:
	python setup.py bdist

clean:
	rm -rf build
distclean: clean
realclean: clean

install:
	python setup.py install

gendoc:
	python setup.py gendoc

check:

distcheck:

.PHONY: all dist clean distclean realclean install gendoc check distcheck

