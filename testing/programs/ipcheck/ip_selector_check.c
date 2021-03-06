/* test selectors, for libreswan
 *
 * Copyright (C) 2000  Henry Spencer.
 * Copyright (C) 2018, 2019, 2020  Andrew Cagney
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
 */

#include <stdio.h>

#include "lswcdefs.h"		/* for elemsof() */
#include "constants.h"		/* for streq() */
#include "ipcheck.h"
#include "ip_selector.h"	/* should be in ip_selector_check.c */

struct selector {
	int family;
	const char *addresses;
	const char *protoport;
};

struct from_test {
	int line;
	struct selector from;
	const char *subnet;
	const char *range;
	unsigned ipproto;
	uint16_t hport;
	uint8_t nport[2];
};

static void check_selector_from(const struct from_test *tests, unsigned nr_tests,
				const char *what,
				err_t (*to_selector)(const struct selector *from,
						     ip_selector *out, struct logger *logger),
				struct logger *logger)
{

	for (size_t ti = 0; ti < nr_tests; ti++) {
		const struct from_test *t = &tests[ti];
		PRINT("%s %s=%s protoport=%s range=%s ipproto=%u hport=%u nport=%02x%02x", \
		      pri_family(t->from.family),			\
		      what, t->from.addresses != NULL ? t->from.addresses : "N/A", \
		      t->from.protoport != NULL ? t->from.protoport : "N/A", \
		      t->range != NULL ? t->range : "N/A",		\
		      t->ipproto,					\
		      t->hport,						\
		      t->nport[0], t->nport[1])

		ip_selector selector;
		err_t err = to_selector(&t->from, &selector, logger);
		if (t->range != NULL) {
			if (err != NULL) {
				FAIL("to_selector() failed: %s", err);
			}
		} else if (err == NULL) {
			FAIL("to_selector() should have failed");
		} else {
			continue;
		}

		CHECK_FAMILY(t->from.family, selector_type(&selector));

		ip_subnet subnet = selector_subnet(selector);
		subnet_buf sb;
		str_subnet(&subnet, &sb);
		if (!streq(sb.buf, t->subnet)) {
			FAIL("str_range() was %s, expected %s", sb.buf, t->subnet);
		}

		ip_range range = selector_range(&selector);
		range_buf rb;
		str_range(&range, &rb);
		if (!streq(rb.buf, t->range)) {
			FAIL("range was %s, expected %s", rb.buf, t->range);
		}

		const ip_protocol *protocol = selector_protocol(&selector);
		if (protocol->ipproto != t->ipproto) {
			FAIL("ipproto was %u, expected %u", protocol->ipproto, t->ipproto);
		}

		ip_port port = selector_port(&selector);

		uint16_t hp = hport(port);
		if (!memeq(&hp, &t->hport, sizeof(hport))) {
			FAIL("selector_hport() returned '%d', expected '%d'",
			     hp, t->hport);
		}

		uint16_t np = nport(port);
		if (!memeq(&np, &t->nport, sizeof(nport))) {
			FAIL("selector_nport() returned '%04x', expected '%02x%02x'",
			     np, t->nport[0], t->nport[1]);
		}
	}
}

static err_t do_selector_from_address(const struct selector *s,
				      ip_selector *selector,
				      struct logger *logger_unused UNUSED)
{
	if (s->family == 0) {
		*selector = unset_selector;
		return NULL;
	}

	ip_address address;
	err_t err = numeric_to_address(shunk1(s->addresses), IP_TYPE(s->family), &address);
	if (err != NULL) {
		return err;
	}

	ip_protoport protoport;
	err = ttoprotoport(s->protoport, &protoport);
	if (err != NULL) {
		return err;
	}

	*selector = selector_from_address_protoport(&address, &protoport);
	return NULL;
}

