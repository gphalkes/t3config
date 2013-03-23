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
#include "util.h"

/*FIXME: what errors should make the search stop, or should it always continue? Option?*/

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

static t3_bool is_dirsep(char c) {
	return c == '/'
#ifdef _WIN32
		|| c == '\\'
#endif
	;
}

#if 0
static size_t locate_dirsep_reverse(const char *str, size_t pos) {
	pos++;
	do {
		pos--;
		if (is_dirsep(str[pos]))
			return pos + 1;
	} while (pos > 0);
	return 0;
}
#endif

static t3_bool clean_name(const char *name) {
	size_t len;

	len = strlen(name);

	/* Check if name ends in /.., /. or equals .. or . Note that it can't end in
	   /, because that is checked before calling this function. */
	if (name[len - 1] == '.' &&
			(len == 1 || is_dirsep(name[len - 2]) || (name[len - 2] == '.' &&
			(len == 2 || is_dirsep(name[len - 3])))))
		return t3_false;

	/* Check for names starting with ../ */
	if (strncmp("../", name, 3) == 0)
		return t3_false;

	/* Check for names containing /../ */
	if (strstr(name, "/../") != NULL)
		return t3_false;

	return t3_true;
}

FILE *t3_config_open_from_path(const char **path, const char *name, int flags) {
	FILE *result = NULL;
	size_t len;

	if ((len = strlen(name)) == 0 || is_dirsep(name[len - 1])) {
		errno = EINVAL;
		return NULL;
	}

	if (is_dirsep(name[0])
#ifdef _WIN32
	||
	/* For Windows machines, anything starting with a drive letter, or a directory
	   separator is considered an absolute path. */
	(strchr("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ", name[0]) != NULL && name[1] == ':')
#endif
	) {
		if (flags & T3_CONFIG_CLEAN_NAME) {
			errno = EINVAL;
			return NULL;
		}
		return try_open("", 0, name);
	}

	if (flags & T3_CONFIG_CLEAN_NAME) {
		if (!clean_name(name)) {
			errno = EINVAL;
			return NULL;
		}
	}

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

	return result;
}
