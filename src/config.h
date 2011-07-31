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

#ifndef T3_CONFIG_H
#define T3_CONFIG_H

#include <stdio.h>
#include <limits.h>
#include "config_api.h"
#include "config_errors.h"

/** @defgroup t3config_other Functions, constants and enums. */
/** @addtogroup t3config_other */
/** @{ */

#ifdef __cplusplus
extern "C" {
#endif

/** The version of libt3config encoded as a single integer.

    The least significant 8 bits represent the patch level.
    The second 8 bits represent the minor version.
    The third 8 bits represent the major version.

	At runtime, the value of T3_CONFIG_VERSION can be retrieved by calling
	::t3_config_get_version.

    @internal
    The value 0 is an invalid value which should be replaced by the script
    that builds the release package.
*/
#define T3_CONFIG_VERSION 0

/** Types of values that can be stored in a config. */
typedef enum {
	T3_CONFIG_NONE, /**< Only used for uninitialized values, or as error value. */
	T3_CONFIG_BOOL, /**< Boolean value. */
	T3_CONFIG_INT, /**< Integer value, at least 32 bits wide. */
	T3_CONFIG_STRING, /**< String value. */
	T3_CONFIG_NUMBER, /**< Floating point value (double). */
	T3_CONFIG_LIST, /**< A list of un-named items. */
	T3_CONFIG_SECTION /**< A list of named items. */
} t3_config_item_type_t;

/** @struct t3_config_item_t
    An opaque struct representing a config or sub-config.
*/
typedef struct t3_config_item_t t3_config_item_t;

/** A structure representing an error, with line number.
    Used by ::t3_config_read_file and ::t3_config_read_buffer. If @p error
    equals ::T3_ERR_PARSE_ERROR, @p line_number will be set to the line
    on which the error was encountered.
*/
typedef struct {
	int error;
	int line_number;
} t3_config_error_t;

/** @name Error codes (libt3config specific) */
/*@{*/
/** Error code: Value is out of range. */
#define T3_ERR_OUT_OF_RANGE (-80)
/** Error code: Parse error. */
#define T3_ERR_PARSE_ERROR (-79)
/** Error code: Key already exists. */
#define T3_ERR_DUPLICATE_KEY (-78)
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
T3_CONFIG_API t3_config_item_t *t3_config_new(void);
/** Read a config from a @c FILE.
    @param file The @c FILE to read from.
    @param error A pointer to the location to store an error value (or @c NULL).
    @return A pointer to the new config or @c NULL on error.
*/
T3_CONFIG_API t3_config_item_t *t3_config_read_file(FILE *file, t3_config_error_t *error);
/** Read a config from memory.
    @param buffer The buffer to parse.
    @param size The size of the buffer.
    @param error A pointer to the location to store an error value (or @c NULL).
    @return A pointer to the new config or @c NULL on error.
*/
T3_CONFIG_API t3_config_item_t *t3_config_read_buffer(const char *buffer, size_t size, t3_config_error_t *error);
/** Write a config to a @c FILE.
    @param config The config to write.
    @param file The @c FILE to write to.
    @return Either ::T3_ERR_ERRNO or ::T3_ERR_SUCCESS
*/
T3_CONFIG_API int t3_config_write_file(t3_config_item_t *config, FILE *file);
/** Free all memory used by @p (sub-)config.
    If you wish to remove a sub-config, either use ::t3_config_erase or
    ::t3_config_erase_from_list, or call ::t3_config_unlink or
    ::t3_config_unlink_from_list on the sub-config first.
*/
T3_CONFIG_API void t3_config_delete(t3_config_item_t *config);

/** Unlink an item from a (sub-)config. */
T3_CONFIG_API t3_config_item_t *t3_config_unlink(t3_config_item_t *config, const char *name);
/** Unlink an item from a (sub-)config or list. */
T3_CONFIG_API t3_config_item_t *t3_config_unlink_from_list(t3_config_item_t *list, t3_config_item_t *item);
/** Erase an item from a (sub-)config.
    All memory related to the item and all sub-items is released. */
T3_CONFIG_API void t3_config_erase(t3_config_item_t *config, const char *name);
/** Erase an item from a (sub-)config or list.
    All memory related to the item and all sub-items is released. */
T3_CONFIG_API void t3_config_erase_from_list(t3_config_item_t *list, t3_config_item_t *item);

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
T3_CONFIG_API int t3_config_add_bool(t3_config_item_t *config, const char *name, t3_bool value);
/** Add (or overwrite) an integer value to a (sub-)config.
    See ::t3_config_add_bool for details.
*/
T3_CONFIG_API int t3_config_add_int(t3_config_item_t *config, const char *name, t3_config_int_t value);
/** Add (or overwrite) an floating point number value to a (sub-)config.
    See ::t3_config_add_bool for details.
*/
T3_CONFIG_API int t3_config_add_number(t3_config_item_t *config, const char *name, double value);
/** Add (or overwrite) a string value to a (sub-)config.

    A copy of the string is stored in the (sub-)config. See ::t3_config_add_bool for
    further details.
*/
T3_CONFIG_API int t3_config_add_string(t3_config_item_t *config, const char *name, const char *value);
/** Add (or overwrite) a list to the (sub-)config.
    @param config The (sub-)config to add to.
    @param name The name under which to add the item, or @c NULL if adding to a list.
    @param error A pointer to the location to store an error value (or @c NULL).
    @return A pointer to the newly added list, or @c NULL on error.

    If an item with the given name already exists, it is replaced by the new
    list. If necessary, memory used by sub-items or values is released prior
    to replacing the value, but after checking for argument validity.
*/
T3_CONFIG_API t3_config_item_t *t3_config_add_list(t3_config_item_t *config, const char *name, int *error);
/** Add (or overwrite) a section to the (sub-)config.
    See ::t3_config_add_list for details.
*/
T3_CONFIG_API t3_config_item_t *t3_config_add_section(t3_config_item_t *config, const char *name, int *error);
/** Add (or overwrite) an existing value to the (sub-)config.
    The primary use of this function is to add complete sections created
    earlier from some source, or unlinked from elsewhere in the configuration.
    This should @b not be used to link the same configuration into the tree
    multiple times.
    See ::t3_config_add_bool for details.
*/
T3_CONFIG_API int t3_config_add_existing(t3_config_item_t *config, const char *name, t3_config_item_t *value);

/** Retrieve a sub-config.
    @param config The (sub-)config to retrieve from.
    @param name The name of the sub-config to retrieve, or @c NULL for the first sub-config.
    @return The requested sub-config, or @c NULL if no sub-config exists with the given name.

    This function can be used both to retrieve a named sub-config, or to get the
    first sub-config for iteration over all items in this (sub-)config.
*/
T3_CONFIG_API t3_config_item_t *t3_config_get(const t3_config_item_t *config, const char *name);
/** Get the type of a (sub-)config.
    See ::t3_config_item_type_t for possible types.
*/
T3_CONFIG_API t3_config_item_type_t t3_config_get_type(const t3_config_item_t *config);
/** Get the name of the (sub-)config.
    Retrieves the name of the @p config, or @c NULL if @p config is part of a
    list or the top-level config.
*/
T3_CONFIG_API const char *t3_config_get_name(const t3_config_item_t *config);

/** Get the boolean value from a config with ::T3_CONFIG_BOOL type.
    @return The boolean value of @p config, or ::t3_false if @p config is @c NULL or not of type ::T3_CONFIG_BOOL.
*/
T3_CONFIG_API t3_bool t3_config_get_bool(const t3_config_item_t *config);
/** Get the integer value from a config with ::T3_CONFIG_INT type.
    @return The integer value of @p config, or @c 0 if @p config is @c NULL or not of type ::T3_CONFIG_INT.
*/
T3_CONFIG_API t3_config_int_t t3_config_get_int(const t3_config_item_t *config);
/** Get the floating point value from a config with ::T3_CONFIG_NUMBER type.
    @return The floating point value of @p config, or @c 0.0 if @p config is @c NULL or not of type ::T3_CONFIG_NUMBER.
*/
T3_CONFIG_API double t3_config_get_number(const t3_config_item_t *config);
/** Get the string value from a config with ::T3_CONFIG_STRING type.
    @return The string value of @p config, or @c NULL if @p config is @c NULL or not of type ::T3_CONFIG_STRING.
*/
T3_CONFIG_API const char *t3_config_get_string(const t3_config_item_t *config);
/** Get the next sub-config from a section or list.
    @return A pointer to the next sub-config or @c NULL if there is no next sub-config.

    This can be used in combination with t3_config_get to iterate over a list
    or section:
    @code
    void iterate(t3_config_item_t *config) {
        t3_config_item_t *item;

        for (item = t3_config_get(config, NULL); item != NULL; item = t3_config_get_next(item)) {
            // Do something with item.
        }
    }
    @endcode
*/
T3_CONFIG_API t3_config_item_t *t3_config_get_next(const t3_config_item_t *config);

/** Get the value of ::T3_CONFIG_VERSION corresponding to the actual used library.
    @ingroup t3window_other
    @return The value of ::T3_CONFIG_VERSION.

    This function can be useful to determine at runtime what version of the library
    was linked to the program. Although currently there are no known uses for this
    information, future library additions may prompt library users to want to operate
    differently depending on the available features.
*/
T3_CONFIG_API int t3_config_get_version(void);

/** Get a string description for an error code.
    @param error The error code returned by a function in libt3config.
    @return An internationalized string description for the error code.
*/
T3_CONFIG_API const char *t3_config_strerror(int error);

#ifdef __cplusplus
} /* extern "C" */
#endif
/** @} */
#endif