static void check_selector_from_address(struct logger *logger)
{
	static const struct from_test tests[] = {
		{ LN, { 4, "128.0.0.0", "0/0", }, "128.0.0.0/32", "128.0.0.0-128.0.0.0", 0, 0, { 0, 0, }, },
		{ LN, { 6, "8000::", "16/10", }, "8000::/128", "8000::-8000::", 16, 10, { 0, 10, }, },
	};
	check_selector_from(tests, elemsof(tests), "address",
			    do_selector_from_address, logger);
}

static err_t do_selector_from_subnet_protoport(const struct selector *s,
					       ip_selector *selector,
					       struct logger *logger)
{
	if (s->family == 0) {
		*selector = unset_selector;
		return NULL;
	}

	ip_subnet subnet;
	err_t err = ttosubnet(shunk1(s->addresses), IP_TYPE(s->family), '6',
			      &subnet, logger);
	if (err != NULL) {
		return err;
	}

	ip_protoport protoport;
	err = ttoprotoport(s->protoport, &protoport);
	if (err != NULL) {
		return err;
	}

	*selector = selector_from_subnet_protoport(&subnet, &protoport);
	return NULL;
}

static void check_selector_from_subnet_protoport(struct logger *logger)
{
	static const struct from_test tests[] = {
		/* zero port implied */
		{ LN, { 4, "0.0.0.0/0", "0/0", }, "0.0.0.0/0", "0.0.0.0-255.255.255.255", 0, 0, { 0, 0, }, },
		{ LN, { 6, "::0/0", "0/0", }, "::/0", "::-ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", 0, 0, { 0, 0, }, },
		{ LN, { 4, "101.102.0.0/16", "0/0", }, "101.102.0.0/16", "101.102.0.0-101.102.255.255", 0, 0, { 0, 0, }, },
		{ LN, { 6, "1001:1002:1003:1004::/64", "0/0", }, "1001:1002:1003:1004::/64", "1001:1002:1003:1004::-1001:1002:1003:1004:ffff:ffff:ffff:ffff", 0, 0, { 0, 0, }, },
		{ LN, { 4, "101.102.103.104/32", "0/0", }, "101.102.103.104/32", "101.102.103.104-101.102.103.104", 0, 0, { 0, 0, }, },
		{ LN, { 6, "1001:1002:1003:1004:1005:1006:1007:1008/128", "0/0", }, "1001:1002:1003:1004:1005:1006:1007:1008/128", "1001:1002:1003:1004:1005:1006:1007:1008-1001:1002:1003:1004:1005:1006:1007:1008", 0, 0, { 0, 0, }, },
		/* "reserved" zero port specified; reject? */
		{ LN, { 4, "0.0.0.0/0", "0/0", }, "0.0.0.0/0", "0.0.0.0-255.255.255.255", 0, 0, { 0, 0, }, },
		{ LN, { 6, "::0/0", "0/0", }, "::/0", "::-ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", 0, 0, { 0, 0, }, },
		{ LN, { 4, "101.102.0.0/16", "0/0", }, "101.102.0.0/16", "101.102.0.0-101.102.255.255", 0, 0, { 0, 0, }, },
		{ LN, { 6, "1001:1002:1003:1004::/64", "0/0", }, "1001:1002:1003:1004::/64", "1001:1002:1003:1004::-1001:1002:1003:1004:ffff:ffff:ffff:ffff", 0, 0, { 0, 0, }, },
		{ LN, { 4, "101.102.103.104/32", "0/0", }, "101.102.103.104/32", "101.102.103.104-101.102.103.104", 0, 0, { 0, 0, }, },
		{ LN, { 6, "1001:1002:1003:1004:1005:1006:1007:1008/128", "0/0", }, "1001:1002:1003:1004:1005:1006:1007:1008/128", "1001:1002:1003:1004:1005:1006:1007:1008-1001:1002:1003:1004:1005:1006:1007:1008", 0, 0, { 0, 0, }, },
		/* non-zero port mixed with mask; only allow when /32/128? */
		{ LN, { 4, "0.0.0.0/0", "16/65534", }, "0.0.0.0/0", "0.0.0.0-255.255.255.255", 16, 65534, { 255, 254, }, },
		{ LN, { 6, "::0/0", "16/65534", }, "::/0", "::-ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", 16, 65534, { 255, 254, }, },
		{ LN, { 4, "101.102.0.0/16", "16/65534", }, "101.102.0.0/16", "101.102.0.0-101.102.255.255", 16, 65534, { 255, 254, }, },
		{ LN, { 6, "1001:1002:1003:1004::/64", "16/65534", }, "1001:1002:1003:1004::/64", "1001:1002:1003:1004::-1001:1002:1003:1004:ffff:ffff:ffff:ffff", 16, 65534, { 255, 254, }, },
		{ LN, { 4, "101.102.103.104/32", "16/65534", }, "101.102.103.104/32", "101.102.103.104-101.102.103.104", 16, 65534, { 255, 254, }, },
		{ LN, { 6, "1001:1002:1003:1004:1005:1006:1007:1008/128", "16/65534", }, "1001:1002:1003:1004:1005:1006:1007:1008/128", "1001:1002:1003:1004:1005:1006:1007:1008-1001:1002:1003:1004:1005:1006:1007:1008", 16, 65534, { 255, 254, }, },
	};
	check_selector_from(tests, elemsof(tests), "subnet",
			    do_selector_from_subnet_protoport, logger);
}

