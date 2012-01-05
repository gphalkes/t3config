/* Copyright (C) 2011-2012 G.P. Halkes
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
#include <string.h>
#include <stdlib.h>

#include "config_internal.h"
#include "util.h"
#include "parser.h"

typedef struct {
	const t3_config_t *root, *types;
	t3_config_error_t *error;
	int flags;
} validation_context_t;

static char meta_schema_buffer[] = {
#include "meta_schema.bytes"
};

static char *dup_file_name(const t3_config_t *config) {
	if (config == NULL || config->file_name == NULL)
		return NULL;
	return _t3_config_strdup(config->file_name->file_name);
}

static t3_bool validate_aggregate_keys(const t3_config_t *config_part, const t3_config_t *schema_part,
	validation_context_t *context);

static t3_config_type_t resolve_type(const char *type_name, const t3_config_t *types, const t3_config_t **schema) {
	t3_config_type_t config_type;
	t3_config_t *type_schema;

	if ((config_type = _t3_config_str2type(type_name)) != T3_CONFIG_NONE)
		return config_type;

	/* This will generally result in a single lookup, because most type
	   definitions are elaborations on the basic types. The only reason not
	   to do so, is to have a single name that (for now) is used as a place
	   holder elsewhere in the schema that allows easy change. */
	while ((type_schema = t3_config_get(types, type_name)) != NULL) {
		type_name = t3_config_get_string(t3_config_get(type_schema, "type"));
		if ((config_type = _t3_config_str2type(type_name)) != T3_CONFIG_NONE) {
			*schema = type_schema;
			return config_type;
		}
	}
	return T3_CONFIG_NONE;
}

static t3_bool validate_constraints(const t3_config_t *config_part, const t3_config_t *schema_part,
		validation_context_t *context)
{
	const t3_config_t *constraint;

	for (constraint = t3_config_get(t3_config_get(schema_part, "constraint"), NULL);
			constraint != NULL; constraint = t3_config_get_next(constraint))
	{
		if (constraint->type == (int) T3_CONFIG_EXPRESSION && !_t3_config_evaluate_expr(constraint->value.expr, config_part, context->root)) {
			if (context->error != NULL) {
				context->error->error = T3_ERR_CONSTRAINT_VIOLATION;
				context->error->line_number = config_part->line_number;
				if (context->flags & T3_CONFIG_VERBOSE_ERROR)
					context->error->extra = _t3_config_strdup(constraint->value.expr->value.operand[1]->value.string);
				if (context->flags & T3_CONFIG_ERROR_FILE_NAME)
					context->error->file_name = dup_file_name(config_part);
			}
			return t3_false;
		}
	}
	return t3_true;
}

static t3_bool validate_key(const t3_config_t *config_part, t3_config_type_t type, const t3_config_t *schema_part,
		validation_context_t *context)
{
	if (type != config_part->type && !(type == T3_CONFIG_LIST && config_part->type == T3_CONFIG_PLIST) && type != (int) T3_CONFIG_ANY) {
		if (context->error != NULL) {
			context->error->error = T3_ERR_INVALID_KEY_TYPE;
			context->error->line_number = config_part->line_number;
			if (context->flags & T3_CONFIG_VERBOSE_ERROR)
				context->error->extra = config_part->name == NULL ? NULL : _t3_config_strdup(config_part->name);
			if (context->flags & T3_CONFIG_ERROR_FILE_NAME)
				context->error->file_name = dup_file_name(config_part);
		}
		return t3_false;
	}

	if (type == T3_CONFIG_SECTION)
		return validate_aggregate_keys(config_part, schema_part, context);
	else if (type == T3_CONFIG_LIST && t3_config_get(schema_part, "item-type") != NULL)
		return validate_aggregate_keys(config_part, schema_part, context);
	else
		return validate_constraints(config_part, schema_part, context);
}

