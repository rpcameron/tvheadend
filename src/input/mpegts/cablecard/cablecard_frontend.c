/* Tvheadend - HDHomeRun CableCARD frontend
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

#include "input.h"

#include "cablecard_private.h"

/*
 * Class
 */
/* Methods */
static void
cablecard_frontend_class_changed(idnode_t *in)
{
	cablecard_device_t	*la = ((cablecard_frontend_t *)in)->hf_device;

	cablecard_device_changed(la);
}

/* Class definition */
extern const idclass_t	mpegts_input_class;
const idclass_t	        cablecard_frontend_class = {
	.ic_super	    = &mpegts_input_class,
	.ic_class	    = "cablecard_frontend",
	.ic_caption	    = N_("HDHomeRun CableCARD frontend"),
	.ic_changed	    = cablecard_frontend_class_changed,
	.ic_properties	= (const property_t[]){
		{
			.type	= PT_INT,
			.id	    = "fe_number",
			.name	= N_("Frontend number"),
			.opts	= PO_RDONLY | PO_NOSAVE,
			.off	= offsetof(cablecard_frontend_t, hf_tuner->tuner),
		}
	}
};

/*
 * Methods
 */
static int
cablecard_frontend_get_weight(mpegts_input_t *mi, mpegts_mux_t *mm, int flags,
  int weight)
{
	return mpegts_input_get_weight(mi, mm, flags, weight);
}

static int
cablecard_frontend_get_priority(mpegts_input_t *mi, mpegts_mux_t *mm, int flags)
{
	return mpegts_input_get_priority(mi, mm, flags);
}

static int
cablecard_frontend_get_grace(mpegts_input_t *mi, mpegts_mux_t *mm)
{
	return 15;
}

static int
cablecard_frontend_is_enabled(mpegts_input_t *mi, mpegts_mux_t *mm, int flags,
  int weight)
{
	return mpegts_input_is_enabled(mi, mm, flags, weight);
}

