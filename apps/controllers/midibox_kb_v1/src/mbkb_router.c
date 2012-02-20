// $Id$
/*
 * MIDI Router functions for MBKB
 *
 * ==========================================================================
 *
 *  Copyright (C) 2011 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

/////////////////////////////////////////////////////////////////////////////
// Include files
/////////////////////////////////////////////////////////////////////////////

#include <mios32.h>
#include <osc_client.h>
#include "app.h"
#include "tasks.h"

#include "mbkb_router.h"
#include "presets.h"


/////////////////////////////////////////////////////////////////////////////
// local defines
/////////////////////////////////////////////////////////////////////////////

// SysEx buffer for each input
#define NUM_SYSEX_BUFFERS     12
#define SYSEX_BUFFER_IN_USB0  0
#define SYSEX_BUFFER_IN_USB1  1
#define SYSEX_BUFFER_IN_USB2  2
#define SYSEX_BUFFER_IN_USB3  3
#define SYSEX_BUFFER_IN_UART0 4
#define SYSEX_BUFFER_IN_UART1 5
#define SYSEX_BUFFER_IN_UART2 6
#define SYSEX_BUFFER_IN_UART3 7
#define SYSEX_BUFFER_IN_OSC0  8
#define SYSEX_BUFFER_IN_OSC1  9
#define SYSEX_BUFFER_IN_OSC2  10
#define SYSEX_BUFFER_IN_OSC3  11

#define SYSEX_BUFFER_SIZE 1024

/////////////////////////////////////////////////////////////////////////////
// global variables
/////////////////////////////////////////////////////////////////////////////
mbkb_router_node_entry_t mbkb_router_node[MBKB_ROUTER_NUM_NODES] = {
  // src chn   dst chn
  { 0x10,  0, 0x20, 17 },
  { 0x10,  0, 0x20, 17 },
  { 0x10,  0, 0x20, 17 },
  { 0x10,  0, 0x20, 17 },
  { 0x10,  0, 0x20, 17 },
  { 0x10,  0, 0x20, 17 },
  { 0x10,  0, 0x20, 17 },
  { 0x10,  0, 0x20, 17 },
  { 0x10,  0, 0x20, 17 },
  { 0x10,  0, 0x20, 17 },
  { 0x10,  0, 0x20, 17 },
  { 0x10,  0, 0x20, 17 },
  { 0x10,  0, 0x20, 17 },
  { 0x10,  0, 0x20, 17 },
  { 0x10,  0, 0x20, 17 },
  { 0x10,  0, 0x20, 17 },
};

u32 mbkb_router_mclk_in;
u32 mbkb_router_mclk_out;

/////////////////////////////////////////////////////////////////////////////
// local variables
/////////////////////////////////////////////////////////////////////////////

static u8 sysex_buffer[NUM_SYSEX_BUFFERS][SYSEX_BUFFER_SIZE];
static u32 sysex_buffer_len[NUM_SYSEX_BUFFERS];


/////////////////////////////////////////////////////////////////////////////
// This function initializes the MIDI router
/////////////////////////////////////////////////////////////////////////////
s32 MBKB_ROUTER_Init(u32 mode)
{
  if( mode != 0 )
    return -1; // only mode 0 supported

  //                     USB0 only     UART0..3       IIC0..3      OSC0..3
  mbkb_router_mclk_in = (0x01 << 0) | (0x0f << 8) | (0x0f << 16) | (0x01 << 24);
  //                      all ports
  mbkb_router_mclk_out = 0xffffffff;

  // clear SysEx buffers
  int i;
  for(i=0; i<NUM_SYSEX_BUFFERS; ++i)
    sysex_buffer_len[i] = 0;

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Receives a MIDI package from APP_NotifyReceivedEvent (-> app.c)
/////////////////////////////////////////////////////////////////////////////
s32 MBKB_ROUTER_Receive(mios32_midi_port_t port, mios32_midi_package_t midi_package)
{
  // SysEx handled by APP_SYSEX_Parser()
  if( midi_package.cin == 0xf ||
      (midi_package.type >= 4 && midi_package.type <= 7) )
    return 0; // no error

  int node;
  mbkb_router_node_entry_t *n = (mbkb_router_node_entry_t *)&mbkb_router_node[0];
  for(node=0; node<MBKB_ROUTER_NUM_NODES; ++node, ++n) {
    if( n->src_chn && n->dst_chn && (n->src_port == port) ) {
      if( midi_package.event >= NoteOff && midi_package.event <= PitchBend ) {
	if( n->src_chn == 17 || midi_package.chn == (n->src_chn-1) ) {
	  mios32_midi_package_t fwd_package = midi_package;
	  if( n->dst_chn <= 16 )
	    fwd_package.chn = (n->dst_chn-1);
	  mios32_midi_port_t port = n->dst_port;
	  MUTEX_MIDIOUT_TAKE;
	  MIOS32_MIDI_SendPackage(port, fwd_package);
	  MUTEX_MIDIOUT_GIVE;
	}
      } else {
	if( n->dst_chn >= 17 ) { // SysEx, MIDI Clock, etc... only forwarded if destination channel set to "All"
	  mios32_midi_port_t port = n->dst_port;
	  MUTEX_MIDIOUT_TAKE;
	  MIOS32_MIDI_SendPackage(port, midi_package);
	  MUTEX_MIDIOUT_GIVE;
	}
      }
    }
  }

  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Receives a SysEx byte from APP_SYSEX_Parser (-> app.c)
/////////////////////////////////////////////////////////////////////////////
s32 MBKB_ROUTER_ReceiveSysEx(mios32_midi_port_t port, u8 midi_in)
{
  // determine SysEx buffer
  int sysex_in = 0;

  switch( port ) {
  case USB0: sysex_in = SYSEX_BUFFER_IN_USB0; break;
  case USB1: sysex_in = SYSEX_BUFFER_IN_USB1; break;
  case USB2: sysex_in = SYSEX_BUFFER_IN_USB2; break;
  case USB3: sysex_in = SYSEX_BUFFER_IN_USB3; break;
  case UART0: sysex_in = SYSEX_BUFFER_IN_UART0; break;
  case UART1: sysex_in = SYSEX_BUFFER_IN_UART1; break;
  case UART2: sysex_in = SYSEX_BUFFER_IN_UART2; break;
  case UART3: sysex_in = SYSEX_BUFFER_IN_UART3; break;
  case OSC0: sysex_in = SYSEX_BUFFER_IN_OSC0; break;
  case OSC1: sysex_in = SYSEX_BUFFER_IN_OSC1; break;
  case OSC2: sysex_in = SYSEX_BUFFER_IN_OSC2; break;
  case OSC3: sysex_in = SYSEX_BUFFER_IN_OSC3; break;
  default:
    return -1; // not assigned
  }

  // store value into buffer, send when:
  //   o 0xf7 (end of stream) has been received
  //   o 0xf0 (start of stream) has been received although buffer isn't empty
  //   o buffer size has been exceeded
  // we check for (SYSEX_BUFFER_SIZE-1), so that we always have a free byte for F7
  u32 buffer_len = sysex_buffer_len[sysex_in];
  if( midi_in == 0xf7 || (midi_in == 0xf0 && buffer_len != 0) || buffer_len >= (SYSEX_BUFFER_SIZE-1) ) {

    if( midi_in == 0xf7 && buffer_len < SYSEX_BUFFER_SIZE ) // note: we always have a free byte for F7
      sysex_buffer[sysex_in][sysex_buffer_len[sysex_in]++] = midi_in;

    int node;
    mbkb_router_node_entry_t *n = (mbkb_router_node_entry_t *)&mbkb_router_node[0];
    for(node=0; node<MBKB_ROUTER_NUM_NODES; ++node, ++n) {
      // SysEx, only forwarded if source and destination channel set to "All"
      if( n->src_chn >= 17 && n->dst_chn >= 17 && (n->src_port == port) ) {
	mios32_midi_port_t port = n->dst_port;
	MUTEX_MIDIOUT_TAKE;
	if( (port & 0xf0) == OSC0 )
	  OSC_CLIENT_SendSysEx(port & 0x0f, sysex_buffer[sysex_in], sysex_buffer_len[sysex_in]);
	else
	  MIOS32_MIDI_SendSysEx(port, sysex_buffer[sysex_in], sysex_buffer_len[sysex_in]);
	MUTEX_MIDIOUT_GIVE;
      }
    }

    // empty buffer
    sysex_buffer_len[sysex_in] = 0;

    // fill with next byte if buffer size hasn't been exceeded
    if( midi_in != 0xf7 )
      sysex_buffer[sysex_in][sysex_buffer_len[sysex_in]++] = midi_in;

  } else {
    // add to buffer
    sysex_buffer[sysex_in][sysex_buffer_len[sysex_in]++] = midi_in;
  }

  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Returns 1 if given port receives MIDI Clock
// Returns 0 if MIDI Clock In disabled
// Returns -1 if port not supported
// Returns -2 if MIDI In function disabled
/////////////////////////////////////////////////////////////////////////////
s32 MBKB_ROUTER_MIDIClockInGet(mios32_midi_port_t port)
{
  // coding: USB0..7, UART0..7, IIC0..7, OSC0..7
  if( !(port & 0x08) && port >= 0x10 && port < 0x50 ) {
    // extra: MIDI IN Clock function not supported for IIC0..7 (yet)
    if( port >= IIC0 && port <= (IIC0+15) )
      return -2; // MIDI In function disabled

    int port_flag = (((port&0xf0)-0x10) >> 1) | (port & 0x7);    
    return (mbkb_router_mclk_in & (1 << port_flag)) ? 1 : 0;
  }

  return -1; // port not supported
}

/////////////////////////////////////////////////////////////////////////////
// Enables/Disables MIDI In Clock function for given port
/////////////////////////////////////////////////////////////////////////////
s32 MBKB_ROUTER_MIDIClockInSet(mios32_midi_port_t port, u8 enable)
{
  // coding: USB0..7, UART0..7, IIC0..7, OSC0..7
  if( !(port & 0x08) && port >= 0x10 && port < 0x50 ) {
    int port_flag = (((port&0xf0)-0x10) >> 1) | (port & 0x7);    
    if( enable )
      mbkb_router_mclk_in |= (1 << port_flag);
    else
      mbkb_router_mclk_in &= ~(1 << port_flag);

    return 0; // no error
  }

  return -1; // port not supported
}


/////////////////////////////////////////////////////////////////////////////
// Returns 1 if given port sends MIDI Clock
// Returns 0 if MIDI Clock Out disabled
// Returns -1 if port not supported
// Returns -2 if MIDI In function disabled
/////////////////////////////////////////////////////////////////////////////
s32 MBKB_ROUTER_MIDIClockOutGet(mios32_midi_port_t port)
{
  // coding: USB0..7, UART0..7, IIC0..7, OSC0..7
  if( !(port & 0x08) && port >= 0x10 && port < 0x50 ) {
    int port_flag = (((port&0xf0)-0x10) >> 1) | (port & 0x7);    
    return (mbkb_router_mclk_out & (1 << port_flag)) ? 1 : 0;
  }

  return -1; // port not supported
}

/////////////////////////////////////////////////////////////////////////////
// Enables/Disables MIDI Out Clock function for given port
/////////////////////////////////////////////////////////////////////////////
s32 MBKB_ROUTER_MIDIClockOutSet(mios32_midi_port_t port, u8 enable)
{
  // coding: USB0..7, UART0..7, IIC0..7, OSC0..7
  if( !(port & 0x08) && port >= 0x10 && port < 0x50 ) {
    int port_flag = (((port&0xf0)-0x10) >> 1) | (port & 0x7);    
    if( enable )
      mbkb_router_mclk_out |= (1 << port_flag);
    else
      mbkb_router_mclk_out &= ~(1 << port_flag);

    return 0; // no error
  }

  return -1; // port not supported
}


/////////////////////////////////////////////////////////////////////////////
// This function sends a MIDI clock/Start/Stop/Continue event to all output
// ports which have been enabled for this function.
// if bpm_tick == 0, the event will be sent immediately, otherwise it will
// be queued
/////////////////////////////////////////////////////////////////////////////
s32 MBKB_ROUTER_SendMIDIClockEvent(u8 evnt0, u32 bpm_tick)
{
  int i;

  mios32_midi_package_t p;
  p.ALL = 0;
  p.type = 0x5; // Single-byte system common message
  p.evnt0 = evnt0;

  u32 port_mask = 0x00000001;
  for(i=0; i<32; ++i, port_mask<<=1) {
    if( mbkb_router_mclk_out & port_mask & 0xffffff0f ) { // filter USB5..USB8 to avoid unwanted clock events to non-existent ports
      // coding: USB0..7, UART0..7, IIC0..7, OSC0..7
      mios32_midi_port_t port = (USB0 + ((i&0x18) << 1)) | (i&7);

      // TODO: special check for OSC, since MIOS32_MIDI_CheckAvailable() won't work here
      if( MIOS32_MIDI_CheckAvailable(port) ) {
	if( bpm_tick ) {
	  //SEQ_MIDI_OUT_Send(port, p, SEQ_MIDI_OUT_ClkEvent, bpm_tick, 0);
	} else {
	  MUTEX_MIDIOUT_TAKE;
	  MIOS32_MIDI_SendPackage(port, p);
	  MUTEX_MIDIOUT_GIVE;
	}
      }
    }
  }

  return 0; // no error;
}
