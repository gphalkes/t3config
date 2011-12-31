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
		case EXPR_LENGTH: {
			const t3_config_t *list;
			if (expr->value.operand[0] == NULL) {
				list = config;
			} else if (expr->value.operand[0]->type == EXPR_LIST) {
				const expr_node_t *list_item;
				result->type = EXPR_INT_CONST;
				result->value.integer = 0;
				for (list_item = expr->value.operand[0]; list_item != NULL; list_item = list_item->value.operand[1]) {
					if (_t3_config_evaluate_expr(list_item->value.operand[0], config, root))
						result->value.integer++;
				}
				return t3_true;
			} else {
				list = lookup_node(expr->value.operand[0], config, root, config);
			}

			if (list->type != T3_CONFIG_LIST && list->type != T3_CONFIG_PLIST)
				return t3_false;

			result->type = EXPR_INT_CONST;
			result->value.integer = 0;
			for (list = t3_config_get(list, NULL); list != NULL; list = list->next, result->value.integer++) {}

			return t3_true;
		}
		default:
			return t3_false;
	}
}

static t3_config_type_t operand_type(const expr_node_t *expr) {
	switch (expr->type) {
		case EXPR_STRING_CONST:
			return T3_CONFIG_STRING;
		case EXPR_INT_CONST:
			return T3_CONFIG_INT;
		case EXPR_NUMBER_CONST:
			return T3_CONFIG_NUMBER;
		case EXPR_BOOL_CONST:
			return T3_CONFIG_BOOL;
		case EXPR_CONFIG:
			return expr->value.config->type;
		default:
			return T3_CONFIG_NONE;
	}
}

static const char *get_string_operand(const expr_node_t *expr) {
	return expr->type == EXPR_CONFIG ? expr->value.config->value.string : expr->value.string;
}

static t3_bool get_bool_operand(const expr_node_t *expr) {
	return expr->type == EXPR_CONFIG ? expr->value.config->value.boolean : expr->value.boolean;
}

static t3_config_int_t get_int_operand(const expr_node_t *expr) {
	return expr->type == EXPR_CONFIG ? expr->value.config->value.integer : expr->value.integer;
}

static double get_number_operand(const expr_node_t *expr) {
	return expr->type == EXPR_CONFIG ? expr->value.config->value.number : expr->value.number;
}

