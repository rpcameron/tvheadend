/* Tvheadend - HDHomeRun CableCARD device
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

#include <openssl/sha.h>

#include "input.h"

#include "cablecard_private.h"

#define MAX_HDHOMERUN_DEVICES 8

#ifdef HDHOMERUN_TAG_DEVICE_AUTH_BIN
#define hdhomerun_discover_find_devices_custom \
	hdhomerun_discover_find_devices_custom_v2
#endif

/*
 * Device class
 */
/* Methods */
static htsmsg_t *
cablecard_device_class_save(idnode_t *in, char *filename, size_t fsize)
{
	cablecard_device_t	    *hd = (cablecard_device_t *)in;
	cablecard_frontend_t	*lfe;
	htsmsg_t	            *m, *l;
	char	                 ubuf[UUID_HEX_SIZE];

	m = htsmsg_create_map();
	idnode_save(&hd->th_id, m);

	l = htsmsg_create_map();
	TAILQ_FOREACH(lfe, &hd->hd_frontends, hf_link)
		cablecard_frontend_save(lfe, l);
	htsmsg_add_msg(m, "frontends", l);

	snprintf(filename, fsize, "input/cablecard/adapters/%s",
	  idnode_uuid_as_str(&hd->th_id, ubuf));
	return m;
}

static idnode_set_t *
cablecard_device_class_get_childs(idnode_t *in)
{
	cablecard_device_t	    *hd = (cablecard_device_t *)in;
	idnode_set_t	        *is = idnode_set_create(0);
	cablecard_frontend_t	*lfe;

	TAILQ_FOREACH(lfe, &hd->hd_frontends, hf_link)
		idnode_set_add(is, &lfe->ti_id, NULL, NULL);
	return is;
}

static const char *
cablecard_device_class_get_title(idnode_t *in, const char *lang)
{
	cablecard_device_t	*hd = (cablecard_device_t *)in;
	snprintf(prop_sbuf, PROP_SBUF_LEN, "HDHomeRun CableCARD - %08X",
	  hd->hd_tuner->device_id);
	return prop_sbuf;
}

static const void *
cablecard_device_class_active_get(void *obj)
{
	cablecard_device_t	    *hd = (cablecard_device_t *)obj;
	cablecard_frontend_t	*lfe;
	static int	             active = 0;

	TAILQ_FOREACH(lfe, &hd->hd_frontends, hf_link) {
		if (*(int *)mpegts_input_class_active_get(lfe)) {
			active = 1;
			break;
		}
	}
	return &active;
}

static const void *
cablecard_device_class_get_ip_address(void *obj)
{
	cablecard_device_t	*hd = (cablecard_device_t *)obj;

	snprintf(prop_sbuf, PROP_SBUF_LEN, "%u.%u.%u.%u",
	  (unsigned int)(hd->ip_addr >> 24) & 0x0FF,
	  (unsigned int)(hd->ip_addr >> 16) & 0x0FF,
	  (unsigned int)(hd->ip_addr >>  8) & 0x0FF,
	  (unsigned int)(hd->ip_addr >>  0) & 0x0FF);
	return &prop_sbuf_ptr;
}

static const void *
cablecard_device_class_get_device_id(void *obj)
{
	cablecard_device_t	*hd = (cablecard_device_t *)obj;

	snprintf(prop_sbuf, PROP_SBUF_LEN, "%08X", hd->hd_tuner->device_id);
	return &prop_sbuf_ptr;
}

/* Class definition */
const idclass_t	cablecard_device_class = {
	.ic_class	    = "cablecard_client",
	.ic_caption	    = N_("HDHomeRun CableCARD client"),
	.ic_save	    = cablecard_device_class_save,
	.ic_get_childs	= cablecard_device_class_get_childs,
	.ic_get_title	= cablecard_device_class_get_title,
	.ic_properties	= (const property_t[]){
		{
			.type	= PT_BOOL,
			.id	    = "active",
			.name	= N_("Active"),
			.opts	= PO_RDONLY | PO_NOSAVE | PO_NOUI,
			.get	= cablecard_device_class_active_get
		},
		{
			.type	= PT_STR,
			.id	    = "model",
			.name	= N_("Model"),
			.opts	= PO_RDONLY | PO_NOSAVE,
			.off	= offsetof(cablecard_device_t, hd_tuner->model)
		},
		{
			.type	= PT_STR,
			.id	    = "device_id",
			.name	= N_("Device ID"),
			.opts	= PO_RDONLY | PO_NOSAVE,
			.get	= cablecard_device_class_get_device_id
		},
		{
			.type	= PT_STR,
			.id	    = "ip_addr",
			.name	= N_("IP Address"),
			.opts	= PO_RDONLY | PO_NOSAVE,
			.get	= cablecard_device_class_get_ip_address
		},
		{
			.type	= PT_STR,
			.id	    = "uuid",
			.name	= N_("UUID"),
			.opts	= PO_RDONLY | PO_NOSAVE,
			.off	= offsetof(cablecard_device_t, uuid)
		}
	}
};

