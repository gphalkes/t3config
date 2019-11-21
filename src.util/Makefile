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

SOURCES.t3config_test := t3config_test.c

TARGETS := t3config_test
#================================================#
# NO RULES SHOULD BE DEFINED BEFORE THIS INCLUDE #
#================================================#
include ../../makesys/rules.mk
#================================================#
include ../../t3shared/rules-base.mk
LDFLAGS.t3config_test := $(T3LDFLAGS.t3config)
LDLIBS.t3config_test := -lt3config

CFLAGS += -I. -I.objects
CFLAGS += -DUSE_GETTEXT -DLOCALEDIR=\"locales\"

.objects/t3config_test.o: | lib

lib:
	@$(MAKE) -q -C ../src libt3config.la || $(MAKE) -C ../src libt3config.la

clang-format:
	clang-format -i *.c *.h

.PHONY: links clang-format
