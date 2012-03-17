/* Copyright (C) 2012 G.P. Halkes
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
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "util.h"


typedef struct {
	const char *env_name;
	const char *homedir_relative;
} xdg_info_t;

static xdg_info_t xdg_dirs[] = {
	{ "XDG_CONFIG_HOME", ".config" },
	{ "XDG_DATA_HOME", ".local/share" },
	{ "XDG_CACHE_HOME", ".cache" },
	{ "XDG_RUNTIME_DIR", NULL }
};

static t3_bool make_dirs(char *dir) {
	char *slash = strchr(dir + (dir[0] == '/'), '/');

	while (slash != NULL) {
		*slash = 0;
		if (mkdir(dir, 0777) == -1 && errno != EEXIST)
			return t3_false;
		*slash = '/';
		slash = strchr(slash + 1, '/');
	}
	if (mkdir(dir, 0777) == -1 && errno != EEXIST)
		return t3_false;
	return t3_true;
}


FILE *t3_config_xdg_open(t3_config_xdg_dirs_t xdg_dir, const char *program_dir, const char *file_name, const char *mode) {
	const char *env = getenv(xdg_dirs[xdg_dir].env_name);
	char *pathname, *tmp;
	size_t extra_size;
	FILE *result;

	if (file_name == NULL || mode == NULL || xdg_dir > sizeof(xdg_dirs) / sizeof(xdg_dirs[0])) {
		errno = EINVAL;
		return NULL;
	}

	if (env != NULL && strlen(env) > 0) {
		if ((pathname = _t3_config_strdup(env)) == NULL)
			return NULL;
	} else if (xdg_dirs[xdg_dir].homedir_relative) {
		env = getenv("HOME");
		if (env != NULL && strlen(env) > 0) {
			if ((pathname = malloc(strlen(env) + 1 + strlen(xdg_dirs[xdg_dir].homedir_relative) + 1)) == NULL)
				return NULL;
			strcpy(pathname, env);
			strcat(pathname, "/");
			strcat(pathname, xdg_dirs[xdg_dir].homedir_relative);
		} else {
			errno = ENOENT;
			return NULL;
		}
	}

	extra_size = strlen(file_name) + 1;
	if (program_dir != NULL)
		extra_size += 1 + strlen(program_dir);

	if ((tmp = realloc(pathname, strlen(pathname) + extra_size + 1)) == NULL)
		goto return_error;

	if (program_dir != NULL) {
		strcat(pathname, "/");
		strcat(pathname, program_dir);
	}
	if (!make_dirs(pathname))
		goto return_error;

	strcat(pathname, "/");
	strcat(pathname, file_name);

	result = fopen(pathname, mode);
	free(pathname);
	return result;

return_error:
	free(pathname);
	return NULL;
}