/* Discovery */
typedef struct cablecard_device_discovery	cablecard_device_discovery_t;

struct	cablecard_device_discovery {
	TAILQ_ENTRY(cablecard_device_discovery)	disc_link;
};

TAILQ_HEAD(cablecard_device_discovery_queue, cablecard_device_discovery);

static int	                                    cablecard_device_discoveries_count;
static struct cablecard_device_discovery_queue	cablecard_device_discoveries;
static pthread_t	                            cablecard_device_discovery_tid;
static pthread_mutex_t	                        cablecard_device_discovery_lock;
static tvh_cond_t	                            cablecard_device_discovery_cond;

static void *
cablecard_device_discovery_thread(void *aux)
{
	struct hdhomerun_discover_device_t	results[MAX_HDHOMERUN_DEVICES];
	int	                                devices, brk;

	while (tvheadend_is_running()) {
		devices = hdhomerun_discover_find_devices_custom(0,
		  HDHOMERUN_DEVICE_TYPE_TUNER, HDHOMERUN_DEVICE_ID_WILDCARD, results,
		  MAX_HDHOMERUN_DEVICES);

		if (devices > 0) {
			while (devices > 0) {
				devices--;
				struct hdhomerun_discover_device_t	*device = &results[devices];
				if (device->device_type == HDHOMERUN_DEVICE_TYPE_TUNER) {
					pthread_mutex_lock(&global_lock);
					cablecard_device_t *existing =
					  cablecard_device_find(device->device_id);
					if (tvheadend_is_running()) {
						if (!existing) {
							cablecard_device_create(device);
							cablecard_device_t *hd;
							hd = cablecard_find_device(device->device_id);
							if (strstr(hd->hd_tuner->model, "_cablecard")) {
								tvhinfo(LS_CABLECARD,
								  "Found HDHomeRun CableCARD device %08X with %d tuners",
								  device->device_id, device->tuner_count);
							} else {
								cablecard_device_destroy(hd);
							}
						} else if (existing->ip_addr != device->ip_addr) {
							tvhinfo(LS_CABLECARD,
							  "HDHomeRun CableCARD device %08X changed IPs: %u.%u.%u.%u -> %u.%u.%u.%u",
							  device->device_id,
							  (unsigned int)(existing->ip_addr >> 24) & 0x0FF,
							  (unsigned int)(existing->ip_addr >> 16) & 0x0FF,
							  (unsigned int)(existing->ip_addr >>  8) & 0x0FF,
							  (unsigned int)(existing->ip_addr >>  0) & 0x0FF,
							  (unsigned int)(device->ip_addr >> 24) & 0x0FF,
							  (unsigned int)(device->ip_addr >> 16) & 0x0FF,
							  (unsigned int)(device->ip_addr >>  8) & 0x0FF,
							  (unsigned int)(device->ip_addr >>  0) & 0x0FF);
							cablecard_device_destroy(existing);
							cablecard_device_create(device);
						}
					}
					pthread_mutex_unlock(&global_lock);
				}
			}
		}

		pthread_mutex_lock(&cablecard_device_discovery_lock);
		brk = 0;
		if (tvheadend_is_running()) {
			brk = tvh_cond_timedwait(&cablecard_device_discovery_cond,
			  &cablecard_device_discovery_lock, mclk() + sec2mono(15));
			brk = !ERRNO_AGAIN(brk) && brk != ETIMEDOUT;
		}
		pthread_mutex_unlock(&cablecard_device_discovery_lock);
		if (brk)
			break;
	}

	return NULL;
}

static void
cablecard_device_discovery_destroy(cablecard_device_discovery_t *d, int unlink)
{
	if (d == NULL)
		return;
	if (unlink) {
		cablecard_device_discoveries--;
		TAILQ_REMOVE(&cablecard_device_discoveries, d, disc_link);
	}
	free(d);
}

/* Type methods */
static void
cablecard_device_calc_bin_uuid(uint8_t *uuid, const uint32_t device_id)
{
	SHA_CTX	sha1;

	SHA1_Init(&sha1);
	SHA1_Update(&sha1, (void *)&device_id, sizeof(device_id));
	SHA1_Final(uuid, &sha1);
}

static void
cablecard_device_calc_uuid(tvh_uuid_t *uuid, const uint32_t device_id)
{
	uint8_t	uuid_bin[20];

	cablecard_device_calc_bin_uuid(uuid_bin, device_id);
	bin2hex(uuid->hex, sizeof(uuid->hex), uuid_bin, sizeof(uuid_bin));
}