static err_t do_numeric_to_selector(const struct selector *s,
				    ip_selector *selector,
				    struct logger *logger_ UNUSED)
{
	if (s->family == 0) {
		*selector = unset_selector;
		return NULL;
	}

	return numeric_to_selector(shunk1(s->addresses), IP_TYPE(s->family), selector);
}

static void check_numeric_to_selector(struct logger *logger)
{
	static const struct from_test tests[] = {
		/* missing prefix-bits */
		{ LN, { 4, "0.0.0.0", NULL, }, "0.0.0.0/32", "0.0.0.0-0.0.0.0", 0, 0, { 0, 0, }, },
		{ LN, { 6, "::0", NULL, }, "::/128", "::-::", 0, 0, { 0, 0, }, },
		/* missing protocol/port */
		{ LN, { 4, "0.0.0.0/0", NULL, }, "0.0.0.0/0", "0.0.0.0-255.255.255.255", 0, 0, { 0, 0, }, },
		{ LN, { 6, "::0/0", NULL, }, "::/0", "::-ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", 0, 0, { 0, 0, }, },
		{ LN, { 4, "101.102.0.0/16", NULL, }, "101.102.0.0/16", "101.102.0.0-101.102.255.255", 0, 0, { 0, 0, }, },
		{ LN, { 6, "1001:1002:1003:1004::/64", NULL, }, "1001:1002:1003:1004::/64", "1001:1002:1003:1004::-1001:1002:1003:1004:ffff:ffff:ffff:ffff", 0, 0, { 0, 0, }, },
		{ LN, { 4, "101.102.103.104/32", NULL, }, "101.102.103.104/32", "101.102.103.104-101.102.103.104", 0, 0, { 0, 0, }, },
		{ LN, { 6, "1001:1002:1003:1004:1005:1006:1007:1008/128", NULL, }, "1001:1002:1003:1004:1005:1006:1007:1008/128", "1001:1002:1003:1004:1005:1006:1007:1008-1001:1002:1003:1004:1005:1006:1007:1008", 0, 0, { 0, 0, }, },
		/* "reserved" zero port specified; reject? */
		{ LN, { 4, "0.0.0.0/0:0/0", NULL, }, "0.0.0.0/0", "0.0.0.0-255.255.255.255", 0, 0, { 0, 0, }, },
		{ LN, { 6, "::0/0:0/0", NULL, }, "::/0", "::-ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", 0, 0, { 0, 0, }, },
		{ LN, { 4, "101.102.0.0/16:0/0", NULL, }, "101.102.0.0/16", "101.102.0.0-101.102.255.255", 0, 0, { 0, 0, }, },
		{ LN, { 6, "1001:1002:1003:1004::/64:0/0", NULL, }, "1001:1002:1003:1004::/64", "1001:1002:1003:1004::-1001:1002:1003:1004:ffff:ffff:ffff:ffff", 0, 0, { 0, 0, }, },
		{ LN, { 4, "101.102.103.104/32:0/0", NULL, }, "101.102.103.104/32", "101.102.103.104-101.102.103.104", 0, 0, { 0, 0, }, },
		{ LN, { 6, "1001:1002:1003:1004:1005:1006:1007:1008/128:0/0", NULL, }, "1001:1002:1003:1004:1005:1006:1007:1008/128", "1001:1002:1003:1004:1005:1006:1007:1008-1001:1002:1003:1004:1005:1006:1007:1008", 0, 0, { 0, 0, }, },
		/* non-zero port mixed with mask; only allow when /32/128? */
		{ LN, { 4, "0.0.0.0/0:udp/65534", NULL, }, "0.0.0.0/0", "0.0.0.0-255.255.255.255", 17, 65534, { 255, 254, }, },
		{ LN, { 6, "::0/0:udp/65534", NULL, }, "::/0", "::-ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", 17, 65534, { 255, 254, }, },
		{ LN, { 4, "101.102.0.0/16:udp/65534", NULL, }, "101.102.0.0/16", "101.102.0.0-101.102.255.255", 17, 65534, { 255, 254, }, },
		{ LN, { 6, "1001:1002:1003:1004::/64:udp/65534", NULL, }, "1001:1002:1003:1004::/64", "1001:1002:1003:1004::-1001:1002:1003:1004:ffff:ffff:ffff:ffff", 17, 65534, { 255, 254, }, },
		{ LN, { 4, "101.102.103.104/32:udp/65534", NULL, }, "101.102.103.104/32", "101.102.103.104-101.102.103.104", 17, 65534, { 255, 254, }, },
		{ LN, { 6, "1001:1002:1003:1004:1005:1006:1007:1008/128:udp/65534", NULL, }, "1001:1002:1003:1004:1005:1006:1007:1008/128", "1001:1002:1003:1004:1005:1006:1007:1008-1001:1002:1003:1004:1005:1006:1007:1008", 17, 65534, { 255, 254, }, },
		/* hex/octal */
		{ LN, { 4, "101.102.0.0/16:tcp/0xfffe", NULL, }, "101.102.0.0/16", "101.102.0.0-101.102.255.255", 6, 65534, { 255, 254, }, },
		{ LN, { 6, "1001:1002:1003:1004::/64:udp/0177776", NULL, }, "1001:1002:1003:1004::/64", "1001:1002:1003:1004::-1001:1002:1003:1004:ffff:ffff:ffff:ffff", 17, 65534, { 255, 254, }, },
		/* invalid */
		{ LN, { 4, "1.2.3.4/", NULL, }, NULL, NULL, 0, 0, { 0, 0, }, },
		{ LN, { 4, "1.2.3.4/24", NULL, }, NULL, NULL, 0, 0, { 0, 0, }, },
		{ LN, { 4, "1.2.3.0/24:", NULL, }, NULL, NULL, 0, 0, { 0, 0, }, },
		{ LN, { 4, "1.2.3.0/24:-1/-1", NULL, }, NULL, NULL, 0, 0, { 0, 0, }, },
		{ LN, { 4, "1.2.3.0/24:none/", NULL, }, NULL, NULL, 0, 0, { 0, 0, }, },
	};

	check_selector_from(tests, elemsof(tests), "selector",
			    do_numeric_to_selector, logger);
}

