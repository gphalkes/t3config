# Copyright (C) 2010 G.P. Halkes
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3, as
# published by the Free Software Foundation.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

all: doxygen examples/example
.PHONY: all

T3NAME:=config
include ../../t3shared/doc-rules.mk

clean::
	rm -f examples.example

examples/example: examples/example.c
	$(CC) -o $@ $< -I../include -L../src/.libs/ -lt3config
