#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>

#include "QF/ecs.h"
#include "QF/ui/node.h"

static ecs_system_t test_sys;
#define t_link_in (test_sys.base + node_link_in)
#define t_link_out (test_sys.base + node_link_out)
#define t_link_con (test_sys.base + node_link_con)
#define t_con_in (test_sys.base + node_con_in)
#define t_con_out (test_sys.base + node_con_out)

typedef struct {
} testdata_t;

// indices into the out_connectors (src) and in_connectors (dst) arrays
static node_link_t test_links[] = {
	{ .src = 0, .dst = 0 },
	{ .src = 1, .dst = 0 },
	{ .src = 2, .dst = 1 },
	{ .src = 2, .dst = 2 },
	{ .src = 0, .dst = 2 },
	{ .src = 0, .dst = 1 },
};

static const char *out_names[] = { "a", "b", "c" };
static const char *in_names[] = { "d", "e", "f" };
static uint32_t out_connectors[countof(out_names)];
static uint32_t in_connectors[countof(in_names)];
static uint32_t links[countof (test_links)];

static int
check_subpool_ranges (ecs_subpool_t *subpool,
					  uint32_t num_expect, uint32_t *expect)
{
	uint32_t    count = subpool->num_ranges - subpool->available;
	uint32_t   *range = subpool->ranges;
	int         ret = 0;

	if (count != num_expect) {
		printf ("incorrct number of ranges: %d %d\n", count, num_expect);
		return 1;
	}
	puts (GRN "check_subpool_ranges" DFL);
	while (count--) {
		uint32_t   *r = range++;
		uint32_t    e = *expect++;
		printf ("%2d: %2d %2d\n", (int)(r - subpool->ranges), *r, e);
		if (*r != e++) {
			ret = 1;
		}
	}
	return ret;
}

int
main (void)
{
	int         ret = 0;

	test_sys.reg = ECS_NewRegistry ("node");
	test_sys.base = ECS_RegisterComponents (test_sys.reg, node_components,
											node_comp_count);
	ECS_CreateComponentPools (test_sys.reg);

	// create the set of output end-points
	for (uint32_t i = 0; i < countof (out_connectors); i++) {
		out_connectors[i] = ECS_NewEntity (test_sys.reg);
		uint32_t grp_id = ECS_NewSubpoolRange (test_sys.reg, t_link_out);
		Ent_SetComponent (out_connectors[i], t_con_out, test_sys.reg, &grp_id);
		Ent_SetComponent (out_connectors[i], ecs_name, test_sys.reg,
						  &out_names[i]);
	}

	// create the set of input end-points
	for (uint32_t i = 0; i < countof (in_connectors); i++) {
		in_connectors[i] = ECS_NewEntity (test_sys.reg);
		uint32_t grp_id = ECS_NewSubpoolRange (test_sys.reg, t_link_in);
		Ent_SetComponent (in_connectors[i], t_con_in, test_sys.reg, &grp_id);
		Ent_SetComponent (in_connectors[i], ecs_name, test_sys.reg,
						  &in_names[i]);
	}

	// create the links from output end-points to input end-points
	for (uint32_t i = 0; i < countof (test_links); i++) {
		node_link_t link = {
			.src = out_connectors[test_links[i].src],
			.dst = in_connectors[test_links[i].dst],
		};
		//FIXME this needs to be an API function
		links[i] = ECS_NewEntity (test_sys.reg);
		Ent_SetComponent (links[i], t_link_con, test_sys.reg, &link);
		Ent_SetComponent (links[i], t_link_out, test_sys.reg, &link.src);
		Ent_SetComponent (links[i], t_link_in,  test_sys.reg, &link.dst);
	}

	ECS_PrintRegistry (test_sys.reg);

	for (uint32_t i = 0; i < countof (links); i++) {
		uint32_t l = links[i];
		uint32_t *l_out = Ent_GetComponent (l, t_link_out, test_sys.reg);
		uint32_t *l_in = Ent_GetComponent (l, t_link_in,  test_sys.reg);
		node_link_t *con = Ent_GetComponent (l, t_link_con, test_sys.reg);
		const char **snm = Ent_GetComponent (con->src, ecs_name, test_sys.reg);
		const char **dnm = Ent_GetComponent (con->dst, ecs_name, test_sys.reg);
		printf ("link: %d %3x.%05x"
				" %sout:%08x"DFL
				" %sin:%08x"DFL
				" %ssrc:%08x"DFL
				" %sdst:%08x"DFL
				" %s%s->%s"DFL"\n",
				i, Ent_Generation (l) >> ENT_IDBITS, Ent_Index (l),
				*l_out == con->src ? DFL : RED,
				*l_out,
				*l_in == con->dst ? DFL : RED,
				*l_in,
				*l_out == con->src ? DFL : RED,
				con->src,
				*l_in == con->dst ? DFL : RED,
				con->dst,
				*l_out == con->src && *l_in == con->dst ? GRN : RED,
				*snm, *dnm);
		if (*l_out != con->src || *l_in != con->dst) {
			ret = 1;
		}
	}
	auto subpools = test_sys.reg->subpools;
	if (check_subpool_ranges (&subpools[test_sys.base + node_link_out],
							  3, (uint32_t[]) { 3, 4, 6})) {
		ret = 1;
	}
	if (check_subpool_ranges (&subpools[test_sys.base + node_link_in],
							  3, (uint32_t[]) { 2, 4, 6})) {
		ret = 1;
	}

	return ret;
}
