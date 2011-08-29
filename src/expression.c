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
#include <string.h>
#include <stdlib.h>

#include "config_internal.h"
#include "util.h"


static const t3_config_t *lookup_node(const expr_node_t *expr, const t3_config_t *config,
	const t3_config_t *root, const t3_config_t *base)
{
	switch (expr->type) {
		case EXPR_PATH_ROOT:
			return root;
		case EXPR_IDENT:
			return t3_config_get(config, expr->value.string);
		case EXPR_PATH:
			return lookup_node(expr->value.operand[1], lookup_node(expr->value.operand[0], config, root, base), root, base);
		case EXPR_DEREF: {
			const t3_config_t *value = lookup_node(expr->value.operand[0], base, root, base);
			if (value == NULL || value->type != T3_CONFIG_STRING)
				return NULL;
			return t3_config_get(config, value->value.string);
		}
		case EXPR_THIS:
			return config;
		default:
			return NULL;
	}
}

static t3_bool resolve(const expr_node_t *expr, const t3_config_t *config, const t3_config_t *root, expr_node_t *result) {
	switch (expr->type) {
		case EXPR_BOOL_CONST:
		case EXPR_INT_CONST:
		case EXPR_NUMBER_CONST:
		case EXPR_STRING_CONST:
			*result = *expr;
			return t3_true;
		case EXPR_PATH:
		case EXPR_DEREF:
		case EXPR_IDENT:
		case EXPR_THIS:
			result->type = EXPR_CONFIG;
			result->value.config = lookup_node(expr, config, root, config);
			return result->value.config != NULL;
		default:
			return t3_false;
	}
}

static t3_config_type_t operand_type(const expr_node_t *expression) {
	switch (expression->type) {
		case EXPR_STRING_CONST:
			return T3_CONFIG_STRING;
		case EXPR_INT_CONST:
			return T3_CONFIG_INT;
		case EXPR_NUMBER_CONST:
			return T3_CONFIG_NUMBER;
		case EXPR_BOOL_CONST:
			return T3_CONFIG_BOOL;
		case EXPR_CONFIG:
			return expression->value.config->type;
		default:
			return T3_CONFIG_NONE;
	}
}

static const char *get_string_operand(const expr_node_t *expression) {
	return expression->type == EXPR_CONFIG ? expression->value.config->value.string : expression->value.string;
}

static t3_bool get_bool_operand(const expr_node_t *expression) {
	return expression->type == EXPR_CONFIG ? expression->value.config->value.boolean : expression->value.boolean;
}

static t3_config_int_t get_int_operand(const expr_node_t *expression) {
	return expression->type == EXPR_CONFIG ? expression->value.config->value.integer : expression->value.integer;
}

static double get_number_operand(const expr_node_t *expression) {
	return expression->type == EXPR_CONFIG ? expression->value.config->value.number : expression->value.number;
}

