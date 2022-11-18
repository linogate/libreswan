/* cloning an address list, for libreswan
 *
 * Copyright (C) 2022 Andrew Cagney
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <https://www.gnu.org/licenses/gpl2.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <stdarg.h>

#include "lswalloc.h"
#include "passert.h"
#include "lswlog.h"		/* for dbg() */
#include "ip_address.h"

const ip_addresses empty_ip_addresses;

diag_t ttoaddresses_num(shunk_t input, const char *delims,
			const struct ip_info *afi,
			ip_addresses *output)
{
	*output = empty_ip_addresses;

	if (input.ptr == NULL) {
		return NULL;
	}

	dbg("%s() input: "PRI_SHUNK, __func__, pri_shunk(input));

	/*
	 * Pass 1: determine the number of tokens.
	 */
	unsigned nr_tokens = 0;
	shunk_t cursor = input;
	while (true) {
		shunk_t token = shunk_token(&cursor, NULL/*delim*/, delims);
		if (token.ptr == NULL) {
			break;
		}
		if (token.len == 0) {
			continue;
		}
		/* validate during first pass */
		ip_address tmp;
		err_t e = ttoaddress_num(token, afi, &tmp);
		if (e != NULL) {
			return diag(PRI_SHUNK" invalid, %s",
				    pri_shunk(token), e);
		}
		nr_tokens++;
	}
	if (nr_tokens == 0) {
		return NULL;
	}
	/*
	 * Allocate.
	 */
	dbg("%s() nr tokens %u", __func__, nr_tokens);
	output->len = nr_tokens;
	output->list = alloc_things(ip_address, nr_tokens, "addresses");

	/*
	 * pass 2: copy things over.
	 */
	cursor = input;
	ip_address *dst = output->list;
	while (true) {
		shunk_t token = shunk_token(&cursor, NULL/*delim*/, delims);
		if (token.ptr == NULL) {
			break;
		}
		if (token.len == 0) {
			continue;
		}
		passert(dst < output->list + output->len);
		err_t e = ttoaddress_num(token, afi, dst);
		passert(e == NULL);
		address_buf ab;
		dbg("%s() %s", __func__, str_address(dst, &ab));
		dst++;
	}
	passert(dst == output->list + output->len);
	return NULL;
}