static t3_bool validate_aggregate_keys(const t3_config_t *config_part, const t3_config_t *schema_part,
		validation_context_t *context)
{
	const t3_config_t *allowed_keys = t3_config_get(schema_part, "allowed-keys"),
		*item_type = t3_config_get(schema_part, "item-type"),
		*sub_part, *sub_schema;
	t3_config_type_t resolved_type;

	if (allowed_keys != NULL || item_type != NULL) {
		for (sub_part = t3_config_get(config_part, NULL); sub_part != NULL; sub_part = t3_config_get_next(sub_part)) {
			if (allowed_keys != NULL && (sub_schema = t3_config_get(allowed_keys, sub_part->name)) != NULL) {
				resolved_type = resolve_type(t3_config_get_string(t3_config_get(sub_schema, "type")), context->types, &sub_schema);
				if (!validate_key(sub_part, resolved_type, sub_schema, context))
					return t3_false;
			} else if (item_type != NULL) {
				sub_schema = NULL;
				resolved_type = resolve_type(t3_config_get_string(item_type), context->types, &sub_schema);
				if (!validate_key(sub_part, resolved_type, sub_schema, context))
					return t3_false;
			} else {
				if (context->error != NULL) {
					context->error->error = T3_ERR_INVALID_KEY;
					context->error->line_number = sub_part->line_number;
					if (context->flags & T3_CONFIG_VERBOSE_ERROR)
						context->error->extra = _t3_config_strdup(sub_part->name);
					if (context->flags & T3_CONFIG_ERROR_FILE_NAME)
						context->error->file_name = dup_file_name(sub_part);
				}
				return t3_false;
			}
		}
	}

	return validate_constraints(config_part, schema_part, context);
}

t3_bool t3_config_validate(t3_config_t *config, const t3_config_schema_t *schema,
		t3_config_error_t *error, int flags)
{
	validation_context_t context;

	if ((int) ((const t3_config_t *) schema)->type != T3_CONFIG_SCHEMA) {
		if (error != NULL) {
			error->error = T3_ERR_BAD_ARG;
			error->line_number = 0;
			if (flags & T3_CONFIG_VERBOSE_ERROR)
				error->extra = NULL;
			if (flags & T3_CONFIG_ERROR_FILE_NAME)
				error->file_name = NULL;
		}
		return t3_false;
	}

	context.root = config;
	context.types = t3_config_get((const t3_config_t *) schema, "types");
	context.error = error;
	context.flags = flags;

	return validate_aggregate_keys(config, (const t3_config_t *) schema, &context);
}

static expr_node_t *parse_constraint_string(const char *constraint, int *error) {
	parse_context_t context;
	int retval;

	context.scan_type = SCAN_BUFFER;
	context.buffer = constraint;
	context.buffer_size = strlen(constraint);
	context.buffer_idx = 0;
	context.line_number = 1;
	context.result = NULL;
	context.constraint_parser = t3_true;

	/* Initialize lexer. */
	if (_t3_config_lex_init_extra(&context, &context.scanner) != 0)
		return NULL;

	/* Perform parse. */
	if ((retval = _t3_config_parse_constraint(&context)) != 0) {
		if (error != NULL)
			*error = retval;
		/* On failure, we free all memory allocated by the partial parse ... */
		_t3_config_delete_expr(context.result);
		/* ... and set context->config to NULL so we return NULL at the end. */
		context.result = NULL;
	}
	/* Free memory allocated by lexer. */
	_t3_config_lex_destroy(context.scanner);
	return context.result;
}

static t3_bool parse_constraints(t3_config_t *schema, const t3_config_t *root, t3_config_error_t *error, const t3_config_opts_t *opts) {
	t3_config_t *constraint;
	t3_config_t *part;

	for (constraint = t3_config_get(t3_config_get(schema, "constraint"), NULL);
			constraint != NULL; constraint = t3_config_get_next(constraint))
	{
		expr_node_t *expr = parse_constraint_string(t3_config_get_string(constraint), error == NULL ? NULL : &error->error);
		if (expr == NULL) {
			if (error != NULL) {
				error->line_number = constraint->line_number;
				if (opts != NULL) {
					if (opts->flags & T3_CONFIG_VERBOSE_ERROR)
						error->extra = NULL;
					if (opts->flags & T3_CONFIG_ERROR_FILE_NAME)
						error->file_name = dup_file_name(constraint);
				}
			}
			return t3_false;
		}
		if (!_t3_config_validate_expr(expr, schema, root)) {
			_t3_config_delete_expr(expr);
			if (error != NULL) {
				error->error = T3_ERR_INVALID_CONSTRAINT;
				error->line_number = constraint->line_number;
				if (opts != NULL) {
					if (opts->flags & T3_CONFIG_VERBOSE_ERROR)
						error->extra = NULL;
					if (opts->flags & T3_CONFIG_ERROR_FILE_NAME)
						error->file_name = dup_file_name(constraint);
				}
			}
			return t3_false;
		}

		constraint->type = T3_CONFIG_EXPRESSION;
		if (expr->value.operand[1]->value.string == NULL)
			expr->value.operand[1]->value.string = constraint->value.string;
		else
			free(constraint->value.string);
		constraint->value.expr = expr;
	}

	for (schema = t3_config_get(schema, NULL); schema != NULL; schema = t3_config_get_next(schema)) {
		if (schema->type != T3_CONFIG_SECTION)
			continue;

		for (part = t3_config_get(schema, NULL); part != NULL; part = t3_config_get_next(part)) {
			if (!parse_constraints(part, root, error, opts))
				return t3_false;
		}
	}
	return t3_true;
}