static cablecard_device_t *
cablecard_device_find(uint32_t device_id)
{
	tvh_hardware_t	*th;
	uint8_t	         uuid_bin[20];

	cablecard_device_calc_bin_uuid(uuid_bin, device_id);
	TVH_HARDWARE_FOREACH(th) {
		if (idnode_is_instance(&th->th_id, &cablecard_device_class) &&
		  memcmp(th->th_id.in_uuid.bin, uuid_bin, UUID_BIN_SIZE) == 0)
			return (cablecard_device_t *)th;
	}
	return NULL;
}

/*
 * Device creation
 */
static void
cablecard_device_create(struct hdhomerun_discover_device_t *disc)
{
	cablecard_device_t	        *hd = calloc(1, sizeof(cablecard_device_t));
	htsmsg_t	                *conf = NULL;
	htsmsg_t	                *feconf = NULL;
	struct hdhomerun_device_t	*hdhomerun_tuner;
	tvh_uuid_t	                 uuid;
	int	                         j, save = 0;

	/* Ensure this is a CableCARD device */
	hdhomerun_tuner = hdhomerun_device_create(disc->device_info, disc->ip_addr,
	  0, NULL);
	const char *model = hdhomerun_device_get_model_str(hdhomerun_tuner);
	hdhomerun_device_destroy(hdhomerun_tuner);
	if (!strstr(model, "_cablecard"))
		return;

	cablecard_device_calc_uuid(&uuid, disc->device_id);

	conf = hts_settings_load("input/cablecard/adapters/%s", uuid.hex);

	if (!tvh_hardware_create0((tvh_hardware_t *)hd, &cablecard_device_class,
	  uuid.hex, conf))
		return;

	TAILQ_INIT(&hd->hd_frontends);

	/* From tvhdhomerun: */
	/* we may check if uuid matches, but the SHA has should be enough */
	if (hd->uuid)
		free(hd->uuid);

	hd->ip_addr = disc->ip_addr;
	hd->uuid = strdup(uuid.hex);

	if (conf)
		feconf = htsmsg_get_map(conf, "frontends");
	save = !conf || !feconf;

	for (j = 0; j < disc->tuner_count; ++j) {
		if (cablecard_frontend_create(hd, disc, feconf, j)) {
			tvhinfo(LS_CABLECARD, "Created frontend %08X-%d", disc->device_id,
			  j);
		} else {
			tvherror(LS_CABLECARD, "Failed to create frontend %08X-%d",
			  disc->device_id, j);
		}
	}

	if (save)
		cablecard_device_changed(hd);

	htsmsg_destroy(conf);
}

void
cablecard_device_destroy(cablecard_device_t *hd)
{
	cablecard_frontend_t 	*lfe;

	lock_asser(&global_lock);

	mtimer_disarm(&hd->hd_destroy_timer);

	idnode_save_check(&hd->th_id, 0);

	tvhinfo(LS_CABLECARD, "Releasing locks for devices");
	while ((lfe = TAILQ_FIRST(&hd->hd_frontends)) != NULL)
		cablecard_frontend_delete(lfe);

	free(hd->uuid);
	tvh_hardware_delete((tvh_hardware_t *)hd);
	free(hd);
}

/*
 * Physical network init
 */
void
cablecard_device_init(void)
{
	hdhomerun_debug_obj = hdhomerun_debug_create();
	const char *s = getenv("TVHEADEND_HDHOMERUN_DEBUG");

	if (s != NULL && *s) {
		hdhomerun_debug_set_filename(hdhomerun_debug_obj, s);
		hdhomerun_debug_enable(hdhomerun_debug_obj);
	}

	idclass_register(&cablecard_device_class);
	idclass_register(&cablecard_frontend_class);
	TAILQ_INIT(&cablecard_device_discoveries);
	pthread_mutex_init(&cablecard_device_discovery_lock, NULL);
	tvh_cond_init(&cablecard_device_discovery_cond);
	tvhthread_create(&cablecard_device_discovery_tid, NULL,
	  cablecard_device_discovery_thread, NULL, "cc-disc");
}

void
cablecard_device_done(void)
{
	tvh_hardware_t	                *th, *n;
	cablecard_device_discovery_t	*d, *nd;

	pthread_mutex_lock(&cablecard_device_discovery_lock);
	tvh_cond_signal(&cablecard_device_discovery_cond, 0);
	pthread_mutex_unlock(&cablecard_device_discovery_lock);
	pthread_join(cablecard_device_discovery_tid, NULL);

	pthread_mutex_lock(&global_lock);
	for (th = LIST_FIRST(&tvh_hardware); th != NULL; th = n) {
		n = LIST_NEXT(th, th_link);
		if (idnode_is_instance(&th->th_id, &cablecard_device_class))
			cablecard_device_destroy((cablecard_device_t *)th);
	}
	for (d = TAILQ_FIRST(&cablecard_device_discoveries); d != NULL; d = nd) {
		nd = TAILQ_NEXT(d, disc_link);
		cablecard_device_discovery_destroy(d, 1);
	}
	pthread_mutex_unlock(&global_lock);
	hdhomerun_debug_destroy(hdhomerun_debug_obj);
}