/*
 *  Tvheadend - CableCARD Logical Network
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

#ifndef __CABLECARD_H__
#define __CABLECARD_H__

#include <stdint.h>

#include "htsmsg.h"
#include "idnode.h"
#include "input.h"


/*
 * CableCARD subsystem/logical network
 */

/* Types */
typedef struct cablecard_network	cablecard_network_t;
typedef struct cablecard_mux	        cablecard_mux_t;
typedef struct cablecard_service	cablecard_service_t;

struct cablecard_network {
	mpegts_network_t;
};

struct cablecard_mux {
	mpegts_mux_t;

	int	        mm_cablecard_vchan;
	uint32_t	mm_cablecard_freq;
	int	        mm_cablecard_pid;
};

struct cablecard_service {
	mpegts_service_t;

	char	*s_cablecard_svcname;
	int	 s_cablecard_chnum;
	char	*s_cablecard_auth;
	char	*s_cablecard_cci;
	char	*s_cablecard_cgms;
	int	 s_cablecard_subscribed;
	int	 s_cablecard_available;
	int	 s_cablecard_protected;
};

/*
 * Classes
 */
extern const idclass_t	cablecard_network_class;
extern const idclass_t	cablecard_mux_class;
extern const idclass_t	cablecard_service_class;

/*
 * Methods
 */
/* CableCARD */
void	cablecard_init(void);
void	cablecard_done(void);

/* Network */
void	cablecard_network_init(void);
void	cablecard_network_done(void);

cablecard_network_t *	cablecard_network_create(const char *,
    const idclass_t *, htsmsg_t *);


/* Mux */
cablecard_mux_t *	cablecard_mux_create(cablecard_network_t *,
    const char *, htsmsg_t *);

#endif /* __CABLECARD_H__ */
