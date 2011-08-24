/* Copyright (C) 2011 G.P. Halkes
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3, as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef T3_CONFIG_UTIL_H
#define T3_CONFIG_UTIL_H

#include "config_api.h"

#ifdef HAVE_STRDUP
#define _t3_config_strdup strdup
#else
T3_CONFIG_LOCAL char *_t3_config_strdup(const char *str);
#endif

T3_CONFIG_LOCAL void _t3_unescape(char *dest, const char *src);
T3_CONFIG_LOCAL double _t3_config_strtod(char *text);

#endif
