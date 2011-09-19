static FILE *try_open(const char *dir, size_t dir_len, const char *name) {
	char *file_name;
	FILE *result;
	size_t len;

	len = dir_len + strlen(name) + 2;
	if ((file_name = malloc(len)) == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	strncpy(file_name, dir, dirlen);
	file_name[dir_len] = 0;
	if (dir_len != 0)
		strcat(file_name, "/");
	strcat(file_name, name);

	result = fopen(file_name, "r");
	free(file_name);
	return result;
}

//FIXME: check that ENOENT is in C standard
//FIXME: what errors should make the search stop, or should it always continue? Option?

#define T3_CONFIG_SPLIT_PATH (1<<0)

FILE *t3_config_open_from_path(const char **path, const char *name, int opts) {
	FILE *result;

	if (name[0] == '/')
		return try_open("", 0, name);

	for (; path != NULL; path++) {
		if (opts & T3_CONFIG_SPLIT_PATH) {
			const char *search_from, *colon;

			search_from = path;
			while (1) {
				colon = strchr(search_from, ':');
				if (colon != NULL)
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
			if ((result = try_open(path, strlen(path), name)) != NULL || errno != ENOENT)
				return result;
		}
	}
}
/*
union {
	struct {
		const char **path;
		int opts;
	} dflt;
	struct {
		FILE *(*open)(const char *name, void *data);
		void *data;
	} user;
} include_callback;
*/