/* This entire method is pulled nearly verbatim from tvhdhomerun */
static void *
cablecard_frontend_input_thread(void *aux)
{
	cablecard_frontend_t	*hfe = (cablecard_frontend_t *)aux;
	mpegts_mux_instance_t	*mmi;
	sbuf_t	                 sb;
	char	                 buf[256];
	char	                 target[64];
	uint32_t	             local_ip;
	int	                     sockfd, nfds;
	int	                     sock_opt = 1;
	int	                     r;
	int	                     rx_size = 1024 * 1024;
	struct sockaddr_in	     sock_addr;
	socklen_t	             sockaddr_len = sizeof(sock_addr);
	tvhpoll_event_t	         ev[2];
	tvhpoll_t	            *efd;

	tvhdebug(LS_CABLECARD, "starting input thread");

	/* Get MMI */
	pthread_mutex_lock(&hfe->hf_input_thread_mutex);
	hfe->mi_display_name((mpegts_input_t*)hfe, buf, sizeof(buf));
	mmi = LIST_FIRST(&hfe->mi_mux_active);
	tvh_cond_signal(&hfe->hf_input_thread_cond, 0);
	pthread_mutex_unlock(&hfe->hf_input_thread_mutex);
	if (mmi == NULL)
		return NULL;

	tvhdebug(LS_CABLECARD, "opening client socket");

	/* One would like to use libhdhomerun for the streaming details,
	 * but that library uses threads on its own and the socket is put
	 * into a ring buffer. That makes it less practical to use here,
	 * so we do the whole UPD recv() stuff ourselves. And we can assume
	 * POSIX here ;)
	 */

	/* local IP */
	/* TODO: this is nasty */
	local_ip = hdhomerun_device_get_local_machine_addr(hfe->hf_tuner);

	/* first setup a local socket for the device to stream to */
	sockfd = tvh_socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd == -1) {
		tvherror(LS_CABLECARD, "failed to open socket (%d)", errno);
		return NULL;
	}

	if (fcntl(sockfd, F_SETFL, O_NONBLOCK) != 0) {
		close(sockfd);
		tvherror(LS_CABLECARD, "failed to set socket nonblocking (%d)", errno);
		return NULL;
	}

	/* enable broadcast */
	if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, (char *)&sock_opt,
	  sizeof(sock_opt)) < 0) {
		close(sockfd);
		tvherror(LS_CABLECARD, "failed to enable broadcast on socket (%d)",
		  errno);
		return NULL;
	}

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&sock_opt,
	  sizeof(sock_opt)) < 0) {
		close(sockfd);
		tvherror(LS_CABLECARD, "failed to set address reuse on socket (%d)",
		  errno);
		return NULL;
	}

	/* important: we need large rx buffers to accomodate the large amount of traffic */
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char *)&rx_size,
	  sizeof(rx_size)) < 0) {
		tvhwarn(LS_CABLECARD,
		  "failed set socket rx buffer size, expect CC errors (%d)", errno);
	}

	memset(&sock_addr, 0, sizeof(sock_addr));
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	sock_addr.sin_port = 0;
	if (bind(sockfd, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) != 0) {
		tvherror(LS_CABLECARD, "failed bind socket: %d", errno);
		close(sockfd);
		return NULL;
	}

	memset(&sock_addr, 0, sizeof(sock_addr));
	if (getsockname(sockfd, (struct sockaddr *)&sock_addr, &sockaddr_len) != 0) {
		tvherror(LS_CABLECARD, "failed to getsockname: %d", errno);
		close(sockfd);
		return NULL;
	}

	/* pretend we are smart and set video_socket; doubt that this is required though */
	//hfe->hf_hdhomerun_tuner->vs = sockfd;

	/* and tell the device to stream to the local port */
	memset(target, 0, sizeof(target));
	snprintf(target, sizeof(target), "udp://%u.%u.%u.%u:%u",
		(unsigned int)(local_ip >> 24) & 0xFF,
		(unsigned int)(local_ip >> 16) & 0xFF,
		(unsigned int)(local_ip >>  8) & 0xFF,
		(unsigned int)(local_ip >>  0) & 0xFF,
		ntohs(sock_addr.sin_port));
	tvhdebug(LS_CABLECARD, "setting target to: %s", target);
	pthread_mutex_lock(&hfe->hf_tuner_mutex);
	r = hdhomerun_device_set_tuner_target(hfe->hf_tuner, target);
	pthread_mutex_unlock(&hfe->hf_tuner_mutex);
	if (r < 1) {
		tvherror(LS_CABLECARD, "failed to set target: %d", r);
		return NULL;
	}

	/* the poll set includes the sockfd and the pipe for IPC */
	efd = tvhpoll_create(2);
	memset(ev, 0, sizeof(ev));
	ev[0].events             = TVHPOLL_IN;
	ev[0].fd = ev[0].data.fd = sockfd;
	ev[1].events             = TVHPOLL_IN;
	ev[1].fd = ev[1].data.fd = hfe->hf_input_thread_pipe.rd;

	r = tvhpoll_add(efd, ev, 2);
	if (r < 0)
		tvherror(LS_CABLECARD, "failed to setup poll");

	sbuf_init_fixed(&sb, (20000000 / 8));

	/* TODO: flush buffer? */

	while (tvheadend_is_running()) {
		nfds = tvhpoll_wait(efd, ev, 1, -1);

		if (nfds < 1)
			continue;
		if (ev[0].data.fd != sockfd)
			break;

		if ((r = sbuf_tsdebug_read(mmi->mmi_mux, &sb, sockfd)) < 0) {
			/* whoopsy */
			if (ERRNO_AGAIN(errno))
				continue;
			if (errno == EOVERFLOW) {
				tvhwarn(LS_CABLECARD, "%s - read() EOVERFLOW", buf);
				continue;
			}
			tvherror(LS_CABLECARD, "%s - read() error %d (%s)", buf, errno,
			  strerror(errno));
			break;
		}

		mpegts_input_recv_packets((mpegts_input_t*)hfe, mmi, &sb, 0, NULL);
	}

	tvhdebug(LS_CABLECARD, "setting target to none");
	pthread_mutex_lock(&hfe->hf_tuner_mutex);
	hdhomerun_device_set_tuner_target(hfe->hf_tuner, "none");
	pthread_mutex_unlock(&hfe->hf_tuner_mutex);

	sbuf_free(&sb);
	tvhpoll_destroy(efd);
	close(sockfd);
	return NULL;
}