t3_bool _t3_config_evaluate_expr(const expr_node_t *expr, const t3_config_t *config, const t3_config_t *root) {
	t3_config_type_t type;
	expr_node_t resolved_nodes[2];

	switch (expr->type) {
		case EXPR_TOP:
			return _t3_config_evaluate_expr(expr->value.operand[0], config, root);
		case EXPR_AND:
			return _t3_config_evaluate_expr(expr->value.operand[0], config, root) &&
				_t3_config_evaluate_expr(expr->value.operand[1], config, root);
		case EXPR_OR:
			return _t3_config_evaluate_expr(expr->value.operand[0], config, root) ||
				_t3_config_evaluate_expr(expr->value.operand[1], config, root);
		case EXPR_XOR:
			return _t3_config_evaluate_expr(expr->value.operand[0], config, root) ^
				_t3_config_evaluate_expr(expr->value.operand[1], config, root);
		case EXPR_NOT:
			return !_t3_config_evaluate_expr(expr->value.operand[0], config, root);

		case EXPR_PATH:
		case EXPR_DEREF:
		case EXPR_IDENT:
		case EXPR_THIS:
			return lookup_node(expr, config, root, config) != NULL;

#define COMPARE(expr_type, operator) \
		case expr_type: \
			if (!resolve(expr->value.operand[0], config, root, &resolved_nodes[0]) || \
					!resolve(expr->value.operand[1], config, root, &resolved_nodes[1])) \
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
			if (!resolve(expr->value.operand[0], config, root, &resolved_nodes[0]) ||
					!resolve(expr->value.operand[1], config, root, &resolved_nodes[1]))
				return t3_false;
			type = operand_type(&resolved_nodes[0]);
			if (type != operand_type(&resolved_nodes[1]))
				return t3_false;

			if (type == T3_CONFIG_STRING) {
				return (strcmp(get_string_operand(&resolved_nodes[0]), get_string_operand(&resolved_nodes[1])) == 0) ^
					(expr->type == EXPR_NE);
			} else if (type == T3_CONFIG_BOOL) {
				return (get_bool_operand(&resolved_nodes[0]) == get_bool_operand(&resolved_nodes[1])) ^
					(expr->type == EXPR_NE);
			} else if (type == T3_CONFIG_INT) {
				return (get_int_operand(&resolved_nodes[0]) == get_int_operand(&resolved_nodes[1])) ^
					(expr->type == EXPR_NE);
			} else if (type == T3_CONFIG_NUMBER) {
				return (get_number_operand(&resolved_nodes[0]) == get_number_operand(&resolved_nodes[1])) ^
					(expr->type == EXPR_NE);
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
		case EXPR_PATH: {
			/* Take into account that when NULL is returned, this may mean that we simply
			   can't resolve the reference at this time. */
			const t3_config_t *sub_config = lookup_node_meta(expr->value.operand[0], config, root, base, success);
			return sub_config == NULL ? NULL : lookup_node_meta(expr->value.operand[1], sub_config, root, base, success);
		}
		case EXPR_DEREF:
			return NULL;
		default:
			*success = t3_false;
			return NULL;
	}
}

static t3_config_type_t operand_type_meta(const expr_node_t *expr, const t3_config_t *config, const t3_config_t *root) {
	t3_config_t *allowed_keys, *key;
	const char *type;

	switch (expr->type) {
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
				if ((key = t3_config_get(allowed_keys, expr->value.string)) == NULL)
					return T3_CONFIG_NONE;
				type = t3_config_get_string(t3_config_get(key, "type"));
			} else {
				type = t3_config_get_string(t3_config_get(config, "item-type"));
			}
			return _t3_config_str2type(type);
		case EXPR_PATH: {
			t3_bool success = t3_true;
			const t3_config_t *context = lookup_node_meta(expr->value.operand[0], config, root, config, &success);
			if (!success)
				return T3_CONFIG_NONE;
			else if (context == NULL)
				return T3_CONFIG_ANY;
			return operand_type_meta(expr->value.operand[1], context, root);
		}
		case EXPR_DEREF:
			return T3_CONFIG_ANY;
		case EXPR_THIS:
			return _t3_config_str2type(t3_config_get_string(t3_config_get(config, "type")));
		case EXPR_LENGTH: {
			t3_config_type_t type_int;
			if (expr->value.operand[0] == NULL)
				type_int = _t3_config_str2type(t3_config_get_string(t3_config_get(config, "type")));
			else if (expr->value.operand[0]->type == EXPR_LIST)
				return _t3_config_validate_expr(expr->value.operand[0], config, root) ? T3_CONFIG_INT : T3_CONFIG_NONE;
			else
				type_int = operand_type_meta(expr->value.operand[0], config, root);

			if (type_int != T3_CONFIG_LIST && (int) type_int != T3_CONFIG_ANY)
				return T3_CONFIG_NONE;

			return T3_CONFIG_INT;
		}
		default:
			return T3_CONFIG_NONE;
	}
}

t3_bool _t3_config_validate_expr(const expr_node_t *expr, const t3_config_t *config, const t3_config_t *root) {
	switch (expr->type) {
		case EXPR_AND:
		case EXPR_OR:
		case EXPR_XOR:
			return _t3_config_validate_expr(expr->value.operand[0], config, root) &&
				_t3_config_validate_expr(expr->value.operand[1], config, root);
		case EXPR_LIST: {
			const expr_node_t *list_item;
			for (list_item = expr; list_item != NULL; list_item = list_item->value.operand[1]) {
				if (!_t3_config_validate_expr(list_item->value.operand[0], config, root))
					return t3_false;
			}
			return t3_true;
		}
		case EXPR_TOP:
		case EXPR_NOT:
			return _t3_config_validate_expr(expr->value.operand[0], config, root);

		case EXPR_PATH:
		case EXPR_IDENT: {
			t3_bool success = t3_true;
			lookup_node_meta(expr, config, root, config, &success);
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
				operand_type_meta(expr->value.operand[0], config, root),
				operand_type_meta(expr->value.operand[1], config, root)
			};

			if (type[0] == T3_CONFIG_NONE || type[1] == T3_CONFIG_NONE)
				return t3_false;

			if ((int) type[0] == T3_CONFIG_ANY || (int) type[1] == T3_CONFIG_ANY)
				return t3_true;

			if (type[0] != type[1])
				return t3_false;

			if (type[0] == T3_CONFIG_STRING || type[0] == T3_CONFIG_BOOL)
				return expr->type == EXPR_EQ || expr->type == EXPR_NE;

			if (type[0] == T3_CONFIG_INT || type[0] == T3_CONFIG_NUMBER)
				return t3_true;
			return t3_false;
		}

		default:
			return t3_false;
	}
}

void _t3_config_delete_expr(expr_node_t *expr) {
	if (expr == NULL)
		return;

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
		case EXPR_LIST:
			_t3_config_delete_expr(expr->value.operand[0]);
			_t3_config_delete_expr(expr->value.operand[1]);
			break;
		case EXPR_NOT:
		case EXPR_DEREF:
		case EXPR_LENGTH:
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
