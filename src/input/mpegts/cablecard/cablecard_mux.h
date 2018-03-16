/*
 *  Tvheadend - CableCARD mux
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

#ifndef __CABLECARD_MUX_H__
#define __CABLECARD_MUX_H__

#include <stdlib.h>

#include "idnode.h"
#include "input.h"
#include "prop.h"

#include "cablecard.h"

/*
 * Type methods
 */
static void	cablecard_mux_display_name(mpegts_mux_t *, char *, size_t);
static htsmsg_t *	cablecard_mux_config_save(mpegts_mux_t *, char *, size_t);
static void	cablecard_mux_create_instances(mpegts_mux_t *);
static void	cablecard_mux_delete(mpegts_mux_t *, int);

#endif /* __CABLECARD_MUX_H__ */