static void
cablecard_frontend_monitor_cb(void *aux)
{
	cablecard_frontend_t	*hfe = (cablecard_frontend_t *)aux;
	mpegts_mux_instance_t	*mmi = LIST_FIRST(&hfe->mi_mux_active);
	mpegts_mux_t	        *mm;
	streaming_message_t	     sm;
	signal_status_t	         sigstat;
	service_t	            *svc;
	int	                     res, e;

	struct hdhomerun_tuner_status_t	    status;
	struct hdhomerun_tuner_vstatus_t	vstatus;
	char	                           *status_str;
	char	                           *vstatus_str;
	char	                           *program;

	/* Stop timer */
	if (!mmi || !hfe->hf_ready)
		return;

	/* Re-arm timer */
	mtimer_arm_rel(&hfe->hf_monitor_timer, cablecard_frontend_monitor_cb, hfe,
	  sec2mono(1));

	/* Get current statuses */
	pthread_mutex_lock(&hfe->hf_tuner_mutex);
	res = hdhomerun_device_get_tuner_status(hfe->hf_tuner, &status_str,
	  &status);
	if (res < 1)
		tvhwarn(LS_CABLECARD, "tuner_status (%d)", res);
	res = hdhomerun_device_get_tuner_vstatus(hfe->hf_tuner, &vstatus_str,
	  &vstatus);
	if (res < 1)
		tvhwarn(LS_CABLECARD, "tuner_vstatus (%d)", res);
	res = hdhomerun_device_get_tuner_program(hfe->hf_tuner, &program);
	pthread_mutex_unlock(&hfe->hf_tuner_mutex);
	if (res < 1)
		tvhwarn(LS_CABLECARD, "tuner_program (%d)", res);

	if (status.signal_present)
		hfe->hf_status = SIGNAL_GOOD;
	else
		hfe->hf_status = SIGNAL_NONE;

	/* Get current mux */
	mm = mmi->mmi_mux;

	/* Wait for signal_present */
	if (!hfe->hf_locked) {
		if (status.signal_present) {
			tvhdebug(LS_CABLECARD, "locked");
			hfe->hf_locked = 1;

			/* Update mux/service vars */
			if (!mm->mm_cablecard_name ||
			  strcmp(mm->mm_cablecard_name, vstatus.name)) {
				free(mm->mm_cablecard_name);
				mm->mm_cablecard_name = strdup(vstatus.name);
			}

			/* Start input thread */
			tvh_pipe(0_NONBLOCK, &hfe->hf_input_thread_pipe);
			pthread_mutex_lock(&hfe->hf_input_thread_mutex);
			tvhthread_create(&hfe->hf_input_thread, NULL,
			  cablecard_frontend_input_thread, hfe, "cc-front");
			do {
				e = tvh_cond_wait(&hfe->hf_input_thread_cond,
				  &hfe->hf_input_thread_mutex);
				if (e == TIMEDOUT)
					break;
			} while (ERRNO_AGAIN(e));
			pthread_mutex_unlock(&hfe->hf_input_thread_mutex);

			/* Install table handlers */
			psi_tables_install(mmi->mmi_input, mm, DVB_SYS_ATSC_ALL);
		} else {
			/* Re-arm timer for signal lock */
			mtimer_arm_rel(&hfe->hf_monitor_timer,
			  cablecard_frontend_monitor_cb, hfe, ms2mono(50));
		}
	}

	pthread_mutex_lock(&mmi->tii_stats_mutex);
	if (status.signal_present) {
		/* quick & dirty conversion */
		mmi->tii_status.signal = status.signal_strength * 655.35;
		mmi->tii_stats.snr = status.signal_to_noise_quality * 655.35;
	} else {
		mmi->tii_status.snr = 0;
	}

	sigstat.status_text  = signal2str(hfe->hf_status);
	sigstat.snr          = mmi->tii_stats.snr;
	sigstat.snr_scale    = mmi->tii_stats.snr_scale = SIGNAL_STATUS_SCALE_RELATIVE;
	sigstat.signal       = mmi->tii_stats.signal;
	sigstat.signal_scale = mmi->tii_stats.signal_scale = SIGNAL_STATUS_SCALE_RELATIVE;
	sigstat.ber          = mmi->tii_stats.ber;
	sigstat.unc          = atomic_get(&mmi->tii_stats.unc);
	sm.sm_type           = SMT_SIGNAL_STATUS;
	sm.sm_data           = &sigstat;

	pthread_mutex_unlock(&mmi->tii_stats_mutex);

	LIST_FOREACH(svc, &mmi->mmi_mux->mm_transports, s_active_link) {
		pthread_mutex_lock(&svc->s_stream_mutex);
		streaming_pad_deliver(&svc->s_streaming_pad, streaming_msg_clone(&sm));
		pthread_mutex_unlock(&svc->s_stream_mutex);
	}
}

