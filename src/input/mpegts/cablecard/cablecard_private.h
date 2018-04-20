/* Tvheadend - CableCARD Private data
 *
 * Copyright (C) 2014 Patric Karlström
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

 #ifndef __CABLECARD_PRIVATE_H__
 #define __CABLECARD_PRIVATE_H__

#include "libhdhomerun/hdhomerun.h"

/*
 * Types
 */
typedef struct cablecard_device	    cablecard_device_t;
typedef struct cablecard_frontend	cablecard_frontend_t;

static struct hdhomerun_debug_t	   *hdhomerun_debug_obj = 0;

struct	cablecard_device {
	tvh_hardware_t;

	struct hdhomerun_device_t	*hd_tuner;
	mtimer_t	                 hd_destroy_timer;
	uint32_t	                 ip_addr;
	char	                    *uuid;

	TAILQ_HEAD(,cablecard_frontend)	hd_frontends;
};

struct	cablecard_frontend {
	mpegts_input_t;

	cablecard_device_t	        *hf_device;
	struct hdhomerun_device_t	*hf_tuner;
	int	                         hf_locked;
	int	                         hf_ready;
	int	                         hf_status;

	TAILQ_ENTRY(cablecard_frontend)	hf_link;

	pthread_t	    hf_input_thread;
	pthread_mutex_t	hf_input_thread_mutex;
	tvh_cond_t	    hf_input_thread_cond;
	th_pipe_t	    hf_input_thread_pipe;
	uint8_t	        hf_input_thread_running;
	uint8_t	        hf_input_thread_terminating;
	pthread_mutex_t	hf_tuner_mutex;

	mtimer_t	             hf_monitor_timer;
	mpegts_mux_instance_t	*hf_mmi;
};

/* Device methods */
void	cablecard_device_init(void);
void	cablecard_device_done(void);

void	cablecard_device_destroy(cablecard_device_t *);
void	cablecard_device_destroy_later(cablecard_device_t *, int);

static void inline
cablecard_device_changed(cablecard_device_t *sd)
{
	idnode_changed(&sd->th_id);
}

/* Frontend methods */
cablecard_frontend_t	*cablecard_frontend_create(cablecard_device_t *,
  struct hdhomerun_discover_device_t *, htsmsg_t *, unsigned int);
void	                 cablecard_frontend_delete(cablecard_frontend_t *);
void	                 cablecard_frontend_save(cablecard_frontend_t *,
  htsmsg_t *);

#endif /* __CABLECARD_PRIVATE_H__ */