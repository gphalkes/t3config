# Copyright (C) 2011 G.P. Halkes
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3, as
# published by the Free Software Foundation.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

SOURCES.test := test.c
CFLAGS.test := -I../../../src/ -I.objects
LDFLAGS.test := -L../../../src/.libs
LDLIBS.test := -lt3config

TARGETS := test

#================================================#
# NO RULES SHOULD BE DEFINED BEFORE THIS INCLUDE #
#================================================#
include ../../../../makesys/rules.mk
#================================================#

.objects/test.o: .objects/meta_schema.bytes

.objects/%.bytes: %.schema
	@[ -d .objects/`dirname '$<'` ] || mkdir -p .objects/`dirname '$<'`
	$(_VERBOSE_GEN) sed -r 's/^[[:space:]]+//;/^#/d;/^$$/d' $< | xxd -i > $@
