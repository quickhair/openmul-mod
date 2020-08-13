/*
 *  memtrack.c: Hello application for MUL Controller 
 *  Copyright (C) 2012, Dipjyoti Saikia <dipjyoti.saikia@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include "config.h"
#include "mul_common.h"
#include "mul_vty.h"
#include "memtrack.h"
#include "malloc.h"

struct event *memtrack_timer;
struct mul_app_client_cb memtrack_app_cbs;

/**
 * memtrack_install_dfl_flows -
 * Installs default flows on a switch
 *
 * @dpid : Switch's datapath-id
 * @return : void
 */
static void
memtrack_install_dfl_flows(uint64_t dpid)
{
    struct flow                 fl;
    struct flow                 mask;

    memset(&fl, 0, sizeof(fl));
    of_mask_set_dc_all(&mask);

    /* Clear all entries for this switch */
    mul_app_send_flow_del(MTRACK_APP_NAME, NULL, dpid, &fl,
                          &mask, OFPP_NONE, 0, C_FL_ENT_NOCACHE, OFPG_ANY);

    /* Zero DST MAC Drop */
    of_mask_set_dl_dst(&mask); 
    mul_app_send_flow_add(MTRACK_APP_NAME, NULL, dpid, &fl, &mask,
                          MTRACK_UNK_BUFFER_ID, NULL, 0, 0, 0, 
                          C_FL_PRIO_DRP, C_FL_ENT_NOCACHE);  

    /* Zero SRC MAC Drop */
    of_mask_set_dc_all(&mask);
    of_mask_set_dl_src(&mask); 
    mul_app_send_flow_add(MTRACK_APP_NAME, NULL, dpid, &fl, &mask, 
                          MTRACK_UNK_BUFFER_ID, NULL, 0, 0, 0,  
                          C_FL_PRIO_DRP, C_FL_ENT_NOCACHE);

    /* Broadcast SRC MAC Drop */
    memset(&fl.dl_src, 0xff, OFP_ETH_ALEN);
    mul_app_send_flow_add(MTRACK_APP_NAME, NULL, dpid, &fl, &mask,
                          MTRACk_UNK_BUFFER_ID, NULL, 0, 0, 0,
                          C_FL_PRIO_DRP, C_FL_ENT_NOCACHE);

    /* Send any unknown flow to app */
    memset(&fl, 0, sizeof(fl));
    of_mask_set_dc_all(&mask);
    mul_app_send_flow_add(MTRACK_APP_NAME, NULL, dpid, &fl, &mask,
                          MTRACK_UNK_BUFFER_ID, NULL, 0, 0, 0,
                          C_FL_PRIO_DFL, C_FL_ENT_LOCAL);
}


/**
 * memtrack_sw_add -
 * Switch join event notifier
 * 
 * @sw : Switch arg passed by infra layer
 * @return : void
 */
static void 
memtrack_sw_add(mul_switch_t *sw)
{
    /* Add few default flows in this switch */
    memtrack_install_dfl_flows(sw->dpid);
    c_log_debug("switch dpid 0x%llx joined network", (unsigned long long)(sw->dpid));
}

/**
 * memtrack_sw_del -
 * Switch delete event notifier
 *
 * @sw : Switch arg passed by infra layer
 * @return : void
 */
static void
memtrack_sw_del(mul_switch_t *sw)
{
    c_log_debug("switch dpid 0x%llx left network", (unsigned long long)(sw->dpid));
}

/**
 * memtrack_packet_in -
 * Memtrack app's packet-in notifier call-back
 *
 * @sw : switch argument passed by infra layer (read-only)
 * @fl : Flow associated with the packet-in
 * @inport : in-port that this packet-in was received
 * @raw : Raw packet data pointer
 * @pkt_len : Packet length
 * 
 * @return : void
 */
