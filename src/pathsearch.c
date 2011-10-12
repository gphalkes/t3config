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
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "config.h"

static FILE *try_open(const char *dir, size_t dir_len, const char *name) {
	char *file_name;
	FILE *result;
	size_t len;

	len = dir_len + strlen(name) + 2;
	if ((file_name = malloc(len)) == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	strncpy(file_name, dir, dir_len);
	file_name[dir_len] = 0;
	if (dir_len != 0)
		strcat(file_name, "/");
	strcat(file_name, name);

	result = fopen(file_name, "r");
	free(file_name);
	return result;
}

//FIXME: what errors should make the search stop, or should it always continue? Option?

//FIXME: disallow anything with /../ or ^../
//FIXME: expand ^~


FILE *t3_config_open_from_path(const char **path, const char *name, int flags) {
	FILE *result;

#ifdef _WIN32
	/* For Windows machines, anything starting with a drive letter, or a directory
	   separator is considered an absolute path. */
	if ((strchr("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ", name[0]) != NULL && name[1] == ':') ||
			strchr("/\\", name[0]) != NULL) {
#else
	if (name[0] == '/') {
#endif
		if (flags & T3_CONFIG_CLEAN_NAME) {
			errno = EINVAL;
			return NULL;
		}
		return try_open("", 0, name);
	}

	//FIXME: cleanse name if T3_CONFIG_CLEAN_NAME was set.

	errno = EINVAL;
	for (; *path != NULL; path++) {
		if (flags & T3_CONFIG_SPLIT_PATH) {
			const char *search_from, *colon;

			search_from = *path;
			while (1) {
#ifdef _WIN32
				colon = strchr(search_from, ';');
#else
				colon = strchr(search_from, ':');
#endif
				if (colon != NULL) {
					if ((result = try_open(search_from, search_from - colon, name)) != NULL || errno != ENOENT)
						return result;
					search_from = colon + 1;
				} else {
					if ((result = try_open(search_from, strlen(search_from), name)) != NULL || errno != ENOENT)
						return result;
					break;
				}
			}
		} else {
			if ((result = try_open(*path, strlen(*path), name)) != NULL || errno != ENOENT)
				return result;
		}
	}
	return NULL;
}
