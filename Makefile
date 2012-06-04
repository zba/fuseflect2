#
# fuseflect - A FUSE filesystem for local directory mirroring
#
# Copyright (c) 2007 Theodoros V. Kalamatianos <nyb@users.sourceforge.net>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as published by
# the Free Software Foundation.
#

prefix := /usr/local
bindir := $(prefix)/bin

DEBUG :=
CFLAGS := -O2 -Wall $(DEBUG)



# Yes, I am lazy...
VER := $(shell head -n 1 NEWS | cut -d : -f 1)



all: fuseflect

fuseflect: fuseflect.c NEWS
	$(CC) $(shell pkg-config fuse --cflags --libs) $(CFLAGS) -DVERSION=\"$(VER)\" $< -o $@

install: all
	install -D -m755 fuseflect $(bindir)/fuseflect

clean:
	rm -f *.o fuseflect
