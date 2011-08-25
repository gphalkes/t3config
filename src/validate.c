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

#define _T3_CONFIG_CONST const
#include "config_internal.h"
#include "util.h"

static t3_bool validate_aggregate_keys(t3_config_t *config_part, const t3_config_t *schema_part,
	const t3_config_t *types, t3_config_error_t *error);

static char basic_types_buffer[] = {
#include ".objects/basic_types.c"
};
static char meta_schema_buffer[] = {
#include ".objects/meta_schema.c"
};

static t3_config_t *basic_types, *meta_schema;

static const t3_config_t *resolve_type(const t3_config_t *types, const char *type_name) {
	t3_config_t *type_schema;
	if ((type_schema = t3_config_get(basic_types, type_name)) != NULL)
		return type_schema;

	while ((type_schema = t3_config_get(types, type_name)) != NULL) {
		type_name = t3_config_get_string(t3_config_get(type_schema, "type"));
		if (_t3_config_str2type(type_name) != T3_CONFIG_NONE)
			return type_schema;
	}
	return NULL;
}

static t3_bool validate_key(t3_config_t *config_part, const t3_config_t *schema_part,
		const t3_config_t *types, t3_config_error_t *error)
{
	/* Make sure that we have the most complete definition of the type name. This
	   can be one of the definitions from basic_types, or a definition from the
	   types definition. */
	const t3_config_t *type_schema = resolve_type(types, t3_config_get_string(t3_config_get(schema_part, "type")));
	t3_config_type_t type = _t3_config_str2type(t3_config_get_string(t3_config_get(type_schema, "type")));

	if (type != config_part->type) {
		if (error != NULL) {
			error->line_number = config_part->line_number;
			error->error = T3_ERR_INVALID_KEY_TYPE;
		}
		return t3_false;
	}

	if (type == T3_CONFIG_SECTION)
		return validate_aggregate_keys(config_part, type_schema, types, error);
	else if (type == T3_CONFIG_LIST && t3_config_get(type_schema, "item-type") != NULL)
		return validate_aggregate_keys(config_part, type_schema, types, error);

	return t3_true;
}

static t3_bool validate_aggregate_keys(t3_config_t *config_part, const t3_config_t *schema_part,
		const t3_config_t *types, t3_config_error_t *error)
{
	t3_config_t *allowed_keys = t3_config_get(schema_part, "allowed_keys"),
		*item_type = t3_config_get(schema_part, "item-type"),
		*sub_part, *sub_schema, *constraints;

	for (sub_part = t3_config_get(config_part, NULL); sub_part != NULL; sub_part = t3_config_get_next(sub_part)) {
		if (allowed_keys != NULL && (sub_schema = t3_config_get(allowed_keys, sub_part->name)) != NULL) {
			if (!validate_key(sub_part, sub_schema, types, error))
				return t3_false;
		} else if (item_type != NULL) {
			if (!validate_key(sub_part, resolve_type(types, t3_config_get_string(item_type)), types, error))
				return t3_false;
		} else {
			if (error != NULL) {
				error->line_number = sub_part->line_number;
				error->error = T3_ERR_INVALID_KEY;
			}
			return t3_false;
		}
	}

	for (constraints = t3_config_get(t3_config_get(schema_part, "constraint"), NULL);
			constraints != NULL; constraints = t3_config_get_next(constraints))
	{
		if (!_t3_config_evaluate_expr(constraints->value.expr, config_part)) {
			if (error != NULL) {
				error->error = T3_ERR_CONSTRAINT_VIOLATION;
				error->line_number = config_part->line_number;
			}
			return t3_false;
		}
	}
	return t3_true;
}

t3_bool t3_config_validate(t3_config_t *config, t3_config_schema_t *schema, t3_config_error_t *error) {
	if (((t3_config_t *) schema)->type != T3_CONFIG_SCHEMA) {
		if (error != NULL) {
			error->error = T3_ERR_INVALID_SCHEMA;
			error->line_number = 0;
		}
		return t3_false;
	}
	return validate_aggregate_keys(config, schema, t3_config_get(schema, "types"), error);
}

t3_config_schema_t *t3_config_read_schema_file(FILE *file, t3_config_error_t *error, void *opts) {
	t3_config_t *config = t3_config_read_file(file, error, opts);
	if (config == NULL)
		return NULL;

	if (!t3_config_validate(config, meta_schema, error))
		return NULL;
	//FIXME: parse all constraints

	config->type = T3_CONFIG_SCHEMA;
	return config;
}

int t3_config_init_schemas(void) {
	t3_config_error_t error;

	if (basic_types != NULL)
		return T3_ERR_SUCCESS;

	if ((basic_types = t3_config_read_buffer(basic_types_buffer, sizeof(basic_types_buffer), &error, NULL)) == NULL)
		return error.error;
	if ((meta_schema = t3_config_read_buffer(meta_schema_buffer, sizeof(meta_schema_buffer), &error, NULL)) == NULL) {
		t3_config_delete(basic_types);
		return error.error;
	}
	//FIXME: parse all constraints
	meta_schema->type = T3_CONFIG_SCHEMA;
	return T3_ERR_SUCCESS;
}

void t3_config_release_schemas(void) {
	t3_config_delete(basic_types);
	basic_types = NULL;
	t3_config_delete(meta_schema);
	meta_schema = NULL;
}
