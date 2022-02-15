/* IKEv2 LIVENESS probe
 *
 * Copyright (C) 1997 Angelos D. Keromytis.
 * Copyright (C) 1998-2001  D. Hugh Redelmeier.
 * Copyright (C) 2005-2008 Michael Richardson <mcr@xelerance.com>
 * Copyright (C) 2008-2010 Paul Wouters <paul@xelerance.com>
 * Copyright (C) 2009 David McCullough <david_mccullough@securecomputing.com>
 * Copyright (C) 2012 Avesh Agarwal <avagarwa@redhat.com>
 * Copyright (C) 2012-2019 Paul Wouters <pwouters@redhat.com>
 * Copyright (C) 2013 Matt Rogers <mrogers@redhat.com>
 * Copyright (C) 2017 Antony Antony <antony@phenome.org>
 * Copyright (C) 2017-2019 Andrew Cagney <cagney@gnu.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <https://www.gnu.org/licenses/gpl2.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include "defs.h"
#include "state.h"
#include "log.h"
#include "connections.h"
#include "iface.h"
#include "kernel.h"
#include "ikev2_send.h"
#include "pluto_stats.h"
#include "timer.h"
#include "server.h"
#include "ikev2.h"			/* for struct v2_state_transition */
#include "ikev2_liveness.h"
#include "ikev2_states.h"

static stf_status send_v2_liveness_request(struct ike_sa *ike,
					   struct child_sa *child UNUSED,
					   struct msg_digest *md UNUSED)
{
	/*
	 * XXX: What does it mean to send a liveness probe for a CHILD
	 * SA?  Since the packet contents are empty there's nothing
	 * for the other end to identify which child this is for!
	 *
	 * XXX: See record 'n'_send for how screwed up all this is:
	 * need to pass in the CHILD SA so that its liveness
	 * timestamp (and not the IKE) gets updated.
	 */
	pstats_ike_dpd_sent++;
	stf_status e = record_v2_informational_request("liveness probe informational request",
						       ike, &ike->sa/*sender*/,
						       NULL/*no payloads to emit*/);
	if (e != STF_OK) {
		return STF_INTERNAL_ERROR;
	}
	return STF_OK;
}

static void schedule_liveness(struct child_sa *child, deltatime_t time_since_last_exchange,
			      const char *reason)
{
	struct connection *c = child->sa.st_connection;
	deltatime_t delay = c->dpd_delay;
	/* reduce wait if contact was by some other means */
	delay = deltatime_sub(delay, time_since_last_exchange);
	/* in case above screws up? */
	delay = deltatime_max(c->dpd_delay, deltatime(MIN_LIVENESS));
	LSWDBGP(DBG_BASE, buf) {
		deltatime_buf db;
		endpoint_buf remote_buf;
		jam(buf, "liveness: #%lu scheduling next check for %s in %s seconds",
		    child->sa.st_serialno,
		    str_endpoint(&child->sa.st_remote_endpoint, &remote_buf),
		    str_deltatime(delay, &db));
		if (deltatime_cmp(time_since_last_exchange, !=, deltatime(0))) {
			deltatime_buf lcb;
			jam(buf, " (%s was %s seconds ago)",
			    reason, str_deltatime(time_since_last_exchange, &lcb));
		} else {
			jam(buf, " (%s)", reason);
		}
	}
	event_schedule(EVENT_v2_LIVENESS, delay, &child->sa);
}

/*
 * The RFC (2.4.  State Synchronization and Connection Timeouts) as
 * this to say:
 *
 *   To be a good network citizen, retransmission times MUST increase
 *   exponentially to avoid flooding the network and making an
 *   existing congestion situation worse.  If there has only been
 *   outgoing traffic on all of the SAs associated with an IKE SA, it
 *   is essential to confirm liveness of the other endpoint to avoid
 *   black holes.  If no cryptographically protected messages have
 *   been received on an IKE SA or any of its Child SAs recently, the
 *   system needs to perform a liveness check in order to prevent
 *   sending messages to a dead peer.  (This is sometimes called "dead
 *   peer detection" or "DPD", although it is really detecting live
 *   peers, not dead ones.)  Receipt of a fresh cryptographically
 *   protected message on an IKE SA or any of its Child SAs ensures
 *   liveness of the IKE SA and all of its Child SAs.
 *
 * However:
 *
 * Only checking incoming packets does not demonstrate liveness.  Just
 * that half the channel is working.  All the incoming packets could
 * be retransmits.
 *
 * note: this mutates *st by calling get_sa_info
 */

