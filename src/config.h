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

typedef enum {
	T3_CONFIG_NONE,
	T3_CONFIG_BOOL,
	T3_CONFIG_INT,
	T3_CONFIG_STRING,
	T3_CONFIG_NUMBER,
	T3_CONFIG_LIST,
	T3_CONFIG_SECTION,
	T3_CONFIG_SCHEMA
} t3_config_item_type_t;

typedef struct t3_config_item_t t3_config_item_t;

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
#else
typedef int t3_config_int_t;
#define T3_CONFIG_INT_MAX INT_MAX
#define T3_CONFIG_INT_MIN INT_MIN
#endif

T3_CONFIG_API t3_config_item_t *t3_config_read_file(FILE *file, t3_config_error_t *error);
T3_CONFIG_API t3_config_item_t *t3_config_read_buffer(const char *buffer, t3_config_error_t *error);
T3_CONFIG_API int t3_config_write_file(t3_config_item_t *config, FILE *file);
T3_CONFIG_API void t3_config_delete(t3_config_item_t *config);

T3_CONFIG_API t3_config_item_t *t3_config_remove(t3_config_item_t *config, const char *name);

T3_CONFIG_API int t3_config_add_bool(t3_config_item_t *config, const char *name, t3_bool value);
T3_CONFIG_API int t3_config_add_int(t3_config_item_t *config, const char *name, t3_config_int_t value);
T3_CONFIG_API int t3_config_add_number(t3_config_item_t *config, const char *name, double value);
T3_CONFIG_API int t3_config_add_string(t3_config_item_t *config, const char *name, const char *value);
T3_CONFIG_API int t3_config_add_list(t3_config_item_t *config, const char *name);
T3_CONFIG_API int t3_config_add_section(t3_config_item_t *config, const char *name);

T3_CONFIG_API t3_config_item_t *t3_config_get(t3_config_item_t *config, const char *name);
T3_CONFIG_API t3_config_item_type_t t3_config_get_type(t3_config_item_t *config);
T3_CONFIG_API const char *t3_config_get_name(t3_config_item_t *config);

T3_CONFIG_API t3_bool t3_config_get_bool(t3_config_item_t *config);
T3_CONFIG_API t3_config_int_t t3_config_get_int(t3_config_item_t *config);
T3_CONFIG_API double t3_config_get_number(t3_config_item_t *config);
T3_CONFIG_API const char *t3_config_get_string(t3_config_item_t *config);
T3_CONFIG_API t3_config_item_t *t3_config_get_list(t3_config_item_t *config);
T3_CONFIG_API t3_config_item_t *t3_config_get_section(t3_config_item_t *config);
T3_CONFIG_API t3_config_item_t *t3_config_get_next(t3_config_item_t *config);

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
