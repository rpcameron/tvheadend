/*
 *  Tvheadend - CableCARD mux
 *
 *  Copyright (C) 2018 Robert Cameron
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "input.h"

/*
 * Class
 */
extern const idclass_t	mpegts_mux_class;
const idclass_t	cablecard_mux_class = {
	.ic_super	= &mpegts_mux_class,
	.ic_class	= "cablecard_mux",
	.ic_caption	= N_("CableCARD Multiplex"),
	.ic_properties	= (const property_t[]){
		{
			.type	= PT_U32,
			.id	    = "vchan",
			.name	= N_("Cable channel"),
			.off	= offsetof(cablecard_mux_t, mm_cablecard_vchan)
		},
		{
			.type	= PT_STR,
			.id	    = "vchan_name",
			.name	= N_("Name"),
			.off	= offsetof(cablecard_mux_t, mm_cablecard_name),
			.opts	= PO_RDONLY | PO_NOSAVE
		}
	}
};

/*
 * Type methods
 */
static void
cablecard_mux_display_name(mpegts_mux_t *mm, char *buf, size_t len)
{
	cablecard_mux_t	*cm = (cablecard_mux_t *)mm;

	snprintf(buf, len, "%u %s", cm->mm_cablecard_vchan, cm->mm_cablecard_name);
}

static htsmsg_t *
cablecard_mux_config_save(mpegts_mux_t *mm, char *filename, size_t fsize)
{
	char         ubuf1[UUID_HEX_SIZE];
	char         ubuf2[UUID_HEX_SIZE];
	htsmsg_t	*c = htsmsg_create_map();

	mpegts_mux_save(mm, c);
	snprintf(filename, fsize, "input/cablecard/networks/%s/muxes/%s",
	  idnode_uuid_as_str(&mm->mm_network->mn_id, ubuf1),
	  idnode_uuid_as_str(&mm->mm_id, ubuf2));
	return c;
}

static void
cablecard_mux_create_instances(mpegts_mux_t *mm)
{
	mpegts_network_link_t *mnl;
	LIST_FOREACH(mnl, &mm->mm_network->mn_inputs, mnl_mn_link) {
		mpegts_input_t *mi = mnl->mnl_input;
		if (mi->mi_is_enabled(mi, mm, 0, -1) != MI_IS_ENABLED_NEVER)
			mi->mi_create_mux_instance(mi, mm);
	}
}

static void
cablecard_mux_delete(mpegts_mux_t *mm, int delconf)
{
	char ubuf1[UUID_HEX_SIZE];
	char ubuf2[UUID_HEX_SIZE];

	/* Remove config */
	if (delconf)
		hts_settings_remove("input/cablecard/networks/%s/muxes/%s",
		  idnode_uuid_as_str(&mm->mm_network->mn_id, ubuf1),
		  idnode_uuid_as_str(&mm->mm_id, ubuf2));

	/* Delete the mux */
	mpegts_mux_delete(mm, delconf);
}

/*
 * Creation
 */
cablecard_mux_t *
cablecard_mux_create(cablecard_network_t *cn, const char *uuid, htsmsg_t *conf)
{
	mpegts_mux_t	*mm;
	cablecard_mux_t	*cm;
	htsmsg_t	    *c, *c2, *e;
	htsmsg_field_t	*f;
	char 	         ubuf1[UUID_HEX_SIZE];
	char 	         ubuf2[UUID_HEX_SIZE];

	/* Create mux */
	mm = calloc(1, sizeof(cablecard_mux_t));
	cm = (cablecard_mux_t *)mm;

	/* Parent init */
	if (!(mm = mpegts_mux_create0(mm, &cablecard_mux_class, uuid,
	  (mpegts_network_t *)cn, MPEGTS_ONID_NONE, MPEGTS_TSID_NONE, conf)))
	    {
		free(mm);
		return NULL;
	}

	/* Assign callbacks */
	cm->mm_config_save	    = cablecard_mux_config_save;
	cm->mm_create_instances = cablecard_mux_create_instances;
	cm->mm_delete 	        = cablecard_mux_delete;
	cm->mm_display_name 	= cablecard_mux_display_name;

	/* No config */
	if (!conf)
		return cm;

	/* Services */
	c2 = NULL;
	c = htsmsg_get_map(conf, "services");
	if (c == NULL)
		c = c2 = hts_settings_load_r(1,
		    "input/cablecard/networks/%s/muxes/%s/services",
		    idnode_uuid_as_str(&cn->mn_id, ubuf1),
		    idnode_uuid_as_str(&mm->mm_id, ubuf2));
	if (c) {
		HTSMSG_FOREACH(f, c) {
			if (!(e = htsmsg_get_map_by_field(f)))
				continue;
			(void)cablecard_service_create(cm, 0, 0, f->hmf_name, e);
		}
		htsmsg_destroy(c2);
	}

	return cm;
}