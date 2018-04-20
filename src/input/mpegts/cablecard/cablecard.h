/* Tvheadend - CableCARD Logical Network
 *
 * Copyright (C) 2018 Robert Cameron
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __CABLECARD_H__
#define __CABLECARD_H__

/*
 * CableCARD subsystem/logical/physical network
 */
/* Classes - Logical network */
extern const idclass_t	cablecard_network_class;
extern const idclass_t	cablecard_mux_class;
extern const idclass_t	cablecard_service_class;

/* Classes - Physical network */
extern const idclass_t	cablecard_device_class;
extern const idclass_t	cablecard_frontend_class;

/* Types - Logical network */
typedef struct cablecard_network	cablecard_network_t;
typedef struct cablecard_mux	    cablecard_mux_t;
typedef struct cablecard_service	cablecard_service_t;

struct cablecard_network {
	mpegts_network_t;
};

struct cablecard_mux {
	mpegts_mux_t;

	uint32_t	mm_cablecard_vchan;
	char	   *mm_cablecard_name;
};

struct cablecard_service {
	mpegts_service_t;
};

/*
 * Methods
 */
/* CableCARD Subsystem */
void	cablecard_init(void);
void	cablecard_done(void);

/* Logical network */
void	cablecard_network_init(void);
void	cablecard_network_done(void);

cablecard_network_t *	cablecard_network_create(const char *,
  const idclass_t *, htsmsg_t *);

/* Mux */
cablecard_mux_t *	cablecard_mux_create(cablecard_network_t *, const char *,
  htsmsg_t *);

/* Service */
cablecard_service_t *	cablecard_service_create(cablecard_mux_t *, uint16_t,
  uint16_t, const char *, htsmsg_t *);

#endif /* __CABLECARD_H__ */