static void 
memtrack_packet_in(mul_switch_t *sw UNUSED,
                struct flow *fl UNUSED,
                uint32_t inport UNUSED,
                uint32_t buffer_id UNUSED,
                uint8_t *raw UNUSED,
                size_t pkt_len UNUSED)
{
    c_log_info("memtrack app - packet-in from network");
    struct mallinfo mi;

    mi = mallinfo();

    c_log_info("###FC### Total non-mmapped bytes (arena):       %d\n", mi.arena);
    c_log_info("# of free chunks (ordblks):            %d\n", mi.ordblks);
    c_log_info("# of free fastbin blocks (smblks):     %d\n", mi.smblks);
    c_log_info("# of mapped regions (hblks):           %d\n", mi.hblks);
    c_log_info("Bytes in mapped regions (hblkhd):      %d\n", mi.hblkhd);
    c_log_info("Max. total allocated space (usmblks):  %d\n", mi.usmblks);
    c_log_info("Free bytes held in fastbins (fsmblks): %d\n", mi.fsmblks);
    c_log_info("Total allocated space (uordblks):      %d\n", mi.uordblks);
    c_log_info("Total free space (fordblks):           %d\n", mi.fordblks);
    c_log_info("Topmost releasable block (keepcost):   %d\n", mi.keepcost);

    return;
}

/**
 * memtrack_core_closed -
 * mul-core connection drop notifier
 */
static void
memtrack_core_closed(void)
{
    c_log_info("%s: ", FN);

    /* Nothing to do */
    return;
}

/**
 * memtrack_core_reconn -
 * mul-core reconnection notifier
 */
static void
memtrack_core_reconn(void)
{
    c_log_info("%s: ", FN);

    /* 
     * Once core connection has been re-established,
     * we need to re-register the app
     */
    mul_register_app_cb(NULL,                 /* Application specific arg */
                        MTRACK_APP_NAME,       /* Application Name */
                        C_APP_ALL_SW,         /* Send any switch's notification */
                        C_APP_ALL_EVENTS,     /* Send all event notification per switch */
                        0,                    /* If any specific dpid filtering is requested */
                        NULL,                 /* List of specific dpids for filtering events */
                        &memtrack_app_cbs);      /* Event notifier call-backs */
}


/* Network event callbacks */
struct mul_app_client_cb memtrack_app_cbs = {
    .switch_priv_alloc = NULL,
    .switch_priv_free = NULL,
    .switch_add_cb =  memtrack_sw_add,         /* Switch add notifier */
    .switch_del_cb = memtrack_sw_del,          /* Switch delete notifier */
    .switch_priv_port_alloc = NULL,
    .switch_priv_port_free = NULL,
    .switch_port_add_cb = NULL,
    .switch_port_del_cb = NULL,
    .switch_port_link_chg = NULL,
    .switch_port_adm_chg = NULL,
    .switch_packet_in = memtrack_packet_in,    /* Packet-in notifier */ 
    .core_conn_closed = memtrack_core_closed,  /* Core connection drop notifier */
    .core_conn_reconn = memtrack_core_reconn   /* Core connection join notifier */
};  

/**
 * memtrack_timer_event -
 * Timer running at specified interval 
 * 
 * @fd : File descriptor used internally for scheduling event
 * @event : Event type
 * @arg : Any application specific arg
 */
static void
memtrack_timer_event(evutil_socket_t fd UNUSED,
                  short event UNUSED,
                  void *arg UNUSED)
{
    struct timeval tv = { 1 , 0 }; /* Timer set to run every one second */

    /* Any housekeeping activity */

    evtimer_add(memtrack_timer, &tv);
}  

/**
 * memtrack_module_init -
 * Hello application's main entry point
 * 
 * @base_arg: Pointer to the event base used to schedule IO events
 * @return : void
 */
void
memtrack_module_init(void *base_arg)
{
    struct event_base *base = base_arg;
    struct timeval tv = { 1, 0 };

    c_log_debug("%s", FN);

    /* Fire up a timer to do any housekeeping work for this application */
    memtrack_timer = evtimer_new(base, memtrack_timer_event, NULL); 
    evtimer_add(memtrack_timer, &tv);

    mul_register_app_cb(NULL,                 /* Application specific arg */
                        MTRACK_APP_NAME,       /* Application Name */ 
                        C_APP_ALL_SW,         /* Send any switch's notification */
                        C_APP_ALL_EVENTS,     /* Send all event notification per switch */
                        0,                    /* If any specific dpid filtering is requested */
                        NULL,                 /* List of specific dpids for filtering events */
                        &memtrack_app_cbs);      /* Event notifier call-backs */

    return;
}

/**
 * memtrack_module_vty_init -
 * Hello Application's vty entry point. If we want any private cli
 * commands. then we register them here
 *
 * @arg : Pointer to the event base(mostly left unused)
 */
void
memtrack_module_vty_init(void *arg UNUSED)
{
    c_log_debug("%s:", FN);
}

module_init(memtrack_module_init);
module_vty_init(memtrack_module_vty_init);
