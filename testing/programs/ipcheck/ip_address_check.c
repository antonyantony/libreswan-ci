/* ip_address tests, for libreswan
 *
 * Copyright (C) 2000  Henry Spencer.
 * Copyright (C) 2012 Paul Wouters <paul@libreswan.org>
 * Copyright (C) 2018 Andrew Cagney
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <https://www.gnu.org/licenses/lgpl-2.1.txt>.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 * License for more details.
 *
 */

#include <stdio.h>
#include <string.h>

#include "lswcdefs.h"		/* for elemsof() */
#include "constants.h"		/* for streq() */
#include "ip_address.h"
#include "jambuf.h"
#include "ipcheck.h"

static void check_shunk_to_address(void)
{
	static const struct test {
		int line;
		int family;
		const char *in;
		const char *cooked;
		bool requires_dns;
	} tests[] = {

		/* any/unspec */
		{ LN, 4, "0.0.0.0", "0.0.0.0", false, },
		{ LN, 6, "::", "::", false, },
		{ LN, 6, "0:0:0:0:0:0:0:0", "::", false, },

		/* local */
		{ LN, 4, "127.0.0.1", "127.0.0.1", false, },
		{ LN, 6, "::1", "::1", false, },
		{ LN, 6, "0:0:0:0:0:0:0:1", "::1", false, },

		/* mask - and buffer overflow */
		{ LN, 4, "255.255.255.255", "255.255.255.255", false, },
		{ LN, 6, "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", false, },

		/* all bytes and '/' */
		{ LN, 4, "1.2.3.4", "1.2.3.4", false, },
		{ LN, 6, "1:2:3:4:5:6:7:8", "1:2:3:4:5:6:7:8", false, },

		/* suppress leading zeros - 01 vs 1 */
		{ LN, 6, "0001:0012:0003:0014:0005:0016:0007:0018", "1:12:3:14:5:16:7:18", false, },
		/* drop leading 0:0: */
		{ LN, 6, "0:0:3:4:5:6:7:8", "::3:4:5:6:7:8", false, },
		/* drop middle 0:...:0 */
		{ LN, 6, "1:2:0:0:0:0:7:8", "1:2::7:8", false, },
		/* drop trailing :0..:0 */
		{ LN, 6, "1:2:3:4:5:0:0:0", "1:2:3:4:5::", false, },
		/* drop first 0:..:0 */
		{ LN, 6, "1:2:0:0:5:6:0:0", "1:2::5:6:0:0", false, },
		/* drop logest 0:..:0 */
		{ LN, 6, "0:0:3:0:0:0:7:8", "0:0:3::7:8", false, },
		/* need two 0 */
		{ LN, 6, "0:2:0:4:0:6:0:8", "0:2:0:4:0:6:0:8", false, },

		{ LN, 4, "www.libreswan.org", "188.127.201.229", .requires_dns = true, },
	};

	err_t err;

	for (size_t ti = 0; ti < elemsof(tests); ti++) {
		const struct test *t = &tests[ti];
		PRINT("%s '%s' -> cooked: %s dns: %s", pri_family(t->family), t->in,
		      t->cooked == NULL ? "ERROR" : t->cooked,
		      bool_str(t->requires_dns));

		const struct ip_info *type;
		ip_address a;

		/* NUMERIC/NULL */

		type = NULL;
		err = numeric_to_address(shunk1(t->in), type, &a);
		if (err != NULL) {
			if (t->cooked != NULL && !t->requires_dns) {
				FAIL("numeric_to_address(NULL) unexpecedly failed: %s", err);
			}
		} else if (t->requires_dns) {
			FAIL(" numeric_to_address(NULL) unexpecedly parsed a DNS address");
		} else if (t->cooked == NULL) {
			FAIL(" numeric_to_address(NULL) unexpecedly succeeded");
		} else {
			CHECK_TYPE(address_type(&a));
		}

		/* NUMERIC/TYPE */

		type = IP_TYPE(t->family);
		err = numeric_to_address(shunk1(t->in), type, &a);
		if (err != NULL) {
			if (!t->requires_dns) {
				FAIL(" numeric_to_address(type) unexpecedly failed: %s", err);
			}
		} else if (t->requires_dns) {
			FAIL(" numeric_to_address(type) unexpecedly parsed a DNS address");
		} else if (t->cooked == NULL) {
			FAIL(" numeric_to_address(type) unexpecedly succeeded");
		} else {
			CHECK_TYPE(address_type(&a));
		}

		if (t->requires_dns && !use_dns) {
			PRINT("skipping str_address() tests as no DNS");
			continue;
		}

		/* DNS/TYPE */

		if (t->requires_dns && !use_dns) {
			PRINT("skipping dns_hunk_to_address(type) -- no DNS");
		} else {
			type = IP_TYPE(t->family);
			err = domain_to_address(shunk1(t->in), type, &a);
			if (err != NULL) {
				if (t->cooked != NULL) {
					FAIL("dns_hunk_to_address(type) unexpecedly failed: %s", err);
				}
			} else if (t->cooked == NULL) {
				FAIL(" dns_hunk_to_address(type) unexpecedly succeeded");
			} else {
				CHECK_TYPE(address_type(&a));
			}
		}

		/* now convert it back cooked */
		CHECK_STR(address_buf, address, t->cooked, &a);

	}
}

