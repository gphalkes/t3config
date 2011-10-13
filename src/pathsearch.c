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

static size_t locate_dirsep_reverse(const char *str, size_t pos) {
	pos++;
	do {
		pos--;
		if (is_dirsep(str[pos]))
			return pos + 1;
	} while (pos > 0);
	return 0;
}

static char *clean_name(const char *name) {
	size_t len, i;
	char *result = NULL;
	t3_bool last_was_dirsep = t3_true;

	len = strlen(name);

	/* Check if name ends in /.., /. or equals .. or . Note that it can't end in
	   /, because that is checked before calling this function. */
	if (name[len - 1] == '.' &&
			(len == 1 || is_dirsep(name[len - 2]) || (name[len - 2] == '.' &&
			(len == 2 || is_dirsep(name[len - 3])))))
		goto invalid_name;

	/* Check if the name starts with ../ or ./ */
	while (name[0] == '.') {
		if (is_dirsep(name[1])) {
			/* If name starts with ./, remove it. */
			name += 2;
			len -= 2;
		} else if (name[1] == '.' && is_dirsep(name[2])) {
			/* If name starts with ../, that is invalid. */
			goto invalid_name;
		} else {
			break;
		}
	}

	if ((result = _t3_config_strdup(name)) == NULL)
		return NULL;

	for (i = 0; i < len; ) {
		if (last_was_dirsep) {
			if (is_dirsep(result[i])) {
				/* Remove // from name. */
				/* Use len - i as length, to also copy the trailing nul byte. */
				memmove(result + i, result + i + 1, len - i);
				len--;
			} else if (result[i] == '.' && is_dirsep(result[i + 1])) {
				/* Remove /./ from name. */
				/* Use len - i - 1 as length, to also copy the trailing nul byte. */
				memmove(result + i, result + i + 2, len - i - 1);
				len -= 2;
			} else if (result[i] == '.' && result[i + 1] == '.' && is_dirsep(result[i + 2])) {
				/* Remove xxx/../ from name. */
				size_t delete_from;

				if (i == 0)
					goto invalid_name;

				delete_from = locate_dirsep_reverse(result, i - 2);
				/* Use len - i - 2 as length, to also copy the trailing nul byte. */
				memmove(result + delete_from, result + i + 3, len - i - 2);
				len -= i + 3 - delete_from;
				i = delete_from;
			} else {
				last_was_dirsep = t3_false;
				i++;
			}
		} else {
			last_was_dirsep = is_dirsep(result[i]);
			i++;
		}
	}

	return result;

invalid_name:
	free(result);
	errno = EINVAL;
	return NULL;
}

FILE *t3_config_open_from_path(const char **path, const char *name, int flags) {
	FILE *result = NULL;
	char *free_name = NULL;
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
		if ((name = free_name = clean_name(name)) == NULL)
			return NULL;
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
						goto return_result;
					search_from = colon + 1;
				} else {
					if ((result = try_open(search_from, strlen(search_from), name)) != NULL || errno != ENOENT)
						goto return_result;
					break;
				}
			}
		} else {
			if ((result = try_open(*path, strlen(*path), name)) != NULL || errno != ENOENT)
				goto return_result;
		}
	}

return_result:
	free(free_name);
	return result;
}