t3_bool _t3_config_evaluate_expr(const expr_node_t *expression, const t3_config_t *config, const t3_config_t *root) {
	t3_config_type_t type;
	expr_node_t resolved_nodes[2];

	switch (expression->type) {
		case EXPR_TOP:
			return _t3_config_evaluate_expr(expression->value.operand[0], config, root);
		case EXPR_AND:
			return _t3_config_evaluate_expr(expression->value.operand[0], config, root) &&
				_t3_config_evaluate_expr(expression->value.operand[1], config, root);
		case EXPR_OR:
			return _t3_config_evaluate_expr(expression->value.operand[0], config, root) ||
				_t3_config_evaluate_expr(expression->value.operand[1], config, root);
		case EXPR_XOR:
			return _t3_config_evaluate_expr(expression->value.operand[0], config, root) ^
				_t3_config_evaluate_expr(expression->value.operand[1], config, root);
		case EXPR_NOT:
			return !_t3_config_evaluate_expr(expression->value.operand[0], config, root);

		case EXPR_PATH:
		case EXPR_DEREF:
		case EXPR_IDENT:
		case EXPR_THIS:
			return lookup_node(expression, config, root, config) != NULL;

#define COMPARE(expr_type, operator) \
		case expr_type: \
			if (!resolve(expression->value.operand[0], config, root, &resolved_nodes[0]) || \
					!resolve(expression->value.operand[1], config, root, &resolved_nodes[1])) \
				return t3_false; \
			type = operand_type(&resolved_nodes[0]); \
			if (type != operand_type(&resolved_nodes[1])) \
				return t3_false; \
			if (type == T3_CONFIG_INT) \
				return get_int_operand(&resolved_nodes[0]) operator get_int_operand(&resolved_nodes[1]); \
			else if (type == T3_CONFIG_NUMBER) \
				return get_number_operand(&resolved_nodes[0]) operator get_number_operand(&resolved_nodes[1]); \
			return t3_false;
		COMPARE(EXPR_LT, <)
		COMPARE(EXPR_LE, <=)
		COMPARE(EXPR_GT, >)
		COMPARE(EXPR_GE, >=)

		case EXPR_NE:
		case EXPR_EQ:
			if (!resolve(expression->value.operand[0], config, root, &resolved_nodes[0]) ||
					!resolve(expression->value.operand[1], config, root, &resolved_nodes[1]))
				return t3_false;
			type = operand_type(&resolved_nodes[0]);
			if (type != operand_type(&resolved_nodes[1]))
				return t3_false;

			if (type == T3_CONFIG_STRING) {
				return (strcmp(get_string_operand(&resolved_nodes[0]), get_string_operand(&resolved_nodes[1])) == 0) ^
					(expression->type == EXPR_NE);
			} else if (type == T3_CONFIG_BOOL) {
				return (get_bool_operand(&resolved_nodes[0]) == get_bool_operand(&resolved_nodes[1])) ^
					(expression->type == EXPR_NE);
			} else if (type == T3_CONFIG_INT) {
				return (get_int_operand(&resolved_nodes[0]) == get_int_operand(&resolved_nodes[1])) ^
					(expression->type == EXPR_NE);
			} else if (type == T3_CONFIG_NUMBER) {
				return (get_number_operand(&resolved_nodes[0]) == get_number_operand(&resolved_nodes[1])) ^
					(expression->type == EXPR_NE);
			}
			return t3_false;
		default:
			return t3_false;
	}
}




static const t3_config_t *lookup_node_meta(const expr_node_t *expr, const t3_config_t *config,
	const t3_config_t *root, const t3_config_t *base, t3_bool *success)
{
	switch (expr->type) {
		case EXPR_PATH_ROOT:
			return root;
		case EXPR_IDENT: {
			t3_config_t *allowed_keys;
			const char *type;

			if ((allowed_keys = t3_config_get(config, "allowed-keys")) != NULL) {
				t3_config_t *result = t3_config_get(allowed_keys, expr->value.string);
				if (result == NULL) {
					*success = t3_false;
					return NULL;
				}

				type = t3_config_get_string(t3_config_get(result, "type"));
				if (_t3_config_str2type(type) == T3_CONFIG_NONE)
					result = t3_config_get(t3_config_get(root, "types"), type);

				return result;
			} else if ((type = t3_config_get_string(t3_config_get(config, "item-type"))) != NULL) {
				return t3_config_get(t3_config_get(root, "types"), type);
			}
			*success = t3_false;
			return NULL;
		}
		case EXPR_PATH:
			return lookup_node_meta(expr->value.operand[1],
				lookup_node_meta(expr->value.operand[0], config, root, base, success),
				root, base, success);
		case EXPR_DEREF:
			return NULL;
		default:
			*success = t3_false;
			return NULL;
	}
}

