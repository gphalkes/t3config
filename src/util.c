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
#include <stdlib.h>
#include <string.h>
#include "util.h"

#ifndef HAS_STRDUP
/** strdup implementation if none is provided by the environment. */
char *_t3_config_strdup(const char *str) {
	char *result;
	size_t len = strlen(str) + 1;

	if ((result = malloc(len)) == NULL)
		return NULL;
	memcpy(result, str, len);
	return result;
}
#endif

void _t3_unescape(char *dest, const char *src) {
	size_t i, j;

	for (i = 1, j = 0; !(src[i] == src[0] && src[i + 1] == 0); i++, j++) {
		dest[j] = src[i];
		/* Because the only quotes that can occur in the string itself
		   are doubled (checked by lexing), we don't have to check the
		   next character. */
		if (src[i] == src[0])
			i++;
	}
	dest[j] = 0;
}