static t3_bool check_type_for_loop(t3_config_t *type, const t3_config_t *types) {
	const char *referred_type;
	int saved_line_number = type->line_number;
	t3_bool result;

	if (type->line_number < 0)
		return t3_true;

	referred_type = t3_config_get(type, "type")->value.string;
	if (_t3_config_str2type(referred_type) != T3_CONFIG_NONE)
		return t3_false;

	type->line_number = -1;
	result = check_type_for_loop(t3_config_get(types, referred_type), types);
	type->line_number = saved_line_number;
	return result;
}

static t3_bool has_loops(const t3_config_t *schema, t3_config_error_t *error, const t3_config_opts_t *opts) {
	const t3_config_t *types = t3_config_get(schema, "types");
	t3_config_t *type;

	if (types == NULL)
		return t3_false;

	for (type = t3_config_get(types, NULL); type != NULL; type = t3_config_get_next(type)) {
		if (check_type_for_loop(type, types)) {
			if (error != NULL) {
				error->error = T3_ERR_RECURSIVE_TYPE;
				error->line_number = type->line_number;
				if (opts != NULL) {
					if (opts->flags & T3_CONFIG_VERBOSE_ERROR)
						error->extra = _t3_config_strdup(t3_config_get(type, "type")->value.string);
					if (opts->flags & T3_CONFIG_ERROR_FILE_NAME)
						error->file_name = dup_file_name(type);
				}
			}
			return t3_true;
		}
	}
	return t3_false;
}

static t3_config_schema_t *handle_schema_validation(t3_config_t *config, t3_config_error_t *error, const t3_config_opts_t *opts) {
	t3_config_t *meta_schema = NULL;
	t3_config_error_t local_error;

	if ((meta_schema = t3_config_read_buffer(meta_schema_buffer, sizeof(meta_schema_buffer), &local_error, NULL)) == NULL||
			!parse_constraints(meta_schema, meta_schema, &local_error, NULL))
	{
		if (error != NULL) {
			error->error = local_error.error == T3_ERR_OUT_OF_MEMORY ? T3_ERR_OUT_OF_MEMORY : T3_ERR_INTERNAL;
			error->line_number = 0;
			if (opts != NULL) {
				if (opts->flags & T3_CONFIG_VERBOSE_ERROR)
					error->extra = NULL;
				if (opts->flags & T3_CONFIG_ERROR_FILE_NAME)
					error->file_name = NULL;
			}
		}
		goto error_end;
	}
	meta_schema->type = T3_CONFIG_SCHEMA;

	if (!t3_config_validate(config, (t3_config_schema_t *) meta_schema, error, opts == NULL ? 0 : opts->flags) ||
			has_loops(config, error, opts) ||
			!parse_constraints(config, config, error, opts))
		goto error_end;

	t3_config_delete(meta_schema);
	config->type = T3_CONFIG_SCHEMA;
	return (t3_config_schema_t *) config;

error_end:
	t3_config_delete(config);
	t3_config_delete(meta_schema);
	return NULL;
}

t3_config_schema_t *t3_config_read_schema_file(FILE *file, t3_config_error_t *error, const t3_config_opts_t *opts) {
	t3_config_t *config;
	if ((config = t3_config_read_file(file, error, opts)) == NULL)
		return NULL;
	return handle_schema_validation(config, error, opts);
}

t3_config_schema_t *t3_config_read_schema_buffer(const char *buffer, size_t size,
		t3_config_error_t *error, const t3_config_opts_t *opts)
{
	t3_config_t *config;
	if ((config = t3_config_read_buffer(buffer, size, error, opts)) == NULL)
		return NULL;
	return handle_schema_validation(config, error, opts);
}

void t3_config_delete_schema(t3_config_schema_t *schema) {
	t3_config_delete((t3_config_t *) schema);
}

#ifdef DEBUG
t3_config_schema_t *_t3_config_config2schema(t3_config_t *config, t3_config_error_t *error, const t3_config_opts_t *opts) {
	return handle_schema_validation(config, error, opts);
}
#endif