static int
cablecard_frontend_tune(cablecard_frontend_t *hfe, mpegts_mux_instance_t *mmi)
{
	cablecard_mux_t	*lm = (cablecard_mux_t *)mmi->mmi_mux;
	char	        *perror;
	char	         vchan_buf[16];
	int	             res;

	tvhinfo(LS_CABLECARD, "Tuning to %u", lm->mm_cablecard_vchan);
		pthread_mutex_lock(&hfe->hf_tuner_mutex);
	res = hdhomerun_device_tuner_lockkey_request(hfe->hf_tuner, &perror);
	if (res < 1) {
		pthread_mutex_unlock(&hfe->hf_tuner_mutex);
		tvherror(LS_CABLECARD, "Failed to acquire lockkey: %s", perror);
		return SM_CODE_TUNING_FAILED;
	}
	snprintf(vchan_buf, sizeof(vchan_buf), "%u", lm->mm_cablecard_vchan);
	res = hdhomerun_device_set_tuner_vchannel(hfe->hf_tuner, vchan_buf);
	pthread_mutex_unlock(&hfe->hf_tuner_mutex);
	if (res < 1) {
		tvherror(LS_CABLECARD, "Failed to tune to %u", lm->mm_cablecard_vchan);
		return SM_CODE_TUNING_FAILED;
	}

	hfe->hf_status = SIGNAL_NONE;

	/* Start monitoring */
	mtimer_arm_rel(&hfe->hf_monitor_timer, cablecard_frontend_monitor_cb, hfe,
	  ms2mono(50));
	hfe->hf_ready = 1;

	return 0;
}

static int
cablecard_frontend_start_mux(mpegts_input_t *mi, mpegts_mux_instance_t *mmi,
  int weight)
{
	cablecard_frontend_t	*hfe = (cablecard_frontend_t *)mi;
	char	                 ubuf1[256];
	char	                 ubuf2[256];
	int	                     res;

	mi->mi_display_name(mi, buf1, sizeof(buf1));
	mpegts_mux_nice_name(mmi->mmi_mux, buf2, sizeof(buf2));
	tvhdebug(LS_CABLECARD, "%s - starting %s", buf1, buf2);

	/* Tune to the mux */
	res = cablecard_frontend_tune(hfe, mmi);
	return res;
}

static void
cablecard_frontend_stop_mux(mpegts_input_t *mi, mpegts_mux_instance_t *mmi)
{
	cablecard_frontend_t	*hfe = (cablecard_frontend_t *)mi;
	char	                 buf1[256];
	char	                 buf2[256];

	mi->mi_display_name(mi, buf1, sizeof(buf1));
	mpegts_mux_nice_name(mmi->mmi_mux, buf2, sizeof(buf2));
	tvhdebug(LS_CABLECARD, "%s - stopping %s", buf1, buf2);

	/* Join input thread */
	if (hfe->hf_input_thread_pipe.wr > 0) {
		tvh_write(hfe->hf_input_thread_pipe.wr, "", 1);
		tvhtrace(LS_CABLECARD, "%s - waiting for input thread", buf1);
		pthread_join(hfe->hf_input_thread, NULL);
		tvh_pipe_close(&hfe->hf_input_thread_pipe);
		tvhtrace(LS_CABLECARD, "%s - input thread stopped", buf1);
	}

	hdhomerun_device_tuner_lockkey_release(hfe->hf_tuner);

	hfe->hf_locked = 0;
	hfe->hf_status = 0;
	hfe->hf_ready  = 0;

	mtimer_arm_rel(&hfe->hf_monitor_timer, cablecard_frontend_monitor_cb, hfe,
	  sec2mono(2));
}

void
cablecard_frontend_save(cablecard_frontend_t *hfe, htsmsg_t *fe)
{
	htsmsg_t	*m = htsmsg_create_map();
	char	     ubuf[UUID_HEX_SIZE];
	char	     id[16];

	/* Save frontend */
	mpegts_input_save((mpegts_input_t *)hfe, m);
	htsmsg_add_str(m, "uuid", idnode_uuid_as_str(&hfe->ti_id, ubuf));

	/* Add to list */
	snprintf(id, sizeof(id), "%08X-%u", hfe->hf_tuner->device_id,
	  hfe->hf_tuner->tuner);
	htsmsg_add_msg(fe, id, m);
}

static mpegts_network_t *
cablecard_frontend_wizard_network(cablecard_frontend_t *hfe)
{
	return (mpegts_network_t *)LIST_FIRST(&hfe->mi_networks);
}

static htsmsg_t *
cablecard_frontend_wizard_get(tvh_input_t *ti, const char *lang)
{
	cablecard_frontend_t	*hfe = (cablecard_network_t *)ti;
	mpegts_network_t	    *mn;
	const idclass_t	        *idc = NULL;

	mn = cablecard_frontend_wizard_network(hfe);
	if ((mn == NULL) || (mn && mn->mn_wizard))
		idc = cablecard_network_class;
	return mpegts_network_wizard_get((mpegts_input_t *)hfe, idc, mn, lang);
}