static void check_selector_contains(struct logger *logger)
{
	static const struct test {
		int line;
		struct selector from;
		bool is_unset;
		bool contains_all_addresses;
		bool contains_some_addresses;
		bool is_one_address;
		bool contains_no_addresses;
	} tests[] = {
		/* all */
		{ LN, { 0, NULL, NULL, },            true, false, false, false, false, },
		/* all */
		{ LN, { 4, "0.0.0.0/0", "0/0", },    false, true, false, false, false, },
		{ LN, { 6, "::/0", "0/0", },         false, true, false, false, false, },
		/* some */
		{ LN, { 4, "127.0.0.0/31", "0/0", }, false, false, true, false, false, },
		{ LN, { 6, "8000::/127", "0/0", },   false, false, true, false, false, },
		/* one */
		{ LN, { 4, "127.0.0.1/32", "0/0", }, false, false, true, true, false, },
		{ LN, { 6, "8000::/128", "0/0", },   false, false, true, true, false, },
		/* none */
		{ LN, { 4, "0.0.0.0/32", "0/0", },   false, false, false, false, true, },
		{ LN, { 6, "::/128", "0/0", },       false, false, false, false, true, },
	};

	for (size_t ti = 0; ti < elemsof(tests); ti++) {
		err_t err;
		const struct test *t = &tests[ti];
		PRINT("%s subnet=%s protoport=%s unset=%s all=%s some=%s one=%s none=%s",
		      pri_family(t->from.family),
		      t->from.addresses != NULL ? t->from.addresses : "<unset>",
		      t->from.protoport != NULL ? t->from.protoport : "<unset>",
		      bool_str(t->is_unset),
		      bool_str(t->contains_all_addresses),
		      bool_str(t->contains_some_addresses),
		      bool_str(t->is_one_address),
		      bool_str(t->contains_no_addresses));

		ip_selector selector;
		err = do_numeric_to_selector(&t->from, &selector, logger);
		if (err != NULL) {
			FAIL("to_selector() failed: %s", err);
		}

#define T(COND)								\
		bool COND = selector_##COND(&selector);			\
		if (COND != t->COND) {					\
			selector_buf b;					\
			FAIL("selector_"#COND"(%s) returned %s, expected %s", \
			     str_selector(&selector, &b),		\
			     bool_str(COND), bool_str(t->COND));	\
		}
		T(is_unset);
		T(contains_all_addresses);
		T(is_one_address);
		T(contains_no_addresses);
	}
}

