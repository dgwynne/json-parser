/* */

/*
 * Copyright (c) 2014 David Gwynne <david@gwynne.id.au>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "json_parser.h"

#ifndef JSON_PARSER_DEFAULT_DEPTH
#define JSON_PARSER_DEFAULT_DEPTH 16
#endif

#define JSON_PARSER_NONE	' '
#define JSON_PARSER_OBJECT	'{'
#define JSON_PARSER_ARRAY	'['

enum json_parser_state {
	s_dead,

	s_init,
	s_next,
	s_delim,
	s_done,

	s_null_N,
	s_null_NU,
	s_null_NUL,

	s_true_T,
	s_true_TR,
	s_true_TRU,

	s_false_F,
	s_false_FA,
	s_false_FAL,
	s_false_FALS,

	s_number_negative,
	s_number_zero,
	s_number,
	s_number_point,
	s_number_decimals,
	s_number_e,
	s_number_e_sign,
	s_number_e_digits,

	s_string_start,
	s_string_mark,
	s_string_escape,
	s_string_u,
	s_string_u0,
	s_string_u00,
	s_string_u00X,
	s_string,

	s_object_start,
	s_object_next,
	s_object,
	s_object_key_start,
	s_object_key_mark,
	s_object_key_escape,
	s_object_key_u,
	s_object_key_u0,
	s_object_key_u00,
	s_object_key_u00X,
	s_object_key,
	s_object_key_end
};

struct json_parser {
	void *			jp_ctx;
	char *			jp_stack;
	enum json_parser_state	jp_state;
	int			jp_depth;
	int			jp_stackdepth;
	char			jp_uchar;
};

static int	json_parser_push(struct json_parser *,
		    const struct json_parser_settings *, char);
static int	json_parser_pop(struct json_parser *,
		    const struct json_parser_settings *, char);
static int	json_parser_push_object(struct json_parser *,
		    const struct json_parser_settings *);
static int	json_parser_pop_object(struct json_parser *,
		    const struct json_parser_settings *);
static int	json_parser_push_array(struct json_parser *,
		    const struct json_parser_settings *);
static int	json_parser_pop_array(struct json_parser *,
		    const struct json_parser_settings *);

static int	json_parser_uchar(u_char);

struct json_parser *
json_parser_new(void *ctx)
{
	struct json_parser *jp;

	jp = malloc(sizeof(*jp));
	if (jp == NULL)
		return (NULL);

	memset(jp, 0, sizeof(*jp));
	jp->jp_state = s_init;
	jp->jp_ctx = ctx;
	jp->jp_stackdepth = JSON_PARSER_DEFAULT_DEPTH;
	jp->jp_stack = malloc(jp->jp_stackdepth);
	if (jp->jp_stack == NULL) {
		free(jp);
		return (NULL);
	}
	jp->jp_depth = -1;

	return (jp);
}

void
json_parser_del(struct json_parser *jp)
{
	free(jp->jp_stack);
	free(jp);
}

static int
json_parser_top(struct json_parser *jp)
{
	return (jp->jp_depth >= 0 ?
	    jp->jp_stack[jp->jp_depth] :
	    JSON_PARSER_NONE);
}

static int
json_parser_push(struct json_parser *jp,
    const struct json_parser_settings *settings, char type)
{
	char *stack = jp->jp_stack;

	if (jp->jp_depth + 1 >= jp->jp_stackdepth) {
		stack = realloc(jp->jp_stack, jp->jp_stackdepth + 16);
		if (stack == NULL)
			return (-1);

		jp->jp_stack = stack;
		jp->jp_stackdepth += 16;
	}

	stack[++jp->jp_depth] = type;

	return (0);
}

static int
json_parser_pop(struct json_parser *jp,
    const struct json_parser_settings *settings, char type)
{
	int depth = jp->jp_depth--;

	if (depth < 0 || jp->jp_stack[depth] != type) {
		jp->jp_state = s_dead;
		errno = EINVAL;
		return (-1);
	}

	return (0);
}

static int
json_parser_push_object(struct json_parser *jp,
    const struct json_parser_settings *settings)
{
	if (json_parser_push(jp, settings, JSON_PARSER_OBJECT) == -1)
		return (-1);

	return (settings->on_object_start(jp->jp_ctx));
}

static int
json_parser_pop_object(struct json_parser *jp,
    const struct json_parser_settings *settings)
{
	if (json_parser_pop(jp, settings, JSON_PARSER_OBJECT) == -1)
		return (-1);

	return (settings->on_object_end(jp->jp_ctx));
}

static int
json_parser_push_array(struct json_parser *jp,
    const struct json_parser_settings *settings)
{
	if (json_parser_push(jp, settings, JSON_PARSER_ARRAY) == -1)
		return (-1);

	return (settings->on_array_start(jp->jp_ctx));
}

static int
json_parser_pop_array(struct json_parser *jp,
    const struct json_parser_settings *settings)
{
	if (json_parser_pop(jp, settings, JSON_PARSER_ARRAY) == -1)
		return (-1);

	return (settings->on_array_end(jp->jp_ctx));
}

static int
json_parser_uchar(u_char digit)
{
	if (digit >= '0' && digit <= '9')
		return (digit - '0');
	if (digit >= 'a' && digit <= 'f')
		return (digit - 'a');
	if (digit >= 'A' && digit <= 'F')
		return (digit - 'A');

	return (-1);
}

static int
json_parser_echar(u_char digit)
{
	switch (digit) {
	case 'b':
		return ('\b');
	case 't':
		return ('\t');
	case 'n':
		return ('\n');
	case 'f':
		return ('\f');
	case 'r':
		return ('\r');
	case '\"':
		return ('\"');
	case '/':
		return ('/');
	case '\\':
		return ('\\');
	case 'u':
		return ('u');
	default:
		return (-1);
	}
}

static int
json_parser_on_number(struct json_parser *jp,
    const struct json_parser_settings *settings,
    const u_char *mark, const u_char *data)
{
	if (mark == data)
		return (0);

	return (settings->on_number(jp->jp_ctx, mark, data - mark));
}

static inline enum json_parser_state
json_parser_next(struct json_parser *jp)
{
	switch (json_parser_top(jp)) {
	case JSON_PARSER_ARRAY:
		return (s_next);
	case JSON_PARSER_OBJECT:
		return (s_object_next);
	default:
		return (s_done);
	}
}

static int
json_parser_number_end(struct json_parser *jp,
    const struct json_parser_settings *settings,
    const u_char *mark, const u_char *data)
{
	int top;

	if (isspace(*data)) {
		if (json_parser_on_number(jp, settings,
		    mark, data) != 0)
			return (-1);
		jp->jp_state = json_parser_next(jp);
		return (1);
	}

	switch (*data) {
	case '}':
		if (json_parser_on_number(jp, settings, mark, data) != 0)
			return (-1);
		if (json_parser_pop_object(jp, settings) != 0)
			return (-1);
		jp->jp_state = json_parser_next(jp);
		return (1);
	case ']':
		if (json_parser_on_number(jp, settings, mark, data) != 0)
			return (-1);
		if (json_parser_pop_array(jp, settings) != 0)
			return (-1);
		jp->jp_state = json_parser_next(jp);
		return (1);
	case ',':
		top = json_parser_top(jp);
		if (top == JSON_PARSER_NONE) {
			jp->jp_state = s_dead;
			return (-1);
		}
		if (json_parser_on_number(jp, settings, mark, data) != 0)
			return (-1);
		if (settings->on_separator(jp->jp_ctx) != 0)
			return (-1);
		jp->jp_state = top == JSON_PARSER_OBJECT ? s_object : s_delim;
		return (1);
	}

	return (0);
}

#include <stdio.h>

size_t
json_parser_exec(struct json_parser *jp,
    const struct json_parser_settings *settings,
    const void *buf, size_t len)
{
	const u_char *data = buf;
	const u_char *end;
	const u_char *mark = NULL;
	u_char uchar;
	int rv;

	if (jp->jp_state == s_dead)
		return (0);

	if (len == 0) {
		/* end of input */
		switch (jp->jp_state) {
		case s_done:

		case s_number_zero:
		case s_number:
		case s_number_decimals:
		case s_number_e_digits:
			return (0);
		default:
			jp->jp_state = s_dead;
			return (1);
		}
	}

	switch (jp->jp_state) {
	case s_number_negative:
	case s_number_zero:
	case s_number:
	case s_number_point:
	case s_number_decimals:
	case s_number_e:
	case s_number_e_sign:
	case s_number_e_digits:
	case s_string:
	case s_object_key:
		mark = data;
		break;

	default:
		break;
	}

	for (end = data + len; data < end; data++) {
		switch (jp->jp_state) {
		case s_delim:
			switch (*data) {
			case '}':
			case ']':
				goto die;
			default:
				break;
			}
			/* FALLTHROUGH */
		case s_init:
			switch (*data) {
			case '{':
				if (json_parser_push_object(jp, settings) != 0)
					goto onerr;

				jp->jp_state = s_object_start;
				break;
			case '}':
				if (json_parser_pop_object(jp, settings) != 0)
					goto onerr;

				jp->jp_state = json_parser_next(jp);
				break;

			case '[':
				if (json_parser_push_array(jp, settings) == -1)
					goto onerr;

				jp->jp_state = s_init;
				break;
			case ']':
				if (json_parser_pop_array(jp, settings) != 0)
					goto onerr;

				jp->jp_state = json_parser_next(jp);
				break;

			case 'n':
				jp->jp_state = s_null_N;
				break;
			case 't':
				jp->jp_state = s_true_T;
				break;
			case 'f':
				jp->jp_state = s_false_F;
				break;

			case '\"':
				jp->jp_state = s_string_start;
				break;

			case '-':
				jp->jp_state = s_number_negative;
				mark = data;
				break;
			case '0':
				jp->jp_state = s_number_zero;
				mark = data;
				break;
			default:
				if (isspace(*data))
					break;

				if (isdigit(*data)) {
					jp->jp_state = s_number;
					mark = data;
					break;
				}

				goto die;
			}
			break;
		case s_next:
			if (isspace(*data))
				break;

			switch (*data) {
			case ',':
				if (settings->on_separator(jp->jp_ctx) != 0)
					goto onerr;

				jp->jp_state = s_delim;
				break;
			case '}':
				if (json_parser_pop_object(jp, settings) != 0)
					goto onerr;

				jp->jp_state = json_parser_next(jp);
				break;
			case ']':
				if (json_parser_pop_array(jp, settings) != 0)
					goto onerr;

				jp->jp_state = json_parser_next(jp);
				break;
			default:
				goto die;
			}
			break;
		case s_done:
			if (isspace(*data))
				break;

			goto die;

		case s_object_start:
			if (isspace(*data))
				break;

			switch (*data) {
			case '\"':
				jp->jp_state = s_object_key_start;
				break;
			case '}':
				if (json_parser_pop_object(jp, settings) != 0)
					goto onerr;

				jp->jp_state = json_parser_next(jp);
				break;
			default:
				goto die;
			}

			break;
		case s_object_next:
			if (isspace(*data))
				break;

			switch (*data) {
			case ',':
				if (settings->on_separator(jp->jp_ctx) != 0)
					goto onerr;

				jp->jp_state = s_object;
				break;
			case '}':
				if (json_parser_pop_object(jp, settings) != 0)
					goto onerr;

				jp->jp_state = json_parser_next(jp);
				break;
			default:
				goto die;
			}

			break;
		case s_object:
			if (isspace(*data))
				break;

			if (*data != '\"')
				goto die;

			jp->jp_state = s_object_key_start;
			break;

		case s_object_key_start:
			if (*data == '\"') {
				/* callback on empty key */
				if (settings->on_object_key(jp->jp_ctx,
				    NULL, 0) != 0)
					goto onerr;
				jp->jp_state = s_object_key_end;
				break;
			}
			/* FALLTHROUGH */
		case s_object_key_mark:
			mark = data;
			jp->jp_state = s_object_key;
			/* FALLTHROUGHT */
		case s_object_key:
			switch (*data) {
			case '\\':
				if (data != mark) {
					if (settings->on_object_key(jp->jp_ctx,
					    mark, data - mark) != 0)
						goto onerr;
				}

				jp->jp_state = s_object_key_escape;
				break;
			case '\"':
				if (data != mark) {
					if (settings->on_object_key(jp->jp_ctx,
					    mark, data - mark) != 0)
						goto onerr;
				}

				jp->jp_state = s_object_key_end;
				break;
			default:
				if (!isprint(*data))
					goto die;

				break;
			}
			break;
		case s_object_key_escape:
			rv = json_parser_echar(*data);
			switch (rv) {
			case 'u':
				jp->jp_state = s_object_key_u;
				continue;
			case -1:
				goto die;
			default:
				uchar = rv;
				if (settings->on_object_key(jp->jp_ctx,
				    &uchar, 1) != 0)
					goto die;

				jp->jp_state = s_object_key_mark;
				break;
			}
			break;
		case s_object_key_u00X:
			rv = json_parser_uchar(*data);
			if (rv == -1)
				goto die;

			jp->jp_uchar |= rv;

			if (settings->on_object_key(jp->jp_ctx,
			    &jp->jp_uchar, 1) != 0)
				goto die;

			jp->jp_state = s_object_key_mark;
			break;

		case s_object_key_end:
			if (isspace(*data))
				continue;
			if (*data != ':')
				goto die;

			jp->jp_state = s_delim;
			break;

		case s_string_start:
			if (*data == '\"') {
				/* callback on empty string */
				if (settings->on_string(jp->jp_ctx,
				    NULL, 0) != 0)
					goto onerr;
				jp->jp_state = json_parser_next(jp);
				break;
			}
			/* FALLTHROUGH */
		case s_string_mark:
			mark = data;
			jp->jp_state = s_string;
			/* FALLTHROUGH */
		case s_string:
			switch (*data) {
			case '\\':
				if (data != mark) {
					if (settings->on_string(jp->jp_ctx,
					    mark, data - mark) != 0)
						goto onerr;
				}

				jp->jp_state = s_string_escape;
				break;
			case '\"':
				if (data != mark) {
					if (settings->on_string(jp->jp_ctx,
					    mark, data - mark) != 0)
						goto onerr;
				}

				jp->jp_state = json_parser_next(jp);
				break;
			default:
				if (!isprint(*data))
					goto die;

				break;
			}
			break;

		case s_string_escape:
			rv = json_parser_echar(*data);
			switch (rv) {
			case 'u':
				jp->jp_state = s_string_u;
				continue;
			case -1:
				goto die;
			default:
				uchar = rv;
				if (settings->on_string(jp->jp_ctx,
				    &uchar, 1) != 0)
					goto die;

				jp->jp_state = s_string_mark;
				break;
			}
			break;

		case s_string_u:
		case s_object_key_u:
		case s_string_u0:
		case s_object_key_u0:
			if (*data != '0')
				goto die;
			jp->jp_state++;
			break;
		case s_string_u00:
		case s_object_key_u00:
			rv = json_parser_uchar(*data);
			if (rv == -1)
				goto die;

			jp->jp_uchar = rv << 4;

			jp->jp_state++;
			break;
		case s_string_u00X:
			rv = json_parser_uchar(*data);
			if (rv == -1)
				goto die;

			jp->jp_uchar |= rv;

			if (settings->on_string(jp->jp_ctx,
			    &jp->jp_uchar, 1) != 0)
				goto die;

			jp->jp_state = s_string_mark;
			break;

		case s_number_negative:
			if (*data == '0')
				jp->jp_state = s_number_zero;
			else if (isdigit(*data))
				jp->jp_state = s_number;
			else
				goto die;
			break;

		case s_number_zero:
			rv = json_parser_number_end(jp, settings, mark, data);
			if (rv == -1)
				goto onerr;
			if (rv == 1)
				break;

			if (*data == '.')
				jp->jp_state = s_number_point;
			else
				goto die;
			break;

		case s_number:
			rv = json_parser_number_end(jp, settings, mark, data);
			if (rv == -1)
				goto onerr;
			if (rv == 1)
				break;

			if (isdigit(*data))
				break;

			switch (*data) {
			case '.':
				jp->jp_state = s_number_point;
				break;
			case 'e':
			case 'E':
				jp->jp_state = s_number_e;
				break;
			default:
				goto die;
			}
			break;

		case s_number_point:
			if (isdigit(*data))
				jp->jp_state = s_number_decimals;
			else
				goto die;
			break;

		case s_number_decimals:
			rv = json_parser_number_end(jp, settings, mark, data);
			if (rv == -1)
				goto onerr;
			if (rv == 1)
				break;

			if (isdigit(*data))
				break;

			switch (*data) {
			case 'e':
			case 'E':
				jp->jp_state = s_number_e;
				break;
			default:
				goto die;
			}
			break;

		case s_number_e:
			if (isdigit(*data)) {
				jp->jp_state = s_number_e_digits;
				break;
			}

			switch (*data) {
			case '+':
			case '-':
				jp->jp_state = s_number_e_sign;
				break;
			default:
				goto die;
			}
			break;

		case s_number_e_sign:
			if (isdigit(*data)) {
				jp->jp_state = s_number_e_digits;
				break;
			}
			goto die;

		case s_number_e_digits:
			rv = json_parser_number_end(jp, settings, mark, data);
			if (rv == -1)
				goto onerr;
			if (rv == 1)
				break;

			if (isdigit(*data))
				break;

			goto die;

		case s_null_N:
			if (*data != 'u')
				goto die;

			jp->jp_state = s_null_NU;
			break;
		case s_null_NU:
			if (*data != 'l')
				goto die;

			jp->jp_state = s_null_NUL;
			break;
		case s_null_NUL:
			if (*data != 'l')
				goto die;

			jp->jp_state = json_parser_next(jp);
			if (settings->on_null(jp->jp_ctx) != 0)
				goto onerr;
			break;

		case s_true_T:
			if (*data != 'r')
				goto die;

			jp->jp_state = s_true_TR;
			break;
		case s_true_TR:
			if (*data != 'u')
				goto die;

			jp->jp_state = s_true_TRU;
			break;
		case s_true_TRU:
			if (*data != 'e')
				goto die;

			jp->jp_state = json_parser_next(jp);
			if (settings->on_bool(jp->jp_ctx, 1) != 0)
				goto onerr;
			break;

		case s_false_F:
			if (*data != 'a')
				goto die;

			jp->jp_state = s_false_FA;
			break;
		case s_false_FA:
			if (*data != 'l')
				goto die;

			jp->jp_state = s_false_FAL;
			break;
		case s_false_FAL:
			if (*data != 's')
				goto die;

			jp->jp_state = s_false_FALS;
			break;
		case s_false_FALS:
			if (*data != 'e')
				goto die;

			jp->jp_state = json_parser_next(jp);
			if (settings->on_bool(jp->jp_ctx, 0) != 0)
				goto onerr;
			break;

		case s_dead:
			/* we should never end up here */
			goto onerr;
		}
	}

	switch (jp->jp_state) {
	case s_number_negative:
	case s_number_zero:
	case s_number:
	case s_number_decimals:
	case s_number_e:
	case s_number_e_sign:
	case s_number_e_digits:
		if (mark != data) {
			if (settings->on_number(jp->jp_ctx,
			    mark, data - mark) != 0)
				goto die;
		}
		break;

	case s_object_key:
		if (mark != data) {
			if (settings->on_object_key(jp->jp_ctx,
			    mark, data - mark) != 0)
				goto die;
		}
		break;
	case s_string:
		if (mark != data) {
			if (settings->on_string(jp->jp_ctx,
			    mark, data - mark) != 0)
				goto die;
		}
		break;
	default:
		break;
	}

	return (data - (u_char *)buf);

die:
	jp->jp_state = s_dead;
onerr:
	return (data - (u_char *)buf);
}

int
json_parser_isdead(struct json_parser *jp)
{
	return (jp->jp_state == s_dead);
}
