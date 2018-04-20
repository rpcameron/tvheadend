/*
 *  Tvheadend - CableCARD service
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
 * Creation
 */
cablecard_service_t *
cablecard_service_create(cablecard_mux_t *cm, uint16_t sid, uint16_t pmt,
  const char *uuid, htsmsg_t *conf)
{
	cablecard_service_t	*cs = (cablecard_service_t *)mpegts_service_create0(
	  calloc(1, sizeof(mpegts_service_t)), &mpegts_service_class, uuid,
	  (mpegts_mux_t *)cm, sid, pmt, conf);

	cs->s_dvb_channel_num = cm->mm_cablecard_vchan;
	cs->s_dvb_svcname = strdup(cm->mm_cablecard_name);
	cs->s_dvb_provider = strdup(cm->mm_network->mm_provider_network_name);

	return cs;
}