static void check_in_selector(struct logger *logger)
{

	static const struct test {
		int line;
		struct selector inner;
		struct selector outer;
		bool selector;
		bool address;
		bool endpoint;
	} tests[] = {

		/* all */

		{ LN, { 4, "0.0.0.0/0", "0/0", }, { 4, "0.0.0.0/0", "0/0", },             true, true, true, },
		{ LN, { 6, "::/0", "0/0", },      { 6, "::/0", "0/0", },                  true, true, true, },

		{ LN, { 4, "0.0.0.0/0", "0/0", }, { 4, "0.0.0.0/0", "udp/10", },          false, true, false, },
		{ LN, { 6, "::/0", "0/0", },      { 6, "::/0", "udp/10", },               false, true, false, },

		{ LN, { 4, "0.0.0.0/0", "udp/10", }, { 4, "0.0.0.0/0", "0/0", },          true, true, true, },
		{ LN, { 6, "::/0", "udp/10", },      { 6, "::/0", "0/0", },               true, true, true, },

		{ LN, { 4, "0.0.0.0/0", "udp/10", }, { 4, "0.0.0.0/0", "udp/10", },       true, true, true, },
		{ LN, { 6, "::/0", "udp/10", },      { 6, "::/0", "udp/10", },            true, true, true, },

		{ LN, { 4, "0.0.0.0/0", "udp/10", }, { 4, "0.0.0.0/0", "udp/11", },       false, true, false, },
		{ LN, { 6, "::/0", "udp/10", },      { 6, "::/0", "udp/11", },            false, true, false, },

		{ LN, { 4, "0.0.0.0/0", "udp/10", }, { 4, "0.0.0.0/0", "tcp/10", },       false, true, false, },
		{ LN, { 6, "::/0", "udp/10", },      { 6, "::/0", "tcp/10", },            false, true, false, },

		/* some */

		{ LN, { 4, "127.0.0.1/32", "0/0", }, { 4, "127.0.0.0/31", "0/0", },       true,true, true, },
		{ LN, { 6, "8000::/128", "0/0", },   { 6, "8000::/127", "0/0", },         true, true, true, },

		{ LN, { 4, "127.0.0.1/32", "0/0", }, { 4, "127.0.0.0/31", "tcp/10", },    false, true, false, },
		{ LN, { 6, "8000::/128", "0/0", },   { 6, "8000::/127", "tcp/10", },      false, true, false, },

		{ LN, { 4, "127.0.0.1/32", "tcp/10", }, { 4, "127.0.0.0/31", "0/0", },    true, true, true, },
		{ LN, { 6, "8000::/128", "tcp/10", },   { 6, "8000::/127", "0/0", },      true, true, true, },

		{ LN, { 4, "127.0.0.1/32", "tcp/10", }, { 4, "127.0.0.0/31", "tcp/10", }, true, true, true, },
		{ LN, { 6, "8000::/128", "tcp/10", },   { 6, "8000::/127", "tcp/10", },   true, true, true, },

		{ LN, { 4, "127.0.0.1/32", "tcp/10", }, { 4, "127.0.0.0/31", "tcp/11", }, false, true, false, },
		{ LN, { 6, "8000::/128", "tcp/10", },   { 6, "8000::/127", "tcp/11", },   false, true, false, },

		{ LN, { 4, "127.0.0.1/32", "tcp/10", }, { 4, "127.0.0.0/31", "udp/10", }, false, true, false, },
		{ LN, { 6, "8000::/128", "tcp/10", },   { 6, "8000::/127", "udp/10", },   false, true, false, },

		/* one */

		{ LN, { 4, "127.0.0.1/32", "0/0", }, { 4, "127.0.0.1/32", "0/0", },       true, true, true, },
		{ LN, { 6, "8000::/128", "0/0", },   { 6, "8000::/128", "0/0", },         true, true, true, },

		{ LN, { 4, "127.0.0.1/32", "0/0", }, { 4, "127.0.0.1/32", "udp/10", },    false, true, false, },
		{ LN, { 6, "8000::/128", "0/0", },   { 6, "8000::/128", "udp/10", },      false, true, false, },

		{ LN, { 4, "127.0.0.1/32", "udp/10", }, { 4, "127.0.0.1/32", "0/0", },    true, true, true, },
		{ LN, { 6, "8000::/128", "udp/10", },   { 6, "8000::/128", "0/0", },      true, true, true, },

		{ LN, { 4, "127.0.0.1/32", "udp/10", }, { 4, "127.0.0.1/32", "udp/10", }, true, true, true, },
		{ LN, { 6, "8000::/128", "udp/10", },   { 6, "8000::/128", "udp/10", },   true, true, true, },

		{ LN, { 4, "127.0.0.1/32", "udp/10", }, { 4, "127.0.0.1/32", "udp/11", }, false, true, false, },
		{ LN, { 6, "8000::/128", "udp/10", },   { 6, "8000::/128", "udp/11", },   false, true, false, },

		{ LN, { 4, "127.0.0.1/32", "udp/10", }, { 4, "127.0.0.1/32", "tcp/10", }, false, true, false, },
		{ LN, { 6, "8000::/128", "udp/10", },   { 6, "8000::/128", "tcp/10", },   false, true, false, },

		/* none - so nothing can match */

		{ LN, { 4, "127.0.0.0/32", "0/0", }, { 4, "0.0.0.0/32", "0/0", },         false, false, false, },
		{ LN, { 6, "::1/128", "0/0", },      { 6, "::/128", "0/0", },             false, false, false, },

		{ LN, { 4, "127.0.0.0/32", "udp/10", }, { 4, "0.0.0.0/32", "0/0", },      false, false, false, },
		{ LN, { 6, "::1/128", "udp/10", },      { 6, "::/128", "0/0", },          false, false, false, },

		{ LN, { 4, "0.0.0.0/32", "0/0", }, { 4, "0.0.0.0/32", "0/0", },           false, false, false, },
		{ LN, { 6, "::/128", "0/0", },      { 6, "::/128", "0/0", },              false, false, false, },

		{ LN, { 4, "0.0.0.0/32", "udp/10", }, { 4, "0.0.0.0/32", "0/0", },        false, false, false, },
		{ LN, { 6, "::/128", "udp/10", },      { 6, "::/128", "0/0", },           false, false, false, },

		/* these a non-sensical - rhs has no addresses yet udp */

		{ LN, { 4, "127.0.0.0/32", "0/0", }, { 4, "0.0.0.0/32", "udp/10", },      false, false, false, },
		{ LN, { 6, "::1/128", "0/0", },      { 6, "::/128", "udp/10", },          false, false, false, },

		{ LN, { 4, "127.0.0.0/32", "udp/10", }, { 4, "0.0.0.0/32", "udp/10", },   false, false, false, },
		{ LN, { 6, "::1/128", "udp/10", },      { 6, "::/128", "udp/10", },       false, false, false, },

		/* these a non-sensical - rhs has zero addresses */

		{ LN, { 4, "127.0.0.0/32", "0/0", }, { 4, "0.0.0.0/31", "0/0", },         false, false, false, },
		{ LN, { 6, "::1/128", "0/0", },      { 6, "::/127", "0/0", },             false, false, false, },

		{ LN, { 4, "127.0.0.0/32", "udp/10", }, { 4, "0.0.0.0/31", "udp/10", },   false, false, false, },
		{ LN, { 6, "::1/128", "udp/10", },      { 6, "::/127", "udp/10", },       false, false, false, },

	};

	for (size_t ti = 0; ti < elemsof(tests); ti++) {
		err_t err;
		const struct test *t = &tests[ti];
		PRINT("{ %s subnet=%s protoport=%s } in { %s subnet=%s protoport=%s } selector=%s address=%s endpoint=%s",
		      pri_family(t->inner.family), t->inner.addresses, t->inner.protoport,
		      pri_family(t->outer.family), t->outer.addresses, t->outer.protoport,
		      bool_str(t->selector),
		      bool_str(t->address),
		      bool_str(t->endpoint));

		ip_selector outer_selector;
		err = do_selector_from_subnet_protoport(&t->outer, &outer_selector, logger);
		if (err != NULL) {
			FAIL("outer-selector failed: %s", err);
		}

		ip_selector inner_selector;
		err = do_selector_from_subnet_protoport(&t->inner, &inner_selector, logger);
		if (err != NULL) {
			FAIL("inner-selector failed: %s", err);
		}
		bool selector = selector_in_selector(&inner_selector, &outer_selector);
		if (selector != t->selector) {
			FAIL("selector_in_selector() returned %s, expecting %s",
			     bool_str(selector), bool_str(t->selector));
		}

		ip_address inner_address = selector_prefix(&inner_selector);
		bool address = address_in_selector(&inner_address, &outer_selector);
		if (address != t->address) {
			FAIL("address_in_selector() returned %s, expecting %s",
			     bool_str(address), bool_str(t->address));
		}

		const ip_protocol *protocol = selector_protocol(&inner_selector);
		ip_port port = selector_port(&inner_selector);
		if (protocol != &ip_protocol_unset && port.hport != 0) {
			ip_endpoint inner_endpoint = endpoint_from_address_protocol_port(&inner_address, protocol, port);
			bool endpoint = endpoint_in_selector(&inner_endpoint, &outer_selector);
			if (endpoint != t->endpoint) {
				FAIL("endpoint_in_selector() returned %s, expecting %s",
				     bool_str(endpoint), bool_str(t->endpoint));
			}
		}
	}
}

void ip_selector_check(struct logger *logger)
{
	check_selector_from_address(logger);
	check_selector_from_subnet_protoport(logger);
	check_selector_contains(logger);
	check_in_selector(logger);
	check_numeric_to_selector(logger);
}
