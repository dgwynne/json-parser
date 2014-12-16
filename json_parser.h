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

struct json_parser_settings {
	int	(*on_null)(void *);
	int	(*on_bool)(void *, int);
	int	(*on_number)(void *, const char *, size_t);
	int	(*on_string)(void *, const char *, size_t);
	int	(*on_object_start)(void *);
	int	(*on_object_key)(void *, const char *, size_t);
	int	(*on_object_end)(void *);
	int	(*on_array_start)(void *);
	int	(*on_array_end)(void *);
	int	(*on_separator)(void *);
};

struct json_parser;

struct json_parser *	json_parser_new(void *);
void			json_parser_del(struct json_parser *);

size_t			json_parser_exec(struct json_parser *,
			    const struct json_parser_settings *,
			    const void *, size_t);

int			json_parser_isdead(struct json_parser *);