static t3_config_type_t operand_type_meta(const expr_node_t *expression, const t3_config_t *config, const t3_config_t *root) {
	t3_config_t *allowed_keys, *key;
	const char *type;

	switch (expression->type) {
		case EXPR_STRING_CONST:
			return T3_CONFIG_STRING;
		case EXPR_INT_CONST:
			return T3_CONFIG_INT;
		case EXPR_NUMBER_CONST:
			return T3_CONFIG_NUMBER;
		case EXPR_BOOL_CONST:
			return T3_CONFIG_BOOL;
		case EXPR_IDENT:
			if ((allowed_keys = t3_config_get(config, "allowed-keys")) != NULL) {
				if ((key = t3_config_get(allowed_keys, expression->value.string)) == NULL)
					return T3_CONFIG_NONE;
				if ((type = t3_config_get_string(t3_config_get(key, "type"))) == NULL)
					return T3_CONFIG_NONE;
			} else if ((type = t3_config_get_string(t3_config_get(config, "item-type"))) == NULL) {
				return T3_CONFIG_NONE;
			}
			return _t3_config_str2type(type);
		case EXPR_PATH: {
			t3_bool success = t3_true;
			const t3_config_t *context = lookup_node_meta(expression->value.operand[0], config, root, config, &success);
			if (!success)
				return T3_CONFIG_NONE;
			return operand_type_meta(expression->value.operand[1], context, root);
		}
		case EXPR_DEREF:
			return T3_CONFIG_ANY;
		case EXPR_THIS:
			if ((type = t3_config_get_string(t3_config_get(config, "type"))) == NULL)
				return T3_CONFIG_NONE;
			return _t3_config_str2type(type);
		default:
			return T3_CONFIG_NONE;
	}
}

t3_bool _t3_config_validate_expr(const expr_node_t *expression, const t3_config_t *config, const t3_config_t *root) {
	switch (expression->type) {
		case EXPR_AND:
		case EXPR_OR:
		case EXPR_XOR:
			return _t3_config_validate_expr(expression->value.operand[0], config, root) &&
				_t3_config_validate_expr(expression->value.operand[1], config, root);
		case EXPR_TOP:
		case EXPR_NOT:
			return _t3_config_validate_expr(expression->value.operand[0], config, root);

		case EXPR_PATH:
		case EXPR_IDENT: {
			t3_bool success = t3_true;
			lookup_node_meta(expression, config, root, config, &success);
			return success;
		}

		case EXPR_PATH_ROOT:
		case EXPR_THIS:
			return t3_false;
		case EXPR_DEREF:
			return t3_true;

		case EXPR_LT:
		case EXPR_LE:
		case EXPR_GT:
		case EXPR_GE:
		case EXPR_EQ:
		case EXPR_NE: {
			t3_config_type_t type[2] = {
				operand_type_meta(expression->value.operand[0], config, root),
				operand_type_meta(expression->value.operand[1], config, root)
			};

			if (type[0] == T3_CONFIG_NONE || type[1] == T3_CONFIG_NONE)
				return t3_false;

			if (type[0] == T3_CONFIG_ANY || type[1] == T3_CONFIG_ANY)
				return t3_true;

			if (type[0] != type[1])
				return t3_false;

			if (type[0] == T3_CONFIG_STRING || type[0] == T3_CONFIG_BOOL)
				return expression->type == EXPR_EQ || expression->type == EXPR_NE;

			if (type[0] == T3_CONFIG_INT || type[0] == T3_CONFIG_NUMBER)
				return t3_true;
			return t3_false;
		}
		case EXPR_BOOL_CONST:
		case EXPR_INT_CONST:
		case EXPR_NUMBER_CONST:
		case EXPR_STRING_CONST:
			return t3_true;

		default:
			return t3_false;
	}
}

void _t3_config_delete_expr(expr_node_t *expr) {
	switch (expr->type) {
		case EXPR_TOP:
		case EXPR_AND:
		case EXPR_OR:
		case EXPR_XOR:
		case EXPR_LT:
		case EXPR_LE:
		case EXPR_GT:
		case EXPR_GE:
		case EXPR_EQ:
		case EXPR_NE:
		case EXPR_PATH:
			_t3_config_delete_expr(expr->value.operand[0]);
			_t3_config_delete_expr(expr->value.operand[1]);
			break;
		case EXPR_NOT:
		case EXPR_DEREF:
			_t3_config_delete_expr(expr->value.operand[0]);
			break;
		case EXPR_STRING_CONST:
		case EXPR_IDENT:
			free(expr->value.string);
			break;
		default:
			break;
	}
	free(expr);
}