void liveness_check(struct state *st)
{
	monotime_t now = mononow();

	passert(st->st_ike_version == IKEv2);
	struct ike_sa *ike = ike_sa(st, HERE);
	if (ike == NULL) {
		/* already logged */
		dbg("liveness: state #%lu has no IKE SA; deleting orphaned child",
		    st->st_serialno);
		event_delete(EVENT_SA_DISCARD, st);
		event_schedule(EVENT_SA_DISCARD, deltatime(0), st);
		return;
	}
	struct child_sa *child = pexpect_child_sa(st);
	if (child == NULL) {
		return;
	}

	struct connection *c = child->sa.st_connection;

	/*
	 * If the child is lingering (replaced but not yet deleted),
	 * don't do liveness.
	 */
	if (c->newest_ipsec_sa != child->sa.st_serialno) {
		dbg("liveness: #%lu was replaced by #%lu so not needed",
		    child->sa.st_serialno, c->newest_ipsec_sa);
		return;
	}

	/*
	 * If the IKE SA is waiting for a response to it's last
	 * request, reschedule the liveness probe.
	 *
	 * If the exchange succeeds, there's been a round trip and
	 * things are alive.
	 *
	 * If the exchange fails, liveness will be triggered.
	 */
	if (v2_msgid_request_outstanding(ike)) {
		schedule_liveness(child, /*time-since-last-exchange*/deltatime(0),
				  "request outstanding");
		return;
	}

	/*
	 * If the IKE SA has a request outstanding, reschedule the
	 * liveness probe.
	 *
	 * For instance, the last exchange list finished, and the next
	 * exchange is about to start.  If that exchange fails
	 * liveness will be triggered.
	 */
	if (v2_msgid_request_pending(ike)) {
		schedule_liveness(child, /*time-since-last-exchange*/deltatime(0),
				  "request pending");
		return;
	}

	/*
	 * If this IKE SA recently completed an exchange, reschedule
	 * the liveness probe.
	 *
	 * Since this end initiated the exchange and got a response, a
	 * recent round-trip probe worked.
	 */
	struct v2_msgid_window *our = &ike->sa.st_v2_msgid_windows.initiator;
	pexpect(!is_monotime_epoch(our->last_contact));
	deltatime_t time_since_our_last_exchange = monotimediff(now, our->last_contact);
	if (deltatime_cmp(time_since_our_last_exchange, <, c->dpd_delay)) {
		schedule_liveness(child, time_since_our_last_exchange, "successful exchange");
		return;
	}

	/*
	 * If this IKE SA recently received a new message request from
	 * the peer, reschedule the liveness probe.
	 *
	 * The arrival of a new message request #N can only happen
	 * (ignoring mobike) once the peer has received our response
	 * to the previous message request #N-1.
	 *
	 * The only issue is that while the PEER->US message was
	 * recent, the US->PEER message could be ancient.  Not to
	 * worry, the next liveness check will pick this up.  The
	 * alternative is to save second-last-contact and use that.
	 *
	 * The more likely scenario is that the other end is
	 * constantly sending liveness probes so this end can skip
	 * them.
	 */
	struct v2_msgid_window *peer = &ike->sa.st_v2_msgid_windows.responder;
	deltatime_t time_since_peer_contact = monotimediff(now, peer->last_contact);
	if (deltatime_cmp(time_since_peer_contact, <, c->dpd_delay)) {
		schedule_liveness(child, time_since_peer_contact, "peer contact");
		return;
	}

	/*
	 * If there's been recent traffic flowing in through the CHILD
	 * SA and it was less than .dpd_delay ago then re-schedule the
	 * probe.
	 *
	 * Per above, the RFC says:
	 *
	 *   If no cryptographically protected messages have been
	 *   received on ... Child SAs recently,
	 *
	 * XXX: But is this useful?  Liveness should be checking
	 * round-trip but this is just looking at incoming data -
	 * outgoing data could lost and this traffic is all
	 * re-transmit requests ...
 	 */
	deltatime_t time_since_last_inbound_message;
	if (get_sa_info(&child->sa, /*inbound*/true, &time_since_last_inbound_message) &&
	    deltatime_cmp(time_since_last_inbound_message, <, c->dpd_delay)) {
		/*
		 * Update .st_liveness_last, with the time of this
		 * traffic (unless other traffic is more recent).
		 */
		monotime_t last_contact = monotime_sub(now, time_since_last_inbound_message);
		if (monobefore(our->last_contact, last_contact)) {
			monotime_buf m0, m1;
			dbg("liveness: #%lu updating #%lu last contact from %s to %s (last IPsec traffic flow)",
			    child->sa.st_serialno, ike->sa.st_serialno,
			    str_monotime(our->last_contact, &m0),
			    str_monotime(last_contact, &m1));
			our->last_contact = last_contact;
		}
		/*
		 * schedule in .dpd_delay seconds, but adjust for:
		 * time since last traffic, and min liveness vis
		 *
		 * max(dpd_delay - time_since_last_message, * deltatime(MIN_LIVENESS))
		 */
		schedule_liveness(child, time_since_last_inbound_message, "recent IPsec traffic");
 		return;
 	}

	endpoint_buf remote_buf;
	dbg("liveness: #%lu queueing liveness probe for %s using #%lu",
	    child->sa.st_serialno,
	    str_endpoint(&child->sa.st_remote_endpoint, &remote_buf),
	    ike->sa.st_serialno);
	submit_v2_liveness_exchange(ike, child->sa.st_serialno);

	/* in case above screws up? */
	schedule_liveness(child, deltatime(0), "backup for liveness probe");
}

/*
 * XXX: where to put this?
 */

static const struct v2_state_transition v2_liveness_probe = {
	.story = "liveness probe",
	.state = STATE_V2_ESTABLISHED_IKE_SA,
	.next_state = STATE_V2_ESTABLISHED_IKE_SA,
	.send = MESSAGE_REQUEST,
	.processor = send_v2_liveness_request,
	.timeout_event =  EVENT_RETAIN,
	.flags = SMF2_SUPPRESS_SUCCESS_LOG,
};

void submit_v2_liveness_exchange(struct ike_sa *ike, so_serial_t who_for)
{
	const struct v2_state_transition *transition = &v2_liveness_probe;
	if (ike->sa.st_state->kind != transition->state) {
		llog_sa(RC_LOG, ike,
			"liveness: IKE SA in state %s but should be %s; liveness for #%lu ignored",
			ike->sa.st_state->short_name,
			finite_states[transition->state]->short_name,
			who_for);
		return;
	}

	v2_msgid_queue_initiator(ike, NULL, NULL,
				 ISAKMP_v2_INFORMATIONAL,
				 transition);
}
