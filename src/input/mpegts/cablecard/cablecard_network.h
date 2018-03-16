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

#ifndef __CABLECARD_NETWORK_H__
#define __CABLECARD_NETWORK_H__

#include <stdint.h>
#include <stdlib.h>

#include "htsmsg.h"
#include "idnode.h"
#include "input.h"

/*
 * Class methods
 */
static void	cablecard_network_class_delete(idnode_t *);

/*
 * Type methods
 */
static htsmsg_t *	cablecard_network_config_save(mpegts_network_t *, char *,
    size_t);
/* mpegts_mux_t *	cablecard_network_create_mux(mpegts_network_t *, void *,
    uint16_t, uint16_t, void *, int); */
static mpegts_service_t *	cablecard_network_create_service(mpegts_mux_t *,
    uint16_t, uint16_t);
static const idclass_t *	cablecard_network_mux_class(mpegts_network_t *);
static mpegts_mux_t *	cablecard_network_mux_create(mpegts_network_t *, htsmsg_t *);

/*
 * Init
 */
static mpegts_network_t *	cablecard_network_builder(const idclass_t *, htsmsg_t *);

#endif /* __CABLECARD_NETWORK_H__ */
