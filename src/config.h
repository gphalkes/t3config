/* Copyright (C) 2011-2012,2019 G.P. Halkes
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

#ifndef T3_CONFIG_H
#define T3_CONFIG_H

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <t3config/config_api.h>
#include <t3config/config_errors.h>

/** @defgroup t3config_other Functions, constants and enums.
    These are the functions and associated constants and enums for manipulating
    configuration data. All functions for retrieving data from a config structure
    in the libt3config library allow a @c NULL value to be passed as their first
    parameter. This allows one to simply pass the result from for example
    ::t3_config_get to ::t3_config_get_string, without checking the result of
    the former. If the string is not present, the result of the combined call
    will simply yield the default value (@c NULL).
*/
/** @addtogroup t3config_other */
/** @{ */

#ifdef __cplusplus
extern "C" {
#endif

/** The version of libt3config encoded as a single integer.

    The least significant 8 bits represent the patch level.
    The second 8 bits represent the minor version.
    The third 8 bits represent the major version.

    At runtime, the value of T3_CONFIG_VERSION can be retrieved by calling ::t3_config_get_version.

    @internal
    The value 0 is an invalid value which should be replaced by the script
    that builds the release package.
*/
#define T3_CONFIG_VERSION 0

/** Types of values that can be stored in a config. */
typedef enum {
  T3_CONFIG_NONE,    /**< Only used for uninitialized values, or as error value. */
  T3_CONFIG_BOOL,    /**< Boolean value. */
  T3_CONFIG_INT,     /**< Integer value, at least 32 bits wide. */
  T3_CONFIG_STRING,  /**< String value. */
  T3_CONFIG_NUMBER,  /**< Floating point value (double). */
  T3_CONFIG_LIST,    /**< A list of un-named items. */
  T3_CONFIG_SECTION, /**< A list of named items. */
  T3_CONFIG_PLIST    /**< A list of un-named items, written using %&lt;name> notation. */
} t3_config_type_t;

/** @struct t3_config_t
    An opaque struct representing a config or sub-config.
*/
typedef struct t3_config_t t3_config_t;

/** @struct t3_config_schema_t
    An opaque struct representing a schema.
*/
typedef struct t3_config_schema_t t3_config_schema_t;

/** Options struct used when reading a file. */
typedef struct {
  int flags; /**< Set of flags, or @c 0 for defaults. */
  /** Information for facilitating file inclusion. */
  union {
    /** Information for the default include mechanism. */
    struct {
      const char **path; /**< The @c NULL-terminated array of search paths, passed to
                            ::t3_config_open_from_path. */
      int flags;         /**< The flags, passed to ::t3_config_open_from_path. */
    } dflt;
    /** Information for the user include mechanism. */
    struct {
      FILE *(*open)(const char *name,
                    void *data); /**< The function to call to open an include file. */
      void *data;                /**< Data passed to the callback function. */
    } user;
  } include_callback;
} t3_config_opts_t;

/** @name Flags for ::t3_config_opts_t. */
/*@{*/
/** Return extra information about the error in the ::t3_config_error_t struct. */
#define T3_CONFIG_VERBOSE_ERROR (1 << 0)
/** Activate the default include file mechanism.
    When using this flags, the @c dflt member of the ::t3_config_opts_t
    @c include_callback union must be filled in. */
#define T3_CONFIG_INCLUDE_DFLT (1 << 1)
/** Activate the user-defined include file mechanism.
    When using this flags, the @c user member of the ::t3_config_opts_t
    @c include_callback union must be filled in. */
#define T3_CONFIG_INCLUDE_USER (1 << 2)
/** Return the file name where the error occured in the ::t3_config_error_t struct. */
#define T3_CONFIG_ERROR_FILE_NAME (1 << 3)
/*@}*/

/** A structure representing an error, with line number.
    Used by ::t3_config_read_file and ::t3_config_read_buffer. If @p error
    equals ::T3_ERR_PARSE_ERROR, @p line_number will be set to the line
    on which the error was encountered.
*/
typedef struct {
  int error;       /**< An integer indicating what went wrong. */
  int line_number; /**< The line number where the error occured. */
  char *extra;     /**< Further information about the error or @c NULL, but only if
                      ::T3_CONFIG_VERBOSE_ERROR was set. Must be free'd. */
  char *file_name; /**< File in which the error occured, but only if
                      ::T3_CONFIG_ERROR_FILE_NAME was set. Must be free'd. */
} t3_config_error_t;

/** @name Error codes (libt3config specific) */
/*@{*/
/** Error code: Value is out of range. */
#define T3_ERR_OUT_OF_RANGE (-80)
/** Error code: Parse error. */
#define T3_ERR_PARSE_ERROR (-79)
/** Error code: Key already exists. */
#define T3_ERR_DUPLICATE_KEY (-78)

/** Error code: Constraint is invalid. */
#define T3_ERR_INVALID_CONSTRAINT (-77)
/** Error code: The key has a different type than allowed by the schema. */
#define T3_ERR_INVALID_KEY_TYPE (-76)
/** Error code: The key is not allowed here by the schema. */
#define T3_ERR_INVALID_KEY (-75)
/** Error code: The configuration violates a constraint in the schema. */
#define T3_ERR_CONSTRAINT_VIOLATION (-74)
/** Error code: The type definition refers to itself, either directly or indirectly. */
#define T3_ERR_RECURSIVE_TYPE (-73)
/** Error code: An included file includes itself, either directly or indirectly. */
#define T3_ERR_RECURSIVE_INCLUDE (-72)
/*@}*/

#if INT_MAX < 2147483647
typedef long t3_config_int_t;
#define T3_CONFIG_INT_MAX LONG_MAX
#define T3_CONFIG_INT_MIN LONG_MIN
#define T3_CONFIG_INT_PRI "l"
#else
typedef int t3_config_int_t;
#define T3_CONFIG_INT_MAX INT_MAX
#define T3_CONFIG_INT_MIN INT_MIN
#define T3_CONFIG_INT_PRI ""
#endif

/** Create a new config.
    @return A pointer to the new config or @c NULL if out of memory.
    Each config is a section. This function creates an empty section.
*/
T3_CONFIG_API t3_config_t *t3_config_new(void);
/** Read a config from a @c FILE.
    @param file The @c FILE to read from.
    @param error A pointer to the location to store an error value (or @c NULL).
    @param opts A pointer to a struct containing options, or @c NULL to use the defaults.
    @return A pointer to the new config or @c NULL on error.
*/
T3_CONFIG_API t3_config_t *t3_config_read_file(FILE *file, t3_config_error_t *error,
                                               const t3_config_opts_t *opts);
/** Read a config from memory.
    @param buffer The buffer to parse.
    @param size The size of the buffer.
    @param error A pointer to the location to store an error value (or @c NULL).
    @param opts A pointer to a struct containing options, or @c NULL to use the defaults.
    @return A pointer to the new config or @c NULL on error.
*/
T3_CONFIG_API t3_config_t *t3_config_read_buffer(const char *buffer, size_t size,
                                                 t3_config_error_t *error,
                                                 const t3_config_opts_t *opts);
/** Write a config to a @c FILE.
    @param config The config to write.
    @param file The @c FILE to write to.
    @return Either ::T3_ERR_ERRNO or ::T3_ERR_SUCCESS
*/
T3_CONFIG_API int t3_config_write_file(t3_config_t *config, FILE *file);
/** Free all memory used by a (sub-)config.
    If you wish to remove a sub-config, either use ::t3_config_erase or
    ::t3_config_erase_from_list, or call ::t3_config_unlink or
    ::t3_config_unlink_from_list on the sub-config first.
*/
T3_CONFIG_API void t3_config_delete(t3_config_t *config);

/** Unlink an item from a (sub-)config. */
T3_CONFIG_API t3_config_t *t3_config_unlink(t3_config_t *config, const char *name);
/** Unlink an item from a (sub-)config or list. */
T3_CONFIG_API t3_config_t *t3_config_unlink_from_list(t3_config_t *list, t3_config_t *item);
/** Erase an item from a (sub-)config.
    All memory related to the item and all sub-items is released. */
T3_CONFIG_API void t3_config_erase(t3_config_t *config, const char *name);
/** Erase an item from a (sub-)config or list.
    All memory related to the item and all sub-items is released. */
T3_CONFIG_API void t3_config_erase_from_list(t3_config_t *list, t3_config_t *item);

/** Add (or overwrite) a boolean value to a (sub-)config.
    @param config The (sub-)config to add to.
    @param name The name under which to add the item, or @c NULL if adding to a list.
    @param value The value to add.
    @retval ::T3_ERR_SUCCESS on successful addition.
    @retval ::T3_ERR_BAD_ARG if @p config is not a list or section, or name is not set correctly.
    @retval ::T3_ERR_OUT_OF_MEMORY .

    If an item with the given name already exists, it is replaced by the new
    value. If necessary, memory used by sub-items or values is released prior
    to replacing the value, but after checking for argument validity.
*/
T3_CONFIG_API int t3_config_add_bool(t3_config_t *config, const char *name, t3_bool value);
/** Add (or overwrite) an integer value to a (sub-)config.
    See ::t3_config_add_bool for details.

    @deprecated Use ::t3_config_add_int64 instead.
*/
T3_CONFIG_API int t3_config_add_int(t3_config_t *config, const char *name, t3_config_int_t value);
/** Add (or overwrite) an integer value to a (sub-)config.
    See ::t3_config_add_bool for details.
*/
T3_CONFIG_API int t3_config_add_int64(t3_config_t *config, const char *name, int64_t value);
/** Add (or overwrite) an floating point number value to a (sub-)config.
    See ::t3_config_add_bool for details.
*/
T3_CONFIG_API int t3_config_add_number(t3_config_t *config, const char *name, double value);
/** Add (or overwrite) a string value to a (sub-)config.

    A copy of the string is stored in the (sub-)config. See ::t3_config_add_bool for
    further details.
*/
T3_CONFIG_API int t3_config_add_string(t3_config_t *config, const char *name, const char *value);
/** Add (or overwrite) a list to the (sub-)config.
    @param config The (sub-)config to add to.
    @param name The name under which to add the item, or @c NULL if adding to a list.
    @param error A pointer to the location to store an error value (or @c NULL).
    @return A pointer to the newly added list, or @c NULL on error.

    If an item with the given name already exists, it is replaced by the new
    list. If necessary, memory used by sub-items or values is released prior
    to replacing the value, but after checking for argument validity.

    If @p config is @c NULL, an error of type ::T3_ERR_BAD_ARG is returned.
*/
T3_CONFIG_API t3_config_t *t3_config_add_list(t3_config_t *config, const char *name, int *error);
/** Add (or overwrite) a plist to the (sub-)config.
    See ::t3_config_add_list for details.
*/
T3_CONFIG_API t3_config_t *t3_config_add_plist(t3_config_t *config, const char *name, int *error);
/** Add (or overwrite) a section to the (sub-)config.
    See ::t3_config_add_list for details.
*/
T3_CONFIG_API t3_config_t *t3_config_add_section(t3_config_t *config, const char *name, int *error);
/** Add (or overwrite) an existing value to the (sub-)config.
    The primary use of this function is to add complete sections created
    earlier from some source, or unlinked from elsewhere in the configuration.
    This should @b not be used to link the same configuration into the tree
    multiple times.

    See ::t3_config_add_bool for details.
*/
T3_CONFIG_API int t3_config_add_existing(t3_config_t *config, const char *name, t3_config_t *value);
/** Set the type of list of an existing list-type (sub-)config. */
T3_CONFIG_API int t3_config_set_list_type(t3_config_t *config, t3_config_type_t type);

/** Retrieve a sub-config.
    @param config The (sub-)config to retrieve from.
    @param name The name of the sub-config to retrieve, or @c NULL for the first sub-config.
    @return The requested sub-config, or @c NULL if no sub-config exists with the given name.

    This function can be used both to retrieve a named sub-config, or to get the
    first sub-config for iteration over all items in this (sub-)config. For lists,
    use @c NULL for the @p name parameter to get the first item in the list, and
    use ::t3_config_get_next to retrieve further items.
*/
T3_CONFIG_API t3_config_t *t3_config_get(const t3_config_t *config, const char *name);
/** Get the type of a (sub-)config.
    See ::t3_config_type_t for possible types.
*/
T3_CONFIG_API t3_config_type_t t3_config_get_type(const t3_config_t *config);
/** Check if a (sub-)config is a ::T3_CONFIG_LIST or ::T3_CONFIG_PLIST. */
T3_CONFIG_API t3_bool t3_config_is_list(const t3_config_t *config);
/** Get the name of the (sub-)config.
    Retrieves the name of the @p config, or @c NULL if @p config is part of a
    list or the top-level config.
*/
T3_CONFIG_API const char *t3_config_get_name(const t3_config_t *config);
/** Get the line number at which the (sub-)config was defined. */
T3_CONFIG_API int t3_config_get_line(const t3_config_t *config);

/** Get the boolean value from a config with ::T3_CONFIG_BOOL type.
    @return The boolean value of @p config, or ::t3_false if @p config is @c NULL or not of type
    ::T3_CONFIG_BOOL.
*/
T3_CONFIG_API t3_bool t3_config_get_bool(const t3_config_t *config);
/** Get the integer value from a config with ::T3_CONFIG_INT type.
    @return The integer value of @p config, or @c 0 if @p config is @c NULL or not of type
    ::T3_CONFIG_INT.

    @deprecated Use ::t3_config_get_int64 instead.
*/
T3_CONFIG_API t3_config_int_t t3_config_get_int(const t3_config_t *config);
/** Get the integer value from a config with ::T3_CONFIG_INT type.
    @return The integer value of @p config, or @c 0 if @p config is @c NULL or not of type
    ::T3_CONFIG_INT.
*/
T3_CONFIG_API int64_t t3_config_get_int64(const t3_config_t *config);
/** Get the floating point value from a config with ::T3_CONFIG_NUMBER type.
    @return The floating point value of @p config, or @c 0.0 if @p config is @c NULL or not of type
    ::T3_CONFIG_NUMBER.
*/
T3_CONFIG_API double t3_config_get_number(const t3_config_t *config);
/** Get the string value from a config with ::T3_CONFIG_STRING type.
    @return The string value of @p config, or @c NULL if @p config is @c NULL or not of type
    ::T3_CONFIG_STRING.
*/
T3_CONFIG_API const char *t3_config_get_string(const t3_config_t *config);

/** Get the boolean value from a config with ::T3_CONFIG_BOOL type.
    @return The boolean value of @p config, or @p dflt @p config is @c NULL or not of type
    ::T3_CONFIG_BOOL.
*/
T3_CONFIG_API t3_bool t3_config_get_bool_dflt(const t3_config_t *config, t3_bool dflt);
/** Get the integer value from a config with ::T3_CONFIG_INT type.
    @return The integer value of @p config, or @p dflt if @p config is @c NULL or not of type
    ::T3_CONFIG_INT.

    @deprecated Use ::t3_config_get_int64_dflt instead.
*/
T3_CONFIG_API t3_config_int_t t3_config_get_int_dflt(const t3_config_t *config,
                                                     t3_config_int_t dflt);
/** Get the integer value from a config with ::T3_CONFIG_INT type.
    @return The integer value of @p config, or @p dflt if @p config is @c NULL or not of type
    ::T3_CONFIG_INT.
*/
T3_CONFIG_API int64_t t3_config_get_int64_dflt(const t3_config_t *config, int64_t dflt);
/** Get the floating point value from a config with ::T3_CONFIG_NUMBER type.
    @return The floating point value of @p config, or @p dflt if @p config is @c NULL or not of
    type ::T3_CONFIG_NUMBER.
*/
T3_CONFIG_API double t3_config_get_number_dflt(const t3_config_t *config, double dflt);
/** Get the string value from a config with ::T3_CONFIG_STRING type.
    @return The string value of @p config, or @p dflt if @p config is @c NULL or not of type
    ::T3_CONFIG_STRING.
*/
T3_CONFIG_API const char *t3_config_get_string_dflt(const t3_config_t *config, const char *dflt);

/** Take ownership of the string value from a config with ::T3_CONFIG_STRING type.
    @return The string value of @p config, or @c NULL if @p config is @c NULL or not of type
    ::T3_CONFIG_STRING.

    After calling this function, the type of the config will be set to ::T3_CONFIG_NONE.
*/
T3_CONFIG_API char *t3_config_take_string(t3_config_t *config);
/** Get the next sub-config from a section or list.
    @return A pointer to the next sub-config or @c NULL if there is no next sub-config.

    This can be used in combination with t3_config_get to iterate over a list
    or section:
    @code
    void iterate(t3_config_t *config) {
        t3_config_t *item;

        for (item = t3_config_get(config, NULL); item != NULL; item = t3_config_get_next(item)) {
            // Do something with item.
        }
    }
    @endcode
*/
T3_CONFIG_API t3_config_t *t3_config_get_next(const t3_config_t *config);
/** Get the number of elements in a section or list.
    If @p config is @c NULL or not a section or list, this function will return 0.
*/
T3_CONFIG_API int t3_config_get_length(const t3_config_t *config);

/** Find a specific value in a section or list.
    @param config The section or list to search.
    @param predicate A function to call which determines whether an item in the section or list
    matches.
    @param data A pointer to user data which will be passed as the second argument to @p predicate.
    @param start_from A pointer to the last found item, or @c NULL to start from the beginning of
    the list.
    @return A pointer to the first item for which @p predicate returned ::t3_true, or @c NULL of
    none was found.

    This function allows one to easily find an item matching a predicate in a
    section or list. It can also be used to find all matching items, by simply
    passing the result of the last call as the @p start_from parameter until
    the function returns @c NULL.
*/
T3_CONFIG_API t3_config_t *t3_config_find(const t3_config_t *config,
                                          t3_bool (*predicate)(const t3_config_t *, const void *),
                                          const void *data, t3_config_t *start_from);

/** Get the value of ::T3_CONFIG_VERSION corresponding to the actual used library.
    @return The value of ::T3_CONFIG_VERSION.

    This function can be useful to determine at runtime what version of the library
    was linked to the program. Although currently there are no known uses for this
    information, future library additions may prompt library users to want to operate
    differently depending on the available features.
*/
T3_CONFIG_API long t3_config_get_version(void);

/** Get a string description for an error code.
    @param error The error code returned by a function in libt3config.
    @return An internationalized string description for the error code.
*/
T3_CONFIG_API const char *t3_config_strerror(int error);

/** Read a schema from a @c FILE.
    @param file The @c FILE to read from.
    @param error A pointer to the location to store an error value (or @c NULL).
    @param opts A pointer to a struct containing options, or @c NULL to use the defaults.
    @return A pointer to the new schema or @c NULL on error.
*/
T3_CONFIG_API t3_config_schema_t *t3_config_read_schema_file(FILE *file, t3_config_error_t *error,
                                                             const t3_config_opts_t *opts);
/** Read a schema from memory.
    @param buffer The buffer to parse.
    @param size The size of the buffer.
    @param error A pointer to the location to store an error value (or @c NULL).
    @param opts A pointer to a struct containing options, or @c NULL to use the defaults.
    @return A pointer to the new schema or @c NULL on error.
*/
T3_CONFIG_API t3_config_schema_t *t3_config_read_schema_buffer(const char *buffer, size_t size,
                                                               t3_config_error_t *error,
                                                               const t3_config_opts_t *opts);
/** Validate that a config adheres to a schema.
    @param config The config to validate.
    @param schema The schema to validate against.
    @param error A pointer to the location to store an error value (or @c NULL).
    @param flags A set of flags influencing the behaviour, or @c 0 for defaults.
    @return ::t3_true if the config is adheres to the schema, ::t3_false otherwise.

    Currently, only the flag T3_CONFIG_VERBOSE_ERROR can be used.
*/
T3_CONFIG_API t3_bool t3_config_validate(t3_config_t *config, const t3_config_schema_t *schema,
                                         t3_config_error_t *error, int flags);
/** Free all memory used by @p schema. */
T3_CONFIG_API void t3_config_delete_schema(t3_config_schema_t *schema);

/** @name Flags for ::t3_config_open_from_path. */
/*@{*/
/** Search paths should first be split on colons or semi-colons (depending on the platform
    standard). */
#define T3_CONFIG_SPLIT_PATH (1 << 0)
/** Only allow file names which are in the path.

    This flag disallows use of .. to read files in directories above the
    directories in the path, and also disallows absolute file names.
*/
#define T3_CONFIG_CLEAN_NAME (1 << 1)
/*@}*/

/** Open a file for reading using a search path.
    @param path A @c NULL terminated array of search paths.
    @param name The name of the file to open.
    @param flags A set of flags influencing the behaviour, or @c 0 for defaults.
    @return A file opened for reading, or @c NULL on error.

    On error, @c errno is set. Possible flags for @p opts are: ::T3_CONFIG_SPLIT_PATH.
*/
T3_CONFIG_API FILE *t3_config_open_from_path(const char **path, const char *name, int flags);

/** Get the line number at which the (sub-)configuration item was defined. */
T3_CONFIG_API int t3_config_get_line_number(const t3_config_t *config);
/** Get the file name in which the (sub-)configuration item was defined.
    This function will return @c NULL if it was not created in a file, or it
    was defined in the file/buffer passed to ::t3_config_read_file or
    ::t3_config_read_buffer.
*/
T3_CONFIG_API const char *t3_config_get_file_name(const t3_config_t *config);

/** Constants for ::t3_config_xdg_open_read and ::t3_config_xdg_open_write. */
typedef enum {
  T3_CONFIG_XDG_CONFIG_HOME, /**< Use the XDG configuration directory. Defaults to $HOME/.config. */
  T3_CONFIG_XDG_DATA_HOME,   /**< Use the XDG data directory. Defaults to $HOME/.local/share. */
  T3_CONFIG_XDG_CACHE_HOME,  /**< Use the XDG cache directory. Defaults to $HOME/.cache. */
  T3_CONFIG_XDG_RUNTIME_DIR  /**< Use the XDG runtime directory. Does not have a default. */
} t3_config_xdg_dirs_t;

/** A structure representing file to write to.
    This structure is returned by ::t3_config_open_write and ::t3_config_xdg_open_write.
*/
typedef struct t3_config_write_file_t t3_config_write_file_t;

/** Query whether this library instance supports the XDG Base Directory Specification support
    functions. */
T3_CONFIG_API t3_bool t3_config_xdg_supported(void);

/** Get a variable containing a specific XDG directory path.
    @param xdg_dir A constant indicating which XDG dir to use.
    @param program_dir An optional (but recommended) directory within the XDG dir to use.
    @param file_name_len The length of the file name that will be appended to this path.
    @return A string containing the path, or @c NULL on error.

    The returned string must be free'd. The @p file_name_len parameter allows
    extra memory to be allocated in the string to allow appending a slash and
    a string of length @p file_name_len.
*/
T3_CONFIG_API char *t3_config_xdg_get_path(t3_config_xdg_dirs_t xdg_dir, const char *program_dir,
                                           size_t file_name_len);

/** Open a configuration file for reading in one of the XDG Base Directory Specification
    directories.
    @param xdg_dir A constant indicating which XDG dir to use.
    @param program_dir An optional (but recommended) directory within the XDG dir to use.
    @param file_name The name of the configuration file to open.
    @return @c NULL is returned on error and @c errno is set.

    This function opens a file in one of the XDG Base Directory Specification
    directories. It uses the XDG_* environment variables to find the files, and
    uses the fallbacks as specified in the standard if the environment variables
    are not set.
*/
T3_CONFIG_API FILE *t3_config_xdg_open_read(t3_config_xdg_dirs_t xdg_dir, const char *program_dir,
                                            const char *file_name);

/** Open a configuration file for writing in one of the XDG Base Directory Specification
    directories.
    @param xdg_dir A constant indicating which XDG dir to use.
    @param program_dir An optional (but recommended) directory within the XDG dir to use.
    @param file_name The name of the configuration file to open.
    @return @c NULL is returned on error and @c errno is set.

    This function opens a temporary file in one of the XDG Base Directory
    Specification directories, associated with a named configuration file. This
    temporary file is then renamed to the configuration file on close. Using
    this method ensures that the configuration file is always intact, even when
    the program is for some reason halted in mid-write.

    This function uses the XDG_* environment variables to find the files, and
    uses the fallbacks as specified in the standard if the environment variables
    are not set.
*/
T3_CONFIG_API t3_config_write_file_t *t3_config_xdg_open_write(t3_config_xdg_dirs_t xdg_dir,
                                                               const char *program_dir,
                                                               const char *file_name);
/** Get the @c FILE member of a ::t3_config_write_file_t returned by ::t3_config_xdg_open_write.

    @deprecated Use ::t3_config_get_write_file instead.
*/
T3_CONFIG_API FILE *t3_config_xdg_get_file(t3_config_write_file_t *file);
/** Close a ::t3_config_write_file_t returned by ::t3_config_xdg_open_write.
    @param file The ::t3_config_write_file_t to close.
    @param cancel_rename Boolean indicating whether the temporary file should be renamed to the
    actual config file.
    @param force Boolean indicating whether to force file close on error.
    @return A boolean indicating whether the close was successful.

    The @p force parameter indicates whether the file should be closed and
    discared when an error is encountered. Errors may be an out of memory
    condition or failure of the rename to the intended file name. If @p force
    is @c t3_true, the file will be closed and discared on error, but the
    returned value will still be @c t3_false to allow detection of the failed
    close.

        @deprecated Use ::t3_config_close_write instead.
*/
T3_CONFIG_API t3_bool t3_config_xdg_close_write(t3_config_write_file_t *file, t3_bool cancel_rename,
                                                t3_bool force);

/** Open a configuration file for writing.
    @param file_name The name of the configuration file to open.
    @return @c NULL is returned on error and @c errno is set.

    This function opens a temporary file, associated with a named configuration
    file. This temporary file is then renamed to the configuration file on
    close. Using this method ensures that the configuration file is always
    intact, even when the program is for some reason halted in mid-write.
*/
T3_CONFIG_API t3_config_write_file_t *t3_config_open_write(const char *file_name);
/** Get the @c FILE member of a ::t3_config_write_file_t returned by ::t3_config_open_write or
 * ::t3_config_xdg_open_write. */
T3_CONFIG_API FILE *t3_config_get_write_file(t3_config_write_file_t *file);
/** Close a ::t3_config_write_file_t returned by ::t3_config_open_write or
   ::t3_config_xdg_open_write.
    @param file The ::t3_config_write_file_t to close.
    @param cancel_rename Boolean indicating whether the temporary file should be renamed to the
    actual config file.
    @param force Boolean indicating whether to force file close on error.
    @return A boolean indicating whether the close was successful.

    The @p force parameter indicates whether the file should be closed and
    discared when an error is encountered. Errors may be an out of memory
    condition or failure of the rename to the intended file name. If @p force
    is @c t3_true, the file will be closed and discared on error, but the
    returned value will still be @c t3_false to allow detection of the failed
    close.
*/
T3_CONFIG_API t3_bool t3_config_close_write(t3_config_write_file_t *file, t3_bool cancel_rename,
                                            t3_bool force);

#ifdef __cplusplus
} /* extern "C" */
#endif
/** @} */
#endif