static void
cablecard_frontend_wizard_set(tvh_input_t *ti, htsmsg_t *conf, const char *lang)
{
	cablecard_frontend_t	*hfe = (cablecard_frontend_t *)ti;
	mpegts_network_t	    *mn;
	const char	            *ntype = htsmsg_get_str(conf, "mepgts_network_type");
	htsmsg_t	            *nlist;

	mn = cablecard_frontend_wizard_network(hfe);
	mpegts_network_wizard_create(ntype, &nlist, lang);
	if (ntype && (mn == NULL || mn->mn_wizard)) {
		htsmsg_add_str(nlist, NULL, ntype);
		mpegts_input_set_networks((mpegts_input_t *)hfe, nlist);
		htsmsg_destroy(nlist);
		if (cablecard_frontend_wizard_network(hfe))
			mpegts_input_set_enabled((mpegts_input_t *)hfe, 1);
		cablecard_device_changed(hfe->hf_device);
	} else {
		htsmsg_destroy(nlist);
	}
}

/*
 * Creation
 */
cablecard_frontend_t *
cablecard_frontend_create(cablecard_device_t *hd,
  struct hdhomerun_discover_device_t *disc, htsmsg_t *conf, unsigned int fe)
{
	const idclass_t	        *idc = &cablecard_frontend_class;
	cablecard_frontend_t	*hfe = calloc(1, sizeof(cablecard_frontend_t));
	const char	            *uuid = NULL;
	char	                 id[16];

	/* Internal config ID */
	snprintf(id, sizeof(id), "%08X-%u", disc->device_id, fe);
	if (conf)
		conf = htsmsg_get_map(conf, id);
	if (conf)
		uuid = htsmsg_get_str(conf, "uuid");

	hfe = (cablecard_frontend_t *)mpegts_input_create0((mpegts_input_t *)hfe,
	  idc, uuid, conf);
	if (!hfe)
		return NULL;

	hfe->hf_device                   = hd;
	hfe->hf_tuner                    = hdhomerun_discover_create(disc->device_id,
	  disc->ip_addr, fe, hdhomerun_debug_obj);
	hfe->hf_input_thread_running     = 0;
	hfe->hf_input_thread_terminating = 0;
	hfe->mi_get_weight               = cablecard_frontend_get_weight;
	hfe->mi_get_priority             = cablecard_frontend_get_priority;
	hfe->mi_get_grace                = cablecard_frontend_get_grace;

	if (!hfe->mi_name) {
		char	name[256];
		snprintf(name, sizeof(name), "HDHomeRun CableCARD Tuner %08X-%u",
		  disc->device_id, fe)
		free(hfe->mi_name);
		hfe->mi_name = strdup(name);
	}

	/* Input callbacks */
	hfe->mi_is_enabled   = cablecard_frontend_is_enabled;
	hfe->mi_empty_status = mpegts_input_empty_status;
	hfe->mi_start_mux    = cablecard_frontend_start_mux;
	hfe->mi_stop_mux     = cablecard_frontend_stop_mux;
	hfe->ti_wizard_get   = cablecard_frontend_wizard_get;
	hfe->ti_wizard_set   = cablecard_frontend_wizard_set;


	/* Adapter link */
	TAILQ_INSERT_TAIL(&hd->hd_frontends, hfe, hf_link);

	/* Mutex init */
	pthread_mutex_init(&hfe->hf_tuner_mutex, NULL);
	pthread_mutex_init(&hfe->hf_input_thread_mutex, NULL);
	tvh_cond_init(&hfe->hf_input_thread_cond);

	return hfe;
}

void
cablecard_frontend_delete(cablecard_frontend_t *hfe)
{
	lock_assert(&global_lock);

	/* Ensure stopped */
	mpegts_input_stop_all((mpegts_input_t *)hfe);

	mtimer_disarm(&hfe->hf_monitor_timer);

	hdhomerun_device_tuner_lockkey_release(hfe->hf_tuner);
	hdhomerun_device_destroy(hfe->hf_tuner);

	/* Remove from adapter */
	TAILQ_REMOVE(&hfe->hf_device->hd_frontends, hfe, hf_link);

	pthread_mutex_destroy(&hfe->hf_input_thread_mutex);
	pthread_mutex_destroy(&hfe->hf_tuner_mutex);

	/* Finish */
	mpegts_input_delete((mpegts_input_t *)hfe, 0);
}