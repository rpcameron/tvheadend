/*
 *  Tvheadend - CableCARD network
 *
 *  Copyright (C) 2017 Robert Cameron
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

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

#include "htsmsg.h"
#include "idnode.h"
#include "input.h"
#include "prop.h"
#include "settings.h"
#include "uuid.h"

#include "cablecard_network.h"

/*
 * Classes
 */

/* Class definition */
extern const idclass_t mpegts_network_class;
const idclass_t cablecard_network_class = {
	.ic_super      = &mpegts_network_class,
	.ic_class      = "cablecard_network",
	.ic_caption    = N_("CableCARD Network"),
	.ic_delete     = cablecard_network_class_delete,
	.ic_properties = (const property_t[]){
		{}
	}
};

/* Class methods */
static void
cablecard_network_class_delete(idnode_t *in)
{
	mpegts_network_t *mn = (mpegts_network_t *)in;
	char ubuf[UUID_HEX_SIZE];

	/* Remove config */
	hts_settings_remove("input/cablecard/networks/%s",
	    idnode_uuid_as_str(in, ubuf));

	/* Parent delete */
	mpegts_network_delete(mn, 1);
}

/*
 * Type methods/callbacks
 */
static htsmsg_t *
cablecard_network_config_save(mpegts_network_t *mn, char *filename,
    size_t fsize)
{
	htsmsg_t *c = htsmsg_create_map();
	char ubuf[UUID_HEX_SIZE];
	idnode_save(&mn->mn_id, c);
	htsmsg_add_str(c, "class", mn->mn_id.in_class->ic_class);
	snprintf(filename, fsize, "input/cablecard/networks/%s/config",
	    idnode_uuid_as_str(&mn->mn_id, ubuf));
	return c;
}

/* I don't think this function is necessary
static mpegts_mux_t *
cablecard_network_create_mux(mpegts_network_t *mn, void *origin, uint16_t onid,
    uint16_t tsid, void *p, int force)
{
	
}
*/	

static mpegts_service_t *
cablecard_network_create_service(mpegts_mux_t *mm, uint16_t sid,
    uint16_t pmt_pid)
{
	/*
	return (mpegts_service_t *)cablecard_service_create(
	    (cablecard_mux_t *)mm, sid, pmt_pid, NULL, NULL);
	*/
	return mpegts_service_create1(NULL, mm, sid, pmt_pid, NULL);
}

static const idclass_t *
cablecard_network_mux_class(mpegts_network_t *mn)
{
	if (idnode_is_instance(&mn->mn_id, &cablecard_network_class))
		return &cablecard_mux_class;
	return NULL;
}

static mpegts_mux_t *
cablecard_network_mux_create(mpegts_network_t *mn, htsmsg_t *conf)
{
	return (mpegts_mux_t *)cablecard_mux_create((cablecard_network_t *)mn,
	    NULL, conf);
}

/*
 * Creation/Config
 */

cablecard_network_t *
cablecard_network_create(const char *uuid, const idclass_t *idc, htsmsg_t *conf)
{
	cablecard_network_t *cn;
	cn = calloc(1, sizeof(cablecard_network_t));

	/* Create network */
	if (!(cn = (cablecard_network_t *)mpegts_network_create0((void *)cn,
	    idc, uuid, NULL, conf)))
		return NULL;

	/* Assign callbacks */
	cn->mn_config_save    = cablecard_network_config_save;
	/* cn->mn_create_mux     = cablecard_network_create_mux; */
	cn->mn_create_service = cablecard_network_create_service;
	cn->mn_mux_class      = cablecard_network_mux_class;
	cn->mn_mux_create2    = cablecard_network_mux_create;
	
	return cn;
}

static mpegts_network_t *
cablecard_network_builder(const idclass_t *idc, htsmsg_t *conf)
{
	return (mpegts_network_t *)cablecard_network_create(NULL, idc, conf);
}

/*
 * Network init
 */
void
cablecard_network_init(void)
{
	htsmsg_t *c, *e;
	htsmsg_field_t *f;

	/* Register mxues */
	idclass_register(&cablecard_mux_class);

	/* Register builders */
	mpegts_network_register_builder(&cablecard_network_class,
	    cablecard_network_builder);

	/* Load settings */
	if (!(c = hts_settings_load_r(1, "input/cablecard/networks")))
		return;

	HTSMSG_FOREACH(f, c) {
		if (!(e = htsmsg_get_map_by_field(f)))
			continue;
		if (!(e = htsmsg_get_map(e, "config")))
			continue;
		cablecard_network_create(f->hmf_name, &cablecard_network_class,
		    e);
	}

	htsmsg_destroy(c);
}

void
cablecard_network_done(void)
{
	extern pthread_mutex_t global_lock;
	pthread_mutex_lock(&global_lock);
	/* Unregister class builder */
	mpegts_network_unregister_builder(&cablecard_network_class);
	mpegts_network_class_delete(&cablecard_network_class, 0);

	pthread_mutex_unlock(&global_lock);
}