static void check_str_address_sensitive(void)
{
	static const struct test {
		int line;
		int family;
		const char *in;
		const char *out;
	} tests[] = {
		{ LN, 4, "1.2.3.4",			"<address>" },
		{ LN, 6, "1:12:3:14:5:16:7:18",	"<address>" },
	};

	for (size_t ti = 0; ti < elemsof(tests); ti++) {
		const struct test *t = &tests[ti];
		PRINT("%s '%s' -> '%s'", pri_family(t->family), t->in, t->out);

		/* convert it *to* internal format */
		const struct ip_info *type = NULL;
		ip_address a;
		err_t err = numeric_to_address(shunk1(t->in), type, &a);
		if (err != NULL) {
			FAIL("numeric_to_address() failed: %s", err);
			continue;
		}
		CHECK_TYPE(address_type(&a));
		CHECK_STR(address_buf, address_sensitive, t->out, &a);
	}
}

static void check_str_address_reversed(void)
{
	static const struct test {
		int line;
		int family;
		const char *in;
		const char *out;                   /* NULL means error expected */
	} tests[] = {
		{ LN, 4, "1.2.3.4", "4.3.2.1.IN-ADDR.ARPA." },
		/* 0 1 2 3 4 5 6 7 8 9 a b c d e f 0 1 2 3 4 5 6 7 8 9 a b c d e f */
		{ LN, 6, "0123:4567:89ab:cdef:1234:5678:9abc:def0",
		  "0.f.e.d.c.b.a.9.8.7.6.5.4.3.2.1.f.e.d.c.b.a.9.8.7.6.5.4.3.2.1.0.IP6.ARPA.", }
	};

	for (size_t ti = 0; ti < elemsof(tests); ti++) {
		const struct test *t = &tests[ti];
		PRINT("%s '%s' -> '%s", pri_family(t->family), t->in, t->out);

		/* convert it *to* internal format */
		const struct ip_info *type = NULL;
		ip_address a;
		err_t err = numeric_to_address(shunk1(t->in), type, &a);
		if (err != NULL) {
			FAIL("numeric_to_address() returned: %s", err);
			continue;
		}
		CHECK_TYPE(address_type(&a));
		CHECK_STR(address_reversed_buf, address_reversed, t->out, &a);
	}
}

static void check_in_addr(void)
{
	static const struct test {
		int line;
		const int family;
		const char *in;
		uint8_t addr[16];
	} tests[] = {
		{ LN, 4, "1.2.3.4", { 1, 2, 3, 4, }, },
		{ LN, 6, "102:304:506:708:90a:b0c:d0e:f10", { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, }, },
	};

	for (size_t ti = 0; ti < elemsof(tests); ti++) {
		const struct test *t = &tests[ti];
		PRINT("%s '%s' -> '%s'", pri_family(t->family), t->in, t->in);

		ip_address a;
		switch (t->family) {
		case 4:
		{
			struct in_addr in;
			memcpy(&in, t->addr, sizeof(in));
			a = address_from_in_addr(&in);
			break;
		}
		case 6:
		{
			struct in6_addr in6;
			memcpy(&in6, t->addr, sizeof(in6));
			a = address_from_in6_addr(&in6);
			break;
		}
		}

		/* as a string */
		address_buf buf;
		const char *out = str_address(&a, &buf);
		if (out == NULL) {
			FAIL("str_address() returned NULL");
		} else if (!strcaseeq(out, t->in)) {
			FAIL("str_address() returned '%s', expecting '%s'",
				out, t->in);
		}

		switch (t->family) {
		case 4:
		{
			uint32_t h = ntohl_address(&a);
			uint32_t n = htonl(h);
			if (!memeq(&n, t->addr, sizeof(n))) {
				FAIL("ntohl_address() returned %08"PRIx32", expecting something else", h);
			}
			break;
		}
		}
	}
}

static void check_address_is(void)
{
	static const struct test {
		int line;
		int family;
		const char *in;
		bool is_unset;
		bool is_any;
		bool is_specified;
		bool is_loopback;
	} tests[] = {
		{ LN, 0, "<invalid>",		.is_unset = true, },
		{ LN, 4, "0.0.0.0",			.is_any = true, },
		{ LN, 6, "::",			.is_any = true, },
		{ LN, 4, "1.2.3.4",			.is_specified = true, },
		{ LN, 6, "1:12:3:14:5:16:7:18",	.is_specified = true, },
		{ LN, 4, "127.0.0.1",		.is_specified = true, .is_loopback = true, },
		{ LN, 6, "::1",			.is_specified = true, .is_loopback = true, },
	};

	for (size_t ti = 0; ti < elemsof(tests); ti++) {
		const struct test *t = &tests[ti];
		PRINT("%s '%s'-> unset: %s, any: %s, specified: %s", pri_family(t->family), t->in,
		      bool_str(t->is_unset), bool_str(t->is_any), bool_str(t->is_specified));

		/* convert it *to* internal format */
		ip_address tmp, *address = &tmp;
		if (t->family == 0) {
			tmp = unset_address;
		} else {
			const struct ip_info *type = NULL;
			err_t err = numeric_to_address(shunk1(t->in), type, &tmp);
			if (err != NULL) {
				FAIL("numeric_to_address() failed: %s", err);
			}
		}

		CHECK_COND(address, is_unset);
		CHECK_COND(address, is_any);
		CHECK_COND(address, is_specified);
		CHECK_COND(address, is_loopback);
	}
}

void ip_address_check(void)
{
	check_shunk_to_address();
	check_str_address_sensitive();
	check_str_address_reversed();
	check_address_is();
	check_in_addr();
}
