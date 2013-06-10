/* Copyright (C) 2012-2013 G.P. Halkes
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
#include <errno.h>
#ifndef NO_XDG
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#endif

#include "config.h"
#include "util.h"

#ifdef NO_XDG
t3_bool t3_config_xdg_supported(void) {
	return t3_false;
}

char *t3_config_xdg_get_path(t3_config_xdg_dirs_t xdg_dir, const char *program_dir, size_t file_name_len) {
	(void) xdg_dir;
	(void) program_dir;
	(void) file_name_len;
	errno = EINVAL;
	return NULL;
}
FILE *t3_config_xdg_open_read(t3_config_xdg_dirs_t xdg_dir, const char *program_dir, const char *file_name) {
	(void) xdg_dir;
	(void) program_dir;
	(void) file_name;
	errno = EINVAL;
	return NULL;
}
t3_config_write_file_t *t3_config_xdg_open_write(t3_config_xdg_dirs_t xdg_dir, const char *program_dir,	const char *file_name) {
	(void) xdg_dir;
	(void) program_dir;
	(void) file_name;
	errno = EINVAL;
	return NULL;
}
FILE *t3_config_xdg_get_file(t3_config_write_file_t *file) {
	(void) file;
	errno = EINVAL;
	return NULL;
}
t3_bool t3_config_xdg_close_write(t3_config_write_file_t *file, t3_bool cancel_rename, t3_bool force) {
	return t3_config_close_write(file, cancel_rename, force);
}

t3_config_write_file_t *t3_config_open_write(const char *file_name) {
	(void) file_name;
	errno = EINVAL;
	return NULL;
}
FILE *t3_config_get_write_file(t3_config_write_file_t *file) {
	(void) file;
	errno = EINVAL;
	return NULL;
}
t3_bool t3_config_close_write(t3_config_write_file_t *file, t3_bool cancel_rename, t3_bool force) {
	(void) file;
	(void) cancel_rename;
	(void) force;
	errno = EINVAL;
	return t3_false;
}

#else

struct t3_config_write_file_t {
	FILE *file;
	char *pathname;
	t3_bool closed;
};

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

t3_bool t3_config_xdg_supported(void) {
	return t3_true;
}

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

char *t3_config_xdg_get_path(t3_config_xdg_dirs_t xdg_dir, const char *program_dir, size_t file_name_len) {
	const char *env = getenv(xdg_dirs[xdg_dir].env_name);
	char *pathname, *tmp;
	size_t extra_size;

	if (xdg_dir > sizeof(xdg_dirs) / sizeof(xdg_dirs[0])) {
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
	} else {
		errno = ENOENT;
		return NULL;
	}

	extra_size = file_name_len + 1;
	if (program_dir != NULL)
		extra_size += 1 + strlen(program_dir);

	if ((tmp = realloc(pathname, strlen(pathname) + extra_size + 1)) == NULL) {
		free(pathname);
		return NULL;
	}
	pathname = tmp;

	if (program_dir != NULL) {
		strcat(pathname, "/");
		strcat(pathname, program_dir);
	}

	return pathname;
}

FILE *t3_config_xdg_open_read(t3_config_xdg_dirs_t xdg_dir, const char *program_dir, const char *file_name) {
	char *pathname;
	FILE *result;

	if (strchr(file_name, '/') != NULL) {
		errno = EINVAL;
		return NULL;
	}

	if ((pathname = t3_config_xdg_get_path(xdg_dir, program_dir, strlen(file_name))) == NULL)
		return NULL;

	strcat(pathname, "/");
	strcat(pathname, file_name);

	result = fopen(pathname, "r");
	free(pathname);
	return result;
}

t3_config_write_file_t *t3_config_xdg_open_write(t3_config_xdg_dirs_t xdg_dir, const char *program_dir, const char *file_name) {
	t3_config_write_file_t *result;
	char *pathname;
	int fd;

	if (strchr(file_name, '/') != NULL) {
		errno = EINVAL;
		return NULL;
	}

	if ((pathname = t3_config_xdg_get_path(xdg_dir, program_dir, strlen(file_name) + 7)) == NULL)
		return NULL;

	if (!make_dirs(pathname)) {
		free(pathname);
		return NULL;
	}

	strcat(pathname, "/.");
	strcat(pathname, file_name);
	strcat(pathname, "XXXXXX");
	if ((fd = mkstemp(pathname)) < 0) {
		free(pathname);
		return NULL;
	}

	if ((result = malloc(sizeof(t3_config_write_file_t))) == NULL || (result->file = fdopen(fd, "w")) == NULL) {
		close(fd);
		unlink(pathname);
		free(pathname);
		return NULL;
	}
	result->pathname = pathname;
	result->closed = t3_false;

	return result;
}

FILE *t3_config_xdg_get_file(t3_config_write_file_t *file) {
	return t3_config_get_write_file(file);
}

t3_bool t3_config_xdg_close_write(t3_config_write_file_t *file, t3_bool cancel_rename, t3_bool force) {
	return t3_config_close_write(file, cancel_rename, force);
}



t3_config_write_file_t *t3_config_open_write(const char *file_name) {
	t3_config_write_file_t *result;
	char *dirsep;
	size_t length;
	char *pathname;
	int fd;

	if ((dirsep = strrchr(file_name, '/')) == NULL) {
		length = 0;
	} else {
		length = dirsep - file_name;
		if (length > 0)
			length--;
	}

	if ((pathname = malloc(strlen(file_name) + 1 + 7)) == NULL)
		return NULL;
	memcpy(pathname, file_name, length);
	pathname[length] = 0;

	if (length > 0 && !make_dirs(pathname)) {
		free(pathname);
		return NULL;
	}

	if (dirsep != NULL)
		strcat(pathname, "/");
	strcat(pathname, ".");
	strcat(pathname, dirsep == NULL ? file_name : dirsep + 1);
	strcat(pathname, "XXXXXX");
	if ((fd = mkstemp(pathname)) < 0) {
		free(pathname);
		return NULL;
	}

	if ((result = malloc(sizeof(t3_config_write_file_t))) == NULL || (result->file = fdopen(fd, "w")) == NULL) {
		close(fd);
		unlink(pathname);
		free(pathname);
		return NULL;
	}
	result->pathname = pathname;
	result->closed = t3_false;

	return result;
}

FILE *t3_config_get_write_file(t3_config_write_file_t *file) {
	return file->file;
}

t3_bool t3_config_close_write(t3_config_write_file_t *file, t3_bool cancel_rename, t3_bool force) {
	char *last_slash, *target_path;
	size_t file_name_len;
	int rename_result;

	if (cancel_rename) {
		if (!file->closed)
			fclose(file->file);
		unlink(file->pathname);
		free(file->pathname);
		free(file);
		return t3_true;
	}

	if (!file->closed) {
		/* Make sure the data has hit the disk. */
		fflush(file->file);
		fsync(fileno(file->file));
		fclose(file->file);
		file->closed = t3_true;
	}

	if ((target_path = _t3_config_strdup(file->pathname)) == NULL) {
		if (!force)
			return t3_false;
		unlink(file->pathname);
		free(file->pathname);
		free(file);
		return t3_false;
	}

	/* Create the target path by removing the leading . and trailing characters
	   from the file name.
	*/
	last_slash = strrchr(target_path, '/');
	if (last_slash != NULL) {
		file_name_len = strlen(target_path) - (last_slash - target_path) - 8;
		memmove(last_slash + 1, last_slash + 2, file_name_len);
		last_slash[file_name_len + 1] = 0;
	} else {
		file_name_len = strlen(target_path) - 7;
		memmove(target_path, target_path + 1, file_name_len);
		target_path[file_name_len] = 0;
	}

	rename_result = rename(file->pathname, target_path);
	free(target_path);

	if (rename_result == 0) {
		free(file->pathname);
		free(file);
		return t3_true;
	}

	if (!force)
		return t3_false;
	unlink(file->pathname);
	free(file->pathname);
	free(file);
	return t3_false;
}

#endif