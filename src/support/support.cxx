// ----------------------------------------------------------------------------
// Copyright (C) 2014
//              David Freese, W1HKJ
//
// This file is part of flrig.
//
// flrig is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// flrig is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
// ----------------------------------------------------------------------------

#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

#include "icons.h"
#include "support.h"
#include "debug.h"
#include "gettext.h"
#include "rig_io.h"
#include "dialogs.h"
#include "rigbase.h"
#include "ptt.h"
#include "socket_io.h"
#include "ui.h"

#include "rig.h"
#include "rigs.h"
#include "K3_ui.h"
#include "KX3_ui.h"

#include "rigpanel.h"

using namespace std;

rigbase *selrig = rigs[0];

extern bool test;

bool flrig_abort = false;

int freqval = 0;

FREQMODE vfoA         = {14070000, 0, 0, UI};
FREQMODE vfoB         = {7070000, 0, 0, UI};
FREQMODE vfo          = {0, 0, 0, UI};
FREQMODE transceiverA = {14070000, 0, 0, UI};
FREQMODE transceiverB = {7070000, 0, 0, UI};
FREQMODE xmlvfo       = {14070000, 0, 0, UI};

enum {VOL, MIC, PWR, SQL, IFSH, NOTCH, RFGAIN, NR };

struct SLIDER {
	int  which;
	float value;
	int  button;
	SLIDER(int a, float f, int b = 0) {which = a; value = f; button = b;}
};

queue<SLIDER> sliders;

queue<FREQMODE> queA;
queue<FREQMODE> queB;
queue<bool> quePTT;

bool useB = false;
bool changed_vfo = false;
bool pushedB = false;

bool volume_changed = false;
bool mic_changed = false;
bool power_changed = false;
bool squelch_changed = false;
bool if_shift_changed = false;
bool notch_changed = false;
bool rfgain_changed = false;
bool noise_reduction_changed = false;

const char **old_bws = NULL;

// Add alpha-tag to FREQMODE;
struct ATAG_FREQMODE {
	long freq;
	int  imode;
	int  iBW;
	int  src;
	char alpha_tag[ATAGSIZE];
};
ATAG_FREQMODE oplist[LISTSIZE];

int  numinlist = 0;
vector<string> rigmodes_;
vector<string> rigbws_;

Cserial *RigSerial;
Cserial *AuxSerial;
Cserial *SepSerial;

bool using_buttons = false;

enum { SWR_IMAGE, ALC_IMAGE };
int meter_image = SWR_IMAGE;

//=============================================================================
// loop for serial i/o thread
// runs continuously until program is closed
// only accesses the serial port if it has been successfully opened
//=============================================================================

bool bypass_serial_thread_loop = true;
bool run_serial_thread = true;

bool PTT = false;
int  powerlevel = 0;

void trace(string s)
{
//	std::cout << s << std::endl;
}

char *print(FREQMODE data)
{
	static char str[256];
	const char **bwt = selrig->bwtable(data.imode);
	const char **dsplo = selrig->lotable(data.imode);
	const char **dsphi = selrig->hitable(data.imode);
	snprintf(
		str, sizeof(str),
		"\
FREQMODE: %3s\n\
  freq: %10ld\n\
  mode: %4s\n\
  bwt index:  %02d\n\
  bwt values: [%s] [%s]",
		data.src == XML ? "XML" : data.src == UI ? "UI" :
			data.src == SRVR ? "SRVR" : "RIG",
		data.freq,
		selrig->modes_ ? selrig->modes_[data.imode] : "modes n/a",
		data.iBW,
		(data.iBW > 256 && selrig->has_dsp_controls) ?
			(dsplo ? dsplo[data.iBW & 0x7F] : "??") : (bwt ? bwt[data.iBW] : "n/a"),
		(data.iBW > 256 && selrig->has_dsp_controls) ?
			(dsphi ? dsphi[(data.iBW >> 8) & 0x7F] : "??") : ""
		);
	return str;
}

// the following functions are ONLY CALLED by the serial loop
// read any data stream sent by transceiver
// support for the K3 and KX3 read of VFO, MODE and BW are
// in the K3_ui and KX3_ui source files

void read_info()
{
	guard_lock serial_lock(&mutex_serial, 1);
	selrig->get_info();
}

// read current vfo frequency

void read_vfo()
{
// transceiver changed ?
	guard_lock serial_lock(&mutex_serial, 2);
	long  freq;
	if (!useB) { // vfo-A
		freq = selrig->get_vfoA();
		if (freq != vfoA.freq) {
			vfoA.freq = freq;
			Fl::awake(setFreqDispA, (void *)vfoA.freq);
			vfo = vfoA;
		}
		if ( selrig->twovfos() ) {
			freq = selrig->get_vfoB();
			if (freq != vfoB.freq) {
				vfoB.freq = freq;
				Fl::awake(setFreqDispB, (void *)vfoB.freq);
			}
		}
	} else { // vfo-B
		freq = selrig->get_vfoB();
		if (freq != vfoB.freq) {
			vfoB.freq = freq;
			Fl::awake(setFreqDispB, (void *)vfoB.freq);
			vfo = vfoB;
		}
		if ( selrig->twovfos() ) {
			freq = selrig->get_vfoA();
			if (freq != vfoA.freq) {
				vfoA.freq = freq;
				Fl::awake(setFreqDispA, (void *)vfoA.freq);
			}
		}
	}
}

void update_vfoAorB(void *d)
{
	long val = (long)d;

	if (xcvr_name == rig_FT817.name_) {
//trace("update_vfoAorB");
		guard_lock serial_lock(&mutex_serial);
		if (val) {
			useB = true;
			vfoB.src = RIG;
			vfoB.freq = selrig->get_vfoB();
			vfoB.imode = selrig->get_modeB();
			vfoB.iBW = selrig->get_bwB();
			changed_vfo = true;
			guard_lock queB_lock(&mutex_queB, 38);
			while (!queB.empty()) queB.pop();
			queB.push(vfoB);
			highlight_vfo(NULL);
		} else {
			useB = false;
			vfoA.src = RIG;
			vfoA.freq = selrig->get_vfoA();
			vfoA.imode = selrig->get_modeA();
			vfoA.iBW = selrig->get_bwA();
			changed_vfo = true;
			guard_lock queA_lock(&mutex_queA, 38);
			while (!queA.empty()) queA.pop();
			queA.push(vfoA);
			highlight_vfo(NULL);
		}
		return;
	}

	if (val) {
//	could use cb_selectB() here, but that switches off split mode
		guard_lock serial_lock(&mutex_serial, 95);
		changed_vfo = true;
		vfoB.src = UI;
		vfoB.freq = FreqDispB->value();
		guard_lock queB_lock(&mutex_queB, 96);
		queB.push(vfoB);
		useB = true;
		highlight_vfo((void *)0);
//		cb_selectB();
	} else {
//	could use cb_selectA() here, but that switches off split mode
		guard_lock serial_lock(&mutex_serial, 97);
		changed_vfo = true;
		vfoA.src = UI;
		vfoA.freq = FreqDispA->value();
		guard_lock queA_lock(&mutex_queA, 98);
		queA.push(vfoA);
		useB = false;
		highlight_vfo((void *)0);
//		cb_selectA();
	}
}

void read_vfoAorB()
{
	int val;
	if (selrig->has_getvfoAorB) {
		{
			guard_lock serial_lock(&mutex_serial, 99);
			val = selrig->get_vfoAorB();
			int retry = 10;
			while (val == -1 && retry--) {
				MilliSleep(50);
				val = selrig->get_vfoAorB();
			}
			if (val == -1) val = 0;
		}
		if (val != useB) {
			Fl::awake(update_vfoAorB, reinterpret_cast<void*>(val));
		}
	}
}

void setModeControl(void *)
{
	opMODE->index(vfo.imode);

// enables/disables the IF shift control, depending on the mode.
// the IF Shift function, is ONLY valid in CW modes, with the 870S.

	if (xcvr_name == rig_TS870S.name_) {
		if (vfo.imode == RIG_TS870S::tsCW || vfo.imode == RIG_TS870S::tsCWR) {
			btnIFsh->activate();
			if (sldrIFSHIFT) sldrIFSHIFT->activate();
			if (spnrIFSHIFT) spnrIFSHIFT->activate();
		} else {
			btnIFsh->deactivate();
			if (sldrIFSHIFT) sldrIFSHIFT->deactivate();
			if (spnrIFSHIFT) spnrIFSHIFT->deactivate();
		}
	}
}

// mode and bandwidth
void read_mode()
{
	guard_lock serial_lock(&mutex_serial, 3);
	int nu_mode;
	int nu_BW;
	if (!useB) {
		nu_mode = selrig->get_modeA();
		if (nu_mode != vfoA.imode) {
			{
				vfoA.imode = vfo.imode = nu_mode;
				selrig->adjust_bandwidth(vfo.imode);
				nu_BW = selrig->get_bwA();
				vfoA.iBW = vfo.iBW = nu_BW;
			}
//			Fl::awake(setModeControl);
			set_bandwidth_control();
			Fl::awake(setModeControl);
		}
	} else {
		nu_mode = selrig->get_modeB();
		if (nu_mode != vfoB.imode) {
			{
				vfoB.imode = vfo.imode = nu_mode;
				selrig->adjust_bandwidth(vfo.imode);
				nu_BW = selrig->get_bwB();
				vfoB.iBW = vfo.iBW = nu_BW;
			}
//			Fl::awake(setModeControl);
			set_bandwidth_control();
			Fl::awake(setModeControl);
		}
	}
}

void setBWControl(void *)
{
	if (selrig->has_dsp_controls) {
		if (vfo.iBW > 256) {
			opBW->index(0);
			opBW->hide();
			opDSP_hi->index((vfo.iBW >> 8) & 0x7F);
			opDSP_hi->hide();
			opDSP_lo->index(vfo.iBW & 0xFF);
			opDSP_lo->show();
			btnDSP->label(selrig->SL_label);
			btnDSP->redraw_label();
			btnDSP->show();
		} else {
			opDSP_lo->index(0);
			opDSP_hi->index(0);
			opDSP_lo->hide();
			opDSP_hi->hide();
			btnDSP->hide();
			opBW->index(vfo.iBW);
			opBW->show();
		}
	} else {
		opDSP_lo->hide();
		opDSP_hi->hide();
		btnDSP->hide();
		opBW->index(vfo.iBW);
		opBW->show();
	}
}

void read_bandwidth()
{
	guard_lock serial_lock(&mutex_serial, 5);
	int nu_BW;
	if (!useB) {
		nu_BW = selrig->get_bwA();
		if (nu_BW != vfoA.iBW) {
			vfoA.iBW = vfo.iBW = nu_BW;
			Fl::awake(setBWControl);
		}
	} else {
		nu_BW = selrig->get_bwB();
		if (nu_BW != vfoB.iBW) {
			vfoB.iBW = vfo.iBW = nu_BW;
			Fl::awake(setBWControl);
		}
	}
}

// read current signal level
int mval = 0;
void read_smeter()
{
	if (!selrig->has_smeter) return;
	int  sig;
	{
		guard_lock serial_lock(&mutex_serial, 7);
		sig = selrig->get_smeter();
	}
	if (sig == -1) return;
	mval = sig;
	Fl::awake(updateSmeter, reinterpret_cast<void*>(sig));
}

// read power out
void read_power_out()
{
	if (!selrig->has_power_out) return;
	int sig;
	{
		guard_lock serial_lock(&mutex_serial, 8);
		sig = selrig->get_power_out();
	}
	if (sig == -1) return;
	mval = sig;
	Fl::awake(updateFwdPwr, reinterpret_cast<void*>(sig));
}

// read swr
void read_swr()
{
	if ((meter_image != SWR_IMAGE) ||
		!selrig->has_swr_control) return;
	int sig;
	{
		guard_lock serial_lock(&mutex_serial, 9);
		sig = selrig->get_swr();
	}
	if (sig > -1)
		Fl::awake(updateSWR, reinterpret_cast<void*>(sig));
}

// alc
void read_alc()
{
	if ((meter_image != ALC_IMAGE) ||
		!selrig->has_alc_control) return;
	int sig;
	{
		guard_lock serial_lock(&mutex_serial, 10);
		sig = selrig->get_alc();
	}
	if (sig > -1)
		Fl::awake(updateALC, reinterpret_cast<void*>(sig));
}

// notch
void update_auto_notch(void *d)
{
	btnAutoNotch->value(progStatus.auto_notch);
}

void read_auto_notch()
{
	if (!selrig->has_auto_notch || notch_changed) return;
	{
		guard_lock serial_lock(&mutex_serial, 11);
		progStatus.auto_notch = selrig->get_auto_notch();
	}
	Fl::awake(update_auto_notch, (void*)0);
}

// NOISE blanker
void update_noise(void *d)
{
	btnNOISE->value(progStatus.noise);
}

void read_noise()
{
	{
		guard_lock serial_lock(&mutex_serial, 12);
		progStatus.noise = selrig->get_noise();
	}
	Fl::awake(update_noise, (void*)0);
}

// preamp - attenuator
void update_preamp(void *d)
{
	btnPreamp->value(progStatus.preamp);
}

void update_attenuator(void *d)
{
	btnAttenuator->value(progStatus.attenuator);
}

void read_preamp_att()
{
	if (selrig->has_preamp_control) {
		{
			guard_lock serial_lock(&mutex_serial, 13);
			progStatus.preamp = selrig->get_preamp();
		}
		Fl::awake(update_preamp, (void*)0);
	}
	if (selrig->has_attenuator_control) {
		{
			guard_lock serial_lock(&mutex_serial, 14);
			progStatus.attenuator = selrig->get_attenuator();
		}
		Fl::awake(update_attenuator, (void*)0);
	}
}

// split
void update_split(void *d)
{
	if (xcvr_name == rig_FT950.name_ || xcvr_name == rig_FTdx1200.name_ ||
		xcvr_name == rig_TS480SAT.name_ || xcvr_name == rig_TS480HX.name_ ||
		xcvr_name == rig_TS590S.name_ || xcvr_name == rig_TS590SG.name_ ||
		xcvr_name == rig_TS2000.name_ || xcvr_name == rig_TS990.name_) {
		switch (progStatus.split) {
			case 0: btnSplit->value(0);
					useB = false;
					highlight_vfo(NULL);
					break;
			case 1: btnSplit->value(1);
					useB = true;
					highlight_vfo(NULL);
					break;
			case 2:	btnSplit->value(1);
					useB = false;
					highlight_vfo(NULL);
					break;
			case 3: btnSplit->value(0);
					useB = true;
					highlight_vfo(NULL);
					break;
		}
	} else
		btnSplit->value(progStatus.split);
}

// read split during xmt on FT817
// read split during rcv on all others
void read_split()
{
	if ((xcvr_name != rig_FT817.name_) && PTT) return;
	int val = progStatus.split;
	if (selrig->can_split()) {
		{
			guard_lock serial_lock(&mutex_serial, 15);
			val = selrig->get_split();
		}
		if (val != progStatus.split) {
			progStatus.split = val;
			Fl::awake(update_split, (void*)0);
		}
	}
}

// volume
void update_volume(void *d)
{
	long *nr = (long *)d;
	if (spnrVOLUME) spnrVOLUME->value(progStatus.volume);
	if (spnrVOLUME) spnrVOLUME->activate();
	sldrVOLUME->value(progStatus.volume); // Set slider to last known value
	sldrVOLUME->activate();				  // activate it

	if (*nr) btnVol->value(1);			  // Button Lit
	else     btnVol->value(0);			  // Button Dark.
}

long nlzero = 0L;
long nlone = 1L;

void read_volume()
{
	if (!selrig->has_volume_control || volume_changed) return;
	int vol;
	{
		guard_lock serial_lock(&mutex_serial, 16);
		vol = selrig->get_volume_control();
	}
	if (vol != progStatus.volume) {
		if (vol <= 1 && !btnVol->value()) return;
		progStatus.volume = vol;
		if (vol <= 1 && btnVol->value())
			Fl::awake(update_volume, (void*)&nlzero);
		else
			Fl::awake(update_volume, (void*)&nlone);
	}
}

// ifshift
void update_ifshift(void *d)
{
	btnIFsh->value(progStatus.shift);
//	if (progStatus.shift)
	if (sldrIFSHIFT) sldrIFSHIFT->value(progStatus.shift_val);
	if (spnrIFSHIFT) spnrIFSHIFT->value(progStatus.shift_val);
}

void read_ifshift()
{
	if (!selrig->has_ifshift_control || if_shift_changed) return;
	{
		guard_lock serial_lock(&mutex_serial, 17);
		progStatus.shift = selrig->get_if_shift(progStatus.shift_val);
	}
	Fl::awake(update_ifshift, (void*)0);
}

// noise reduction
void update_nr(void *d)
{
	btnNR->value(progStatus.noise_reduction);
	if (sldrNR) sldrNR->value(progStatus.noise_reduction_val);
	if (spnrNR) spnrNR->value(progStatus.noise_reduction_val);
}

void read_nr()
{
	if (!selrig->has_noise_reduction || noise_reduction_changed) return;
	{
		guard_lock serial_lock(&mutex_serial, 18);
		progStatus.noise_reduction = selrig->get_noise_reduction();
		progStatus.noise_reduction_val = selrig->get_noise_reduction_val();
	}
	Fl::awake(update_nr, (void*)0);
}

// manual notch
bool rig_notch;
int  rig_notch_val;
void update_notch(void *d)
{
	btnNotch->value(progStatus.notch = rig_notch);
	if (sldrNOTCH) sldrNOTCH->value(progStatus.notch_val = rig_notch_val);
	if (spnrNOTCH) spnrNOTCH->value(progStatus.notch_val = rig_notch_val);
}

void read_notch()
{
	if (!selrig->has_notch_control || notch_changed) return;
	{
		guard_lock serial_lock(&mutex_serial, 19);
		rig_notch = selrig->get_notch(rig_notch_val);
	}
	if (rig_notch != progStatus.notch || rig_notch_val != progStatus.notch_val)
		Fl::awake(update_notch, (void*)0);
}

// power_control
void update_power_control(void *d)
{
	set_power_controlImage(progStatus.power_level);
	if (sldrPOWER) sldrPOWER->value(progStatus.power_level);
	if (spnrPOWER) spnrPOWER->value(progStatus.power_level);
	if (xcvr_name == rig_K2.name_) {
		double min, max, step;
		selrig->get_pc_min_max_step(min, max, step);
		if (sldrPOWER) sldrPOWER->minimum(min);
		if (sldrPOWER) sldrPOWER->maximum(max);
		if (sldrPOWER) sldrPOWER->step(step);
		if (sldrPOWER) sldrPOWER->redraw();
		if (spnrPOWER) spnrPOWER->minimum(min);
		if (spnrPOWER) spnrPOWER->maximum(max);
		if (spnrPOWER) spnrPOWER->step(step);
		if (spnrPOWER) spnrPOWER->redraw();
	}
}

void read_power_control()
{
	if (!selrig->has_power_control || power_changed) return;
	{
		guard_lock serial_lock(&mutex_serial, 20);
		progStatus.power_level = selrig->get_power_control();
	}
	Fl::awake(update_power_control, (void*)0);
}

// mic gain
void update_mic_gain(void *d)
{
	if (sldrMICGAIN) sldrMICGAIN->value(progStatus.mic_gain);
	if (spnrMICGAIN) spnrMICGAIN->value(progStatus.mic_gain);
}

void read_mic_gain()
{
	if (!selrig->has_micgain_control || mic_changed) return;
	{
		guard_lock serial_lock(&mutex_serial, 21);
		progStatus.mic_gain = selrig->get_mic_gain();
	}
	Fl::awake(update_mic_gain, (void*)0);
}

// rf gain
void update_rfgain(void *d)
{
	if (sldrRFGAIN) sldrRFGAIN->value(progStatus.rfgain);
	if (spnrRFGAIN) spnrRFGAIN->value(progStatus.rfgain);
}

void read_agc()
{
	if (!selrig->has_agc_control) return;
	progStatus.agc_level = selrig->get_agc();
	Fl::awake(setAGC);
}

void read_rfgain()
{
//	if (!selrig->has_rf_control || rfgain_changed) return;
	if (selrig->has_rf_control) {
		guard_lock serial_lock(&mutex_serial, 22);
		progStatus.rfgain = selrig->get_rf_gain();
		Fl::awake(update_rfgain, (void*)0);
		read_agc();
	}
}

// squelch
void update_squelch(void *d)
{
	if (sldrSQUELCH) sldrSQUELCH->value(progStatus.squelch);
	if (spnrSQUELCH) spnrSQUELCH->value(progStatus.squelch);
}

void read_squelch()
{
	if (!selrig->has_sql_control || squelch_changed) return;
	{
		guard_lock serial_lock(&mutex_serial, 23);
		progStatus.squelch = selrig->get_squelch();
	}
	Fl::awake(update_squelch, (void*)0);
}

struct POLL_PAIR {
	int *poll;
	void (*pollfunc)();
};

POLL_PAIR RX_poll_pairs[] = {
	{&progStatus.poll_vfoAorB, read_vfoAorB},
	{&progStatus.poll_frequency, read_vfo},
	{&progStatus.poll_mode, read_mode},
	{&progStatus.poll_bandwidth, read_bandwidth},
	{&progStatus.poll_smeter, read_smeter},
	{&progStatus.poll_volume, read_volume},
	{&progStatus.poll_auto_notch, read_auto_notch},
	{&progStatus.poll_notch, read_notch},
	{&progStatus.poll_ifshift, read_ifshift},
	{&progStatus.poll_power_control, read_power_control},
	{&progStatus.poll_pre_att, read_preamp_att},
	{&progStatus.poll_micgain, read_mic_gain},
	{&progStatus.poll_squelch, read_squelch},
	{&progStatus.poll_rfgain, read_rfgain},
	{&progStatus.poll_split, read_split},
	{&progStatus.poll_nr, read_nr},
	{&progStatus.poll_noise, read_noise},
	{NULL, NULL}
};

POLL_PAIR TX_poll_pairs[] = {
	{&progStatus.poll_pout, read_power_out},
	{&progStatus.poll_swr, read_swr},
	{&progStatus.poll_alc, read_alc},
	{&progStatus.poll_split, read_split}, // read during XMT on FT817
	{NULL, NULL}
};

POLL_PAIR *poll_parameters;

static bool resetrcv = true;
static bool resetxmt = true;

void serviceA()
{
	if (queA.empty()) return;
trace("serviceA");
	guard_lock serial_lock(&mutex_serial, 24);
	{
		guard_lock queA_lock(&mutex_queA, 25);
		while (!queA.empty()) {
			vfoA = queA.front();
			queA.pop();
		}
	}

	if (!selrig->can_change_alt_vfo && useB) return;

	if (changed_vfo && !useB) {
		selrig->selectA();
	}

// if TT550 etal and on the B vfo
	if (selrig->can_change_alt_vfo && useB) {
		selrig->set_vfoA(vfoA.freq);
		goto end_serviceA;
	}

//trace(std::string("VFO is:\n").append(print(vfo)));
//trace(std::string("VFO ->:\n").append(print(vfoA)));

	if (vfoA.imode != vfo.imode || changed_vfo) {
		selrig->set_modeA(vfoA.imode);
		vfo.imode = vfoA.imode;
		Fl::awake(setModeControl);
		int saveBW = vfoA.iBW;
		set_bandwidth_control();
		if (saveBW != -1) {
			vfo.iBW = vfoA.iBW = saveBW;
			selrig->set_bwA(saveBW);
			Fl::awake(setBWControl);
		}
	} else if (vfoA.iBW != vfo.iBW) {
		selrig->set_bwA(vfoA.iBW);
		vfo.iBW = vfoA.iBW;
		Fl::awake(setBWControl);
	}

	if (vfoA.freq != vfo.freq || changed_vfo) {
		selrig->set_vfoA(vfoA.freq);
		vfo.freq = vfoA.freq;
		Fl::awake(setFreqDispA, (void *)vfoA.freq);
	}

end_serviceA:

	if (RIG_DEBUG) LOG_INFO("%s", print(vfoA));

	changed_vfo = false;
}

void serviceB()
{
	if (queB.empty())
		return;
	guard_lock serial_lock(&mutex_serial, 26);
	{
		guard_lock queB_lock(&mutex_queB, 27);
		while (!queB.empty()) {
			vfoB = queB.front();
			queB.pop();
		}
	}

	if (!selrig->can_change_alt_vfo && !useB) return;

	if (changed_vfo && useB) {
		selrig->selectB();
	}
// if TT550 or K3 and split or on vfoA just update the B vfo
	if ((xcvr_name == rig_K3.name_) || (selrig->can_change_alt_vfo && !useB)) {
		selrig->set_vfoB(vfoB.freq);
		goto end_serviceB;
	}

//trace(std::string("VFO is:\n").append(print(vfo)));
//trace(std::string("VFO ->:\n").append(print(vfoB)));

	if (vfoB.imode != vfo.imode || pushedB || changed_vfo) {
		selrig->set_modeB(vfoB.imode);
		vfo.imode = vfoB.imode;
		Fl::awake(setModeControl);
		int saveBW = vfoB.iBW;
		set_bandwidth_control();
		if (saveBW != -1) {
			vfo.iBW = vfoB.iBW = saveBW;
			selrig->set_bwB(saveBW);
			Fl::awake(setBWControl);
		}
	} else if (vfoB.iBW != vfo.iBW || pushedB) {
		selrig->set_bwB(vfoB.iBW);
		vfo.iBW = vfoB.iBW;
		Fl::awake(setBWControl);
	}

	if (vfoB.freq != vfo.freq || pushedB || changed_vfo) {
		selrig->set_vfoB(vfoB.freq);
		vfo.freq = vfoB.freq;
		Fl::awake(setFreqDispB, (void *)vfoB.freq);
	}

	pushedB = false;

end_serviceB:

	if (RIG_DEBUG) LOG_INFO("%s", print(vfoB));

	changed_vfo = false;
}

void servicePTT()
{
	if (quePTT.empty()) return;

	guard_lock ptt_lock(&mutex_ptt, 28);
	while (!quePTT.empty()) {
		PTT = quePTT.front();
		quePTT.pop();
		rigPTT(PTT);
		Fl::awake(update_UI_PTT);
	}
}

void serviceSliders()
{
	if (sliders.empty()) return;

	SLIDER working(0,0);
	guard_lock serial_lock(&mutex_serial, 29);
	while(!sliders.empty()) {
		working = sliders.front();
		sliders.pop();
		switch (working.which) {
			case VOL:
				selrig->set_volume_control(working.value);
				progStatus.volume = working.value;
				volume_changed = false;
				break;
			case MIC:
				progStatus.mic_gain = working.value;
				selrig->set_mic_gain(working.value);
				mic_changed = false;
				break;
			case PWR:
				selrig->set_power_control(working.value);
				power_changed = false;
				break;
			case SQL:
				progStatus.squelch = working.value;
				selrig->set_squelch(working.value);
				squelch_changed = false;
				break;
			case IFSH:
				progStatus.shift = working.button;
				progStatus.shift_val = working.value;
				if (xcvr_name == rig_TS990.name_) {
					if (progStatus.shift)
						selrig->set_monitor(1);
					else
						selrig->set_monitor(0);
					selrig->set_if_shift(working.value);
				} else {
					selrig->set_if_shift(working.value);
				}
				if_shift_changed = false;
				break;
			case NOTCH:
				progStatus.notch = working.button;
				progStatus.notch_val = working.value;
				selrig->set_notch(working.button, working.value);
				notch_changed = false;
				break;
			case RFGAIN:
				progStatus.rfgain = working.value;
				selrig->set_rf_gain(working.value);
				rfgain_changed = false;
				break;
			case NR:
				if (xcvr_name == rig_TS2000.name_) {
					if (working.button != -1) { // pia
						if (selrig->noise_reduction_level() == 0) {
							selrig->set_noise_reduction(1);
							selrig->set_noise_reduction_val(selrig->nrval1());
							progStatus.noise_reduction = 1;
							progStatus.noise_reduction_val = selrig->nrval1();
							Fl::awake(update_nr, (void*)0);
						} else if (selrig->currmode() != RIG_TS2000::FM &&
								  selrig->noise_reduction_level() == 1) {
							selrig->set_noise_reduction(2);
							selrig->set_noise_reduction_val(selrig->nrval2());
							progStatus.noise_reduction = 2;
							progStatus.noise_reduction_val = selrig->nrval2();
							Fl::awake(update_nr, (void*)0);
						} else
							selrig->set_noise_reduction(0);
					} else {
						progStatus.noise_reduction_val = working.value;
						selrig->set_noise_reduction_val(working.value);
						Fl::awake(update_nr, (void*)0);
					}
				} else { // not TS2000
					if (working.button != -1) {
						progStatus.noise_reduction = working.button;
						selrig->set_noise_reduction(working.button);
					}
					progStatus.noise_reduction_val = working.value;
					selrig->set_noise_reduction_val(working.value);
				}
				noise_reduction_changed = false;
				break;
			default :
				break;
		}
	}
}

inline bool que_pending()
{
	if (!quePTT.empty()) return true;
	if (!queA.empty()) return true;
	if (!queB.empty()) return true;
	return false;
}

void * serial_thread_loop(void *d)
{
  static int  loopcount = progStatus.serloop_timing / 10;
  static int  poll_nbr = 0;

	for(;;) {
		if (!run_serial_thread) break;

		MilliSleep(10);

		if (bypass_serial_thread_loop) {
			goto serial_bypass_loop;
		}

		serviceSliders();
		servicePTT();

//send any freq/mode/bw changes in the queu

		if (!PTT) {
			serviceA();
			if (!quePTT.empty())
				continue;
			serviceB();
			if (que_pending())
				continue;

			if (resetrcv) {
				Fl::awake(zeroXmtMeters, 0);
				resetrcv = false;
				loopcount = progStatus.serloop_timing / 10;
				poll_nbr = 0;
			}
			resetxmt = true;

			if (--loopcount <= 0) {
				loopcount = progStatus.serloop_timing / 10;
				poll_nbr++;

				if (xcvr_name == rig_K3.name_) {
					if (que_pending()) continue;
					read_K3();
				}
				else if (xcvr_name == rig_KX3.name_) {
					if (que_pending()) continue;
					read_KX3();
				}
				if ((xcvr_name == rig_K2.name_) ||
					(selrig->has_get_info &&
						(progStatus.poll_frequency ||
						 progStatus.poll_mode ||
						 progStatus.poll_bandwidth) ) )
					read_info();
				if (que_pending() || bypass_serial_thread_loop) continue;
				poll_parameters = &RX_poll_pairs[0];
				while (poll_parameters->poll) {
					if (que_pending() || bypass_serial_thread_loop) break;
					if (*(poll_parameters->poll) && !(poll_nbr % *(poll_parameters->poll)))
						(poll_parameters->pollfunc)();
					poll_parameters++;
				}
			}
		} else {
			if (resetxmt) {
				Fl::awake(updateSmeter, (void *)(0));
				resetxmt = false;
				loopcount = progStatus.serloop_timing / 10;
				poll_nbr = 0;
			}
			resetrcv = true;
			if (--loopcount <= 0) {
				loopcount = progStatus.serloop_timing / 10;
				poll_nbr++;
				poll_parameters = &TX_poll_pairs[0];
				while (poll_parameters->poll) {
					if (!quePTT.empty()) break;
					if (*(poll_parameters->poll) && !(poll_nbr % *(poll_parameters->poll)))
						(poll_parameters->pollfunc)();
					poll_parameters++;
				}
			}
		}
serial_bypass_loop: ;
	}
	return NULL;
}

//=============================================================================

void setBW()
{
	FREQMODE fm = vfo;
	fm.src = UI;
	fm.iBW = opBW->index();
	if (useB) {
		guard_lock queB_lock(&mutex_queB, 30);
		queB.push(fm);
	} else {
		guard_lock queA_lock(&mutex_queA, 31);
		queA.push(fm);
	}
}

void setDSP()
{
	FREQMODE fm = vfo;
	fm.src = UI;
	fm.iBW = ((opDSP_hi->index() << 8) | 0x8000) | (opDSP_lo->index() & 0xFF) ;
	if (useB) {
		guard_lock queB_lock(&mutex_queB, 32);
		queB.push(fm);
	} else {
		guard_lock queA_lock(&mutex_queA, 33);
		queA.push(fm);
	}
}

void selectDSP()
{
	if (btnDSP->label()[0] == selrig->SL_label[0]) {
		btnDSP->label(selrig->SH_label);
		btnDSP->redraw_label();
		opDSP_hi->show();
		opDSP_lo->hide();
	} else {
		btnDSP->label(selrig->SL_label);
		btnDSP->redraw_label();
		opDSP_lo->show();
		opDSP_hi->hide();
	}
}

// set_bandwidth_control updates iBW and then posts the call for
// the UI thread to updateBandwidthControl
// changes to the UI cannot come from any thread other than the
// main FL thread!  Doing otherwise can cause what appears to be
// random program crashes.

void set_bandwidth_control()
{
	if (!selrig->has_bandwidth_control) return;

	vfo.iBW = selrig->adjust_bandwidth(vfo.imode);

	if (vfo.iBW < 256) {
		int i = 0;
		while (selrig->bandwidths_[i]) i++;
		i--;
		if (vfo.iBW > i) vfo.iBW = selrig->def_bandwidth(vfo.imode);
	}
	useB ? vfoB.iBW = vfo.iBW : vfoA.iBW = vfo.iBW;
	Fl::awake(updateBandwidthControl);
}

void updateBandwidthControl(void *d)
{
	if (selrig->has_bandwidth_control) {
		if (selrig->adjust_bandwidth(vfo.imode) != -1) {
			opBW->clear();
			rigbws_.clear();
			for (int i = 0; selrig->bandwidths_[i] != NULL; i++) {
				rigbws_.push_back(selrig->bandwidths_[i]);
				opBW->add(selrig->bandwidths_[i]);
			}

			if (selrig->has_dsp_controls) {
				opDSP_lo->clear();
				opDSP_hi->clear();
				for (int i = 0; selrig->dsp_SL[i] != NULL; i++)
					opDSP_lo->add(selrig->dsp_SL[i]);
				for (int i = 0; selrig->dsp_SH[i] != NULL; i++)
					opDSP_hi->add(selrig->dsp_SH[i]);
				if (vfo.iBW > 256) {
					opBW->index(0);
					opBW->hide();
					opBW->hide();
					opDSP_lo->index(vfo.iBW & 0xFF);
					opDSP_lo->hide();
					opDSP_hi->index((vfo.iBW >> 8) & 0x7F);
					btnDSP->label(selrig->SL_label);
					opDSP_lo->show();
					btnDSP->show();
				} else {
					opDSP_lo->hide();
					opDSP_hi->hide();
					btnDSP->hide();
					opBW->index(vfo.iBW);
					opBW->show();
				}
			} else {  // no DSP control so update BW control, hide DSP
				opDSP_lo->hide();
				opDSP_hi->hide();
				btnDSP->hide();
				opBW->index(vfo.iBW);
				opBW->show();
			}
		}
	} else { // no BW, no DSP controls
		opBW->index(0);
		opBW->hide();
		opDSP_lo->index(0);
		opDSP_hi->index(0);
		btnDSP->hide();
		opDSP_lo->hide();
		opDSP_hi->hide();
	}
}

void setMode()
{
	FREQMODE fm = vfo;
	fm.imode = opMODE->index();
	fm.iBW = selrig->def_bandwidth(fm.imode);
	fm.src = UI;

	if (useB) {
		guard_lock queB_lock(&mutex_queB, 34);
		queB.push(fm);
	} else {
		guard_lock queA_lock(&mutex_queA, 35);
		queA.push(fm);
	}
}

void sortList() {
	if (!numinlist) return;
	ATAG_FREQMODE temp;
	for (int i = 0; i < numinlist - 1; i++)
		for (int j = i + 1; j < numinlist; j++)
			if (oplist[i].freq > oplist[j].freq) {
					temp = oplist[i];
					oplist[i] = oplist[j];
					oplist[j] = temp;
			}
}

void clearList() {
	if (!numinlist) return;
	for (int i = 0; i < LISTSIZE; i++) {
		oplist[i].freq = 0;
		oplist[i].imode = USB;
		oplist[i].iBW = 0;
		memset(oplist[i].alpha_tag, 0, ATAGSIZE);
	}
	FreqSelect->clear();
	numinlist = 0;
	inAlphaTag->value("");
}

void updateSelect() {
	char szline[80 + ATAGSIZE];
	char szatag[ATAGSIZE];
	int i;
	if (!numinlist) return;
	sortList();
	FreqSelect->clear();
// stripe lines
	int bg1, bg2, bg_clr;
	bg1 = FL_WHITE; bg2 = FL_LIGHT2;

	for (int n = 0; n < numinlist; n++) {
		memset(szline, 0, sizeof(szline));
		memset(szatag, 0, sizeof(szatag));
		for (i = 0; i < ATAGSIZE - 1; i++) {
			szatag[i] = oplist[n].alpha_tag[i];
			if (szatag[i] == 0) szatag[i] = ' ';
		}
		bg_clr = (n % 2) ? bg1 : bg2;
		snprintf(szline, sizeof(szline),
			"@B%d@r%.3f\t@B%d@r%s\t@B%d@r%s\t@B%d@r%s", bg_clr,
			oplist[n].freq / 1000.0, bg_clr,
			selrig->get_bwname_(oplist[n].iBW, oplist[n].imode), bg_clr,
			selrig->get_modename_(oplist[n].imode), bg_clr,
			szatag );
		FreqSelect->add (szline);
	}
	inAlphaTag->value("");
}

void addtoList(int val, int imode, int iBW) {
	if (numinlist < LISTSIZE) {
		oplist[numinlist].imode = imode;
		oplist[numinlist].freq = val;
		oplist[numinlist].iBW = iBW;
		memset(oplist[numinlist].alpha_tag, 0, ATAGSIZE);
		numinlist++;
	}
}

void readFile() {
	ifstream iList(defFileName.c_str());
	if (!iList) {
		fl_message ("Could not open %s", defFileName.c_str());
		return;
	}
	clearList();
	int i = 0, mode, bw;
	long freq;
	while (!iList.eof()) {
		freq = 0L; mode = -1;
		iList >> freq >> mode >> bw;
		if (freq && (mode > -1)) {
			oplist[i].freq = freq;
			oplist[i].imode = mode;
			oplist[i].iBW = (bw == -1 ? 0 : bw);
			memset(oplist[i].alpha_tag, 0, ATAGSIZE);
			i++;
		}
	}
	iList.close();
	numinlist = i;
	updateSelect();
}

void readTagFile() {
	ifstream iList(defFileName.c_str());
	if (!iList) {
		fl_message ("Could not open %s", defFileName.c_str());
		return;
	}
	clearList();
	int i = 0, mode, bw;
	long freq;
	string atag;
	char ca[ATAGSIZE + 60];
	while (!iList.eof()) {
		freq = 0L; mode = -1;
		atag.clear();
		memset(ca, 0, sizeof(ca));
		iList >> freq >> mode >> bw;
		iList.getline(ca, sizeof(ca) - 1);
		atag = ca;
		if (freq && (mode > -1)) {
			oplist[i].freq = freq;
			oplist[i].imode = mode;
			oplist[i].iBW = (bw == -1 ? 0 : bw);
// trim leading, trailing spaces and double quotes
			atag = lt_trim(atag);
			snprintf(oplist[i].alpha_tag, ATAGSIZE, "%s", atag.c_str());
			i++;
		}
	}
	iList.close();
	numinlist = i;
	updateSelect();
}

void buildlist() {
	string tmpFN, orgFN;
// check for new Memory-Alpha-Tag file
	defFileName = RigHomeDir;
	defFileName.append(selrig->name_);
	defFileName.append(".mat");
	FILE *fh = fopen(defFileName.c_str(), "r");
	if (fh != NULL) {
		fclose (fh);
		readTagFile();
		return;
	}
// else only read original file to make new MAT file
	orgFN = RigHomeDir;
	orgFN.append(selrig->name_);
	orgFN.append(".arv");
	fh = fopen(orgFN.c_str(), "r");
	if (fh != NULL) {
		fclose (fh);
		tmpFN = defFileName;
		defFileName = orgFN;
		readFile();
		defFileName = tmpFN;
		return;
	}
	clearList();
}

// flrig front panel changed

int movFreqA() {
	FREQMODE vfo = vfoA;
	vfo.freq = FreqDispA->value();
	vfo.src = UI;
	guard_lock queA_lock(&mutex_queA, 36);
	queA.push(vfo);
	return 1;
}

int movFreqB() {
	if (progStatus.split && (!selrig->twovfos()))
		return 0; // disallow for ICOM transceivers
	FREQMODE vfo = vfoB;
	vfo.freq = FreqDispB->value();
	vfo.src = UI;
	guard_lock queB_lock(&mutex_queB, 37);
	queB.push(vfo);
	return 1;
}

void CATswapAB() // called by UI action; do not need Fl::awake(...)
{
	guard_lock queA_lock(&mutex_queA);
	while (!queA.empty()) queA.pop();

	guard_lock queB_lock(&mutex_queB);
	while (!queB.empty()) queB.pop();

	guard_lock serial_lock(&mutex_serial);
	selrig->swapvfos();

// Recent Icom xcvrs simply select A or B along with all of the xcvr
// internal items associated with the newly active VFO

	if (selrig->ICswap()) {
		useB = !useB;
		Fl_Color norm_fg = fl_rgb_color(progStatus.fg_red, progStatus.fg_green, progStatus.fg_blue);
		Fl_Color norm_bg = fl_rgb_color(progStatus.bg_red, progStatus.bg_green, progStatus.bg_blue);
		Fl_Color dim_bg = fl_color_average( norm_bg, FL_BLACK, 0.75);
		if (useB) {
			FreqDispA->SetONOFFCOLOR( norm_fg, dim_bg );
			FreqDispB->SetONOFFCOLOR( norm_fg, norm_bg );
			btnA->value(0);
			btnB->value(1);
		} else {
			FreqDispA->SetONOFFCOLOR( norm_fg, norm_bg );
			FreqDispB->SetONOFFCOLOR( norm_fg, dim_bg);
			btnA->value(1);
			btnB->value(0);
		}
		FreqDispA->redraw();
		FreqDispB->redraw();
		btnA->redraw();
		btnB->redraw();

	} else {

		vfoB.freq = selrig->get_vfoB();
		vfoB.imode = selrig->get_modeB();
		vfoB.iBW = selrig->get_bwB();
		FreqDispB->value(vfoB.freq);
		FreqDispB->redraw();

		vfoA.freq = selrig->get_vfoA();
		vfoA.imode = selrig->get_modeA();
		vfoA.iBW = selrig->get_bwA();
		FreqDispA->value(vfoA.freq);
		FreqDispA->redraw();

		vfo = vfoA;
	}

	if (selrig->has_dsp_controls) {
		if (vfo.iBW > 256) {
			opBW->index(0);
			opBW->hide();
			opDSP_hi->index((vfo.iBW >> 8) & 0x7F);
			opDSP_hi->hide();
			opDSP_lo->index(vfo.iBW & 0xFF);
			opDSP_lo->show();
			btnDSP->label(selrig->SL_label);
			btnDSP->redraw_label();
			btnDSP->show();
		} else {
			opDSP_lo->index(0);
			opDSP_hi->index(0);
			opDSP_lo->hide();
			opDSP_hi->hide();
			btnDSP->hide();
			opBW->index(vfo.iBW);
			opBW->show();
		}
	} else {
		opDSP_lo->hide();
		opDSP_hi->hide();
		btnDSP->hide();
		opBW->index(vfo.iBW);
		opBW->show();
	}

	return;
}

void cbAswapB()
{
	if (Fl::event_button() == FL_RIGHT_MOUSE) {
		return cbA2B();
	}
	if (selrig->canswap()) {
		CATswapAB();
		return;
	}
	if (!selrig->twovfos()) {
		vfoB.freq = FreqDispB->value();
		FREQMODE temp = vfoB;
		vfoB = vfoA;
		vfoA = temp;
		FreqDispB->value(vfoB.freq);
		FreqDispB->redraw();
		FreqDispA->value(vfoA.freq);
		FreqDispA->redraw();
		if (!useB) {
			guard_lock queA_lock(&mutex_queA, 38);
			while (!queA.empty()) queA.pop();
			queA.push(vfoA);
		} else {
			guard_lock queB_lock(&mutex_queB, 38);
			while (!queB.empty()) queB.pop();
			queB.push(vfoB);
		}
	} else {
		guard_lock serial_lock(&mutex_serial, 39);
		if (!useB) {      // vfoA is used, swap vfos and update display B
			vfoB.freq = FreqDispB->value();
			FREQMODE temp = vfoB;
			vfoB = vfoA;
			vfoA = temp;
			FreqDispB->value(vfoB.freq);
			FreqDispB->redraw();
		} else {          // vfoB is used, swap vfos and update display A
			vfoA.freq = FreqDispA->value();
			FREQMODE temp = vfoA;
			vfoA = vfoB;
			vfoB = temp;
			FreqDispA->value(vfoA.freq);
			FreqDispA->redraw();
		}
		guard_lock queA_lock(&mutex_queA, 40);
		while (!queA.empty()) queA.pop();
		queA.push(vfoA);
		guard_lock queB_lock(&mutex_queB, 41);
		while (!queB.empty()) queB.pop();
		queB.push(vfoB);
		pushedB = true;
	}
}

void cbA2B()
{
	if (xcvr_name == rig_K3.name_) {
		K3_A2B();
		return;
	}
	if (xcvr_name == rig_KX3.name_) {
		KX3_A2B();
		return;
	}
	if (xcvr_name == rig_K2.name_) {
		guard_lock serial_lock(&mutex_serial, 43);
		vfoB = vfoA;
		selrig->set_vfoB(vfoB.freq);
		FreqDispB->value(vfoB.freq);
		return;
	}
	if (selrig->has_a2b) {
		guard_lock serial_lock(&mutex_serial, 44);
		selrig->A2B();
	}
	vfoB = vfoA;
	{
		guard_lock queB_lock(&mutex_queB, 45);
		queB.push(vfoB);
	}
	FreqDispB->value(vfoB.freq);
	FreqDispB->redraw();
	pushedB = true;
}

void highlight_vfo(void *d)
{
	Fl_Color norm_fg = fl_rgb_color(progStatus.fg_red, progStatus.fg_green, progStatus.fg_blue);
	Fl_Color norm_bg = fl_rgb_color(progStatus.bg_red, progStatus.bg_green, progStatus.bg_blue);
	Fl_Color dim_bg = fl_color_average( norm_bg, FL_BLACK, 0.75);
	if (useB) {
		FreqDispA->SetONOFFCOLOR( norm_fg, dim_bg );
		FreqDispB->SetONOFFCOLOR( norm_fg, norm_bg );
		btnA->value(0);
		btnB->value(1);
	} else {
		FreqDispA->SetONOFFCOLOR( norm_fg, norm_bg );
		FreqDispB->SetONOFFCOLOR( norm_fg, dim_bg);
		btnA->value(1);
		btnB->value(0);
	}
	FreqDispA->redraw();
	FreqDispB->redraw();
	btnA->redraw();
	btnB->redraw();
	Fl::flush();
}

void cb_set_split(int val)
{
	progStatus.split = val;

	if (selrig->has_split_AB) {
		guard_lock serial_lock(&mutex_serial);
		selrig->set_split(val);
	} else if (val) {
		if (useB) {
			if (vfoB.freq != FreqDispB->value()) {
				vfoB.freq = FreqDispB->value();
				guard_lock serial_lock(&mutex_serial, 47);
				selrig->selectB();
				selrig->set_vfoB(vfoB.freq);
				selrig->selectA();
			}
			btnA->value(1);
			btnB->value(0);
			cb_selectA();
		}
	} else
		cb_selectA();
}

void cb_selectA() {
	if (progStatus.split) {
		btnSplit->value(0);
		cb_set_split(0);
	}
	guard_lock serial_lock(&mutex_serial);
	guard_lock queA_lock(&mutex_queA);
	guard_lock queB_lock(&mutex_queB);

	changed_vfo = true;
	vfoA.src = UI;
	vfoA.freq = FreqDispA->value();

	while (!queB.empty()) queB.pop();
	while (!queA.empty()) queA.pop();
	queA.push(vfoA);

	useB = false;
	highlight_vfo((void *)0);
}

void cb_selectB() {
	if (progStatus.split) {
		btnSplit->value(0);
		cb_set_split(0);
	}
	guard_lock serial_lock(&mutex_serial);
	guard_lock queA_lock(&mutex_queA);
	guard_lock queB_lock(&mutex_queB);

	changed_vfo = true;
	vfoB.src = UI;
	vfoB.freq = FreqDispB->value();

	while (!queA.empty()) queA.pop();
	while (!queB.empty()) queB.pop();
	queB.push(vfoB);

	useB = true;
	highlight_vfo((void *)0);
}

void setLower()
{
}

void setUpper()
{
}

void selectFreq() {
	long n = FreqSelect->value();
	if (!n) return;

	n--;
	FREQMODE fm;
	fm.freq  = oplist[n].freq;
	fm.imode = oplist[n].imode;
	fm.iBW   = oplist[n].iBW;
	fm.src   = UI;
	if (!useB) {
		FreqDispA->value(fm.freq);
		guard_lock queA_lock(&mutex_queA, 52);
		queA.push(fm);
	} else {
		FreqDispB->value(fm.freq);
		guard_lock queB_lock(&mutex_queB, 53);
		queB.push(fm);
	}
}

void select_and_close()
{
	switch (Fl::event_button()) {
		case FL_LEFT_MOUSE:
			if (FreqSelect->value() > 0)
				inAlphaTag->value(oplist[FreqSelect->value() - 1].alpha_tag);
			if (Fl::event_clicks()) { // double click
				selectFreq();
				cbCloseMemory();
			}
			break;
		case FL_RIGHT_MOUSE:
			if (FreqSelect->value() > 0)
				inAlphaTag->value(oplist[FreqSelect->value() - 1].alpha_tag);
			selectFreq();
			break;
		default:
			break;
	}

// update Alpha Tag field when keyboard scrolling
	switch (Fl::event_key()) {
		case FL_Up:
		case FL_Down:
			if (FreqSelect->value() > 0)
				inAlphaTag->value(oplist[FreqSelect->value() - 1].alpha_tag);
			break;
		default:
			break;
	}
}

void delFreq() {
	if (FreqSelect->value()) {
		long n = FreqSelect->value() - 1;
		for (int i = n; i < numinlist; i ++)
			oplist[i] = oplist[i+1];
		oplist[numinlist - 1].imode = USB;
		oplist[numinlist - 1].freq = 0;
		oplist[numinlist - 1].iBW = 0;
		memset(oplist[numinlist - 1].alpha_tag, 0, ATAGSIZE);
		numinlist--;
		updateSelect();
	}
}

void addFreq() {
	if (useB) {
		long freq = FreqDispB->value();
		if (!freq) return;
		int mode = opMODE->index();
		int bw;
		if (btnDSP->visible())
			bw = ((opDSP_hi->index() << 8) | 0x8000) | (opDSP_lo->index() & 0xFF) ;
		else
			bw = opBW->index();
		for (int n = 0; n < numinlist; n++)
			if (freq == oplist[n].freq && mode == oplist[n].imode) {
				oplist[n].iBW = bw;
				updateSelect();	// update list
				return;
			}
		addtoList(freq, mode, bw);
		updateSelect();
		FreqDispB->visual_beep();
		} else {
		long freq = FreqDispA->value();
		if (!freq) return;
		int mode = opMODE->index();
		int bw;
		if (btnDSP->visible())
			bw = ((opDSP_hi->index() << 8) | 0x8000) | (opDSP_lo->index() & 0xFF) ;
		else
			bw = opBW->index();
		for (int n = 0; n < numinlist; n++)
			if (freq == oplist[n].freq && mode == oplist[n].imode) {
				oplist[n].iBW = bw;
				updateSelect();	// update list
				return;
			}
		addtoList(freq, mode, bw);
		updateSelect();
		FreqDispA->visual_beep();
		}
}

void cbRIT()
{
	guard_lock serial_lock(&mutex_serial, 54);
	if (selrig->has_rit)
		selrig->setRit((int)cntRIT->value());
}

void cbXIT()
{
	guard_lock serial_lock(&mutex_serial, 55);
	selrig->setXit((int)cntXIT->value());
}

void cbBFO()
{
	if (selrig->has_bfo) {
		guard_lock serial_lock(&mutex_serial, 56);
		selrig->setBfo((int)cntBFO->value());
	}
}

void cbAttenuator()
{
	guard_lock serial_lock(&mutex_serial, 57);
	progStatus.attenuator = btnAttenuator->value();
	selrig->set_attenuator(progStatus.attenuator);
}

void setAttControl(void *d)
{
	int val = (long)d;
	btnAttenuator->value(val);
}

void cbPreamp()
{
	guard_lock serial_lock(&mutex_serial, 58);
	progStatus.preamp = btnPreamp->value();
	selrig->set_preamp(progStatus.preamp);
}

void setPreampControl(void *d)
{
	int val = (long)d;
	btnPreamp->value(val);
}

void cbNoise()
{
	progStatus.noise = btnNOISE->value();
	guard_lock serial_lock(&mutex_serial, 59);
	selrig->set_noise(progStatus.noise);
}

void cbNR()
{
	if (!selrig->has_noise_reduction_control) return;
	noise_reduction_changed = true;
	if (sldrNR) sliders.push(SLIDER(NR, sldrNR->value(), btnNR->value() ) );
	if (spnrNR) sliders.push(SLIDER(NR, spnrNR->value(), btnNR->value() ) );
}

void setNR()
{
	if (!selrig->has_noise_reduction_control) return;
	noise_reduction_changed = true;
	if (xcvr_name == rig_TS2000.name_ ||
		xcvr_name == rig_TS590S.name_ ||
		xcvr_name == rig_TS590SG.name_ ||
		xcvr_name == rig_TS990.name_) {
		if (sldrNR) sliders.push(SLIDER(NR, sldrNR->value(), -1 ) );
		if (spnrNR) sliders.push(SLIDER(NR, spnrNR->value(), -1 ) );
	} else {
		if (sldrNR) sliders.push(SLIDER(NR, sldrNR->value(), btnNR->value() ) );
		if (spnrNR) sliders.push(SLIDER(NR, spnrNR->value(), btnNR->value() ) );
	}
}

void cbAN()
{
	progStatus.auto_notch = btnAutoNotch->value();
	guard_lock serial_lock(&mutex_serial, 60);
	selrig->set_auto_notch(progStatus.auto_notch);
}

void cbbtnNotch()
{
	if (!selrig->has_notch_control) return;
	notch_changed = true;
	if (sldrNOTCH) sliders.push(SLIDER(NOTCH, sldrNOTCH->value(), btnNotch->value() ));
	if (spnrNOTCH) sliders.push(SLIDER(NOTCH, spnrNOTCH->value(), btnNotch->value() ));
}

void setNotch()
{
	if (!selrig->has_notch_control) return;
	notch_changed = true;
	if (sldrNOTCH) sliders.push(SLIDER(NOTCH, sldrNOTCH->value(), btnNotch->value() ));
	if (spnrNOTCH) sliders.push(SLIDER(NOTCH, spnrNOTCH->value(), btnNotch->value() ));
}

// called from xml_io thread
void setNotchControl(void *d)
{
//	guard_lock serial_lock(&mutex_serial, 61);
//	selrig->set_notch(progStatus.notch, progStatus.notch_val);

	if (sldrNOTCH) sldrNOTCH->value(progStatus.notch_val);
	if (spnrNOTCH) spnrNOTCH->value(progStatus.notch_val);
	btnNotch->value(progStatus.notch);
}

void adjust_if_shift_control(void *d)
{
	if (sldrIFSHIFT) sldrIFSHIFT->minimum(selrig->if_shift_min);
	if (sldrIFSHIFT) sldrIFSHIFT->maximum(selrig->if_shift_max);
	if (sldrIFSHIFT) sldrIFSHIFT->step(selrig->if_shift_step);
	if (sldrIFSHIFT) sldrIFSHIFT->value(selrig->if_shift_mid);
	if (sldrIFSHIFT) sldrIFSHIFT->redraw();
	if (spnrIFSHIFT) spnrIFSHIFT->minimum(selrig->if_shift_min);
	if (spnrIFSHIFT) spnrIFSHIFT->maximum(selrig->if_shift_max);
	if (spnrIFSHIFT) spnrIFSHIFT->step(selrig->if_shift_step);
	if (spnrIFSHIFT) spnrIFSHIFT->value(selrig->if_shift_mid);
	if (spnrIFSHIFT) spnrIFSHIFT->redraw();
	btnIFsh->value(0);
	btnIFsh->redraw();
}

void setIFshiftButton(void *d)
{
	bool b = (bool)d;
	if (b && !btnIFsh->value()) {
		btnIFsh->value(1);
	}
	else if (!b && btnIFsh->value()) {
		btnIFsh->value(0);
		if (sldrIFSHIFT) sldrIFSHIFT->value( selrig->if_shift_mid );
		if (spnrIFSHIFT) spnrIFSHIFT->value( selrig->if_shift_mid );
	}
}

void setIFshiftControl(void *d)
{
	int val = (long)d;
	if (sldrIFSHIFT) {
		if (sldrIFSHIFT->value() != val)
			sldrIFSHIFT->value(val);
	}
	if (spnrIFSHIFT) {
		if (spnrIFSHIFT->value() != val)
			spnrIFSHIFT->value(val);
	}
	btnIFsh->value( val != selrig->if_shift_mid );
}

void setIFshift()
{
	if_shift_changed = true;
	if (sldrIFSHIFT) sliders.push(SLIDER(IFSH, sldrIFSHIFT->value(), btnIFsh->value()));
	if (spnrIFSHIFT) sliders.push(SLIDER(IFSH, spnrIFSHIFT->value(), btnIFsh->value()));
}

void cbIFsh()
{
	if_shift_changed = true;
	if (sldrIFSHIFT) sliders.push(SLIDER(IFSH, sldrIFSHIFT->value(), btnIFsh->value()));
	if (spnrIFSHIFT) sliders.push(SLIDER(IFSH, spnrIFSHIFT->value(), btnIFsh->value()));
}

void cbEventLog()
{
	debug::show();
}

void setVolume()
{
	volume_changed = true;
	if (spnrVOLUME) sliders.push(SLIDER(VOL, spnrVOLUME->value()));
	if (sldrVOLUME) sliders.push(SLIDER(VOL, sldrVOLUME->value()));
}

void cbMute()
{
	volume_changed = true;
	if (btnVol->value() == 0) {
		if (spnrVOLUME) spnrVOLUME->deactivate();
		if (sldrVOLUME) sldrVOLUME->deactivate();
		sliders.push(SLIDER(VOL, 0));
	} else {
		if (spnrVOLUME) {
			spnrVOLUME->activate();
			sliders.push(SLIDER(VOL, spnrVOLUME->value()));
		}
		if (sldrVOLUME) {
			sldrVOLUME->activate();
			sliders.push(SLIDER(VOL, sldrVOLUME->value()));
		}
	}
}

void setMicGain()
{
	mic_changed = true;
	if (sldrMICGAIN) sliders.push(SLIDER(MIC, sldrMICGAIN->value()));
	if (spnrMICGAIN) sliders.push(SLIDER(MIC, spnrMICGAIN->value()));
}

void setMicGainControl(void* d)
{
	int val = (long)d;
	if (sldrMICGAIN) sldrMICGAIN->value(val);
	if (spnrMICGAIN) spnrMICGAIN->value(val);
}

static int img = -1;

void set_power_controlImage(double pwr)
{
	if (progStatus.pwr_scale == 0 || (progStatus.pwr_scale == 5 && pwr <= 5.0)) {
		if (img != 1) {
			img = 1;
			scalePower->image(image_p5);
			sldrFwdPwr->maximum(5.0);
			sldrFwdPwr->minimum(0.0);
			scalePower->redraw();
		}
	}
	else if (progStatus.pwr_scale == 1 || (progStatus.pwr_scale == 5 && pwr <= 25.0)) {
		if (img != 2) {
			img = 2;
			scalePower->image(image_p25);
			sldrFwdPwr->maximum(25.0);
			sldrFwdPwr->minimum(0.0);
			scalePower->redraw();
		}
	}
	else if (progStatus.pwr_scale == 2 || (progStatus.pwr_scale == 5 && pwr <= 50.0)) {
		if (img != 3) {
			img = 3;
			scalePower->image(image_p50);
			sldrFwdPwr->maximum(50.0);
			sldrFwdPwr->minimum(0.0);
			scalePower->redraw();
		}
	}
	else if (progStatus.pwr_scale == 3 || (progStatus.pwr_scale == 5 && pwr <= 100.0)) {
		if (img != 4) {
			img = 4;
			scalePower->image(image_p100);
			sldrFwdPwr->maximum(100.0);
			sldrFwdPwr->minimum(0.0);
			scalePower->redraw();
		}
	}
	else if (progStatus.pwr_scale == 4 || (pwr > 100.0)) {
		if (img != 5) {
			img = 5;
			scalePower->image(image_p200);
			sldrFwdPwr->maximum(200.0);
			sldrFwdPwr->minimum(0.0);
			scalePower->redraw();
		}
	}
	return;
}

void setPower()
{
	float pwr = 0;
	if (spnrPOWER) pwr = progStatus.power_level = spnrPOWER->value();
	if (sldrPOWER) pwr = progStatus.power_level = sldrPOWER->value();
	if (xcvr_name == rig_K2.name_) {
		double min, max, step;
		selrig->get_pc_min_max_step(min, max, step);
		if (spnrPOWER) spnrPOWER->minimum(min);
		if (spnrPOWER) spnrPOWER->maximum(max);
		if (spnrPOWER) spnrPOWER->step(step);
		if (spnrPOWER) spnrPOWER->value(progStatus.power_level);
		if (spnrPOWER) spnrPOWER->redraw();
		if (sldrPOWER) sldrPOWER->minimum(min);
		if (sldrPOWER) sldrPOWER->maximum(max);
		if (sldrPOWER) sldrPOWER->step(step);
		if (sldrPOWER) sldrPOWER->value(progStatus.power_level);
		if (sldrPOWER) sldrPOWER->redraw();
	}
	power_changed = true;
	sliders.push(SLIDER(PWR, progStatus.power_level));
	set_power_controlImage(pwr);
}

void cbTune()
{
	guard_lock serial_lock(&mutex_serial, 62);
	selrig->tune_rig();
}

void cbPTT()
{
	guard_lock ptt_lock(&mutex_ptt, 63);
	quePTT.push(btnPTT->value());
}

void setSQUELCH()
{
	squelch_changed = true;
	if (sldrSQUELCH) sliders.push(SLIDER(SQL, sldrSQUELCH->value() ));
	if (spnrSQUELCH) sliders.push(SLIDER(SQL, spnrSQUELCH->value() ));
}

// val == 0 --> AGC off
// val > 1  --> AGC active and set

int agcwas = 0;
void redrawAGC()
{
	const char *lbl = selrig->agc_label();
	int val = selrig->agc_val();

	btnAGC->label(lbl);
	btnAGC->redraw_label();

	int rignbr = 0;
	if (xcvr_name == rig_IC7200.name_) rignbr = 1;
	if (xcvr_name == rig_IC7300.name_) rignbr = 2;
	if (rignbr) {
		if (val == 0) btnAGC->selection_color(FL_BACKGROUND_COLOR);  // off
		if (val == 1) btnAGC->selection_color(
			rignbr == 1 ? FL_GREEN : FL_RED); // fast
		if (val == 2) btnAGC->selection_color(
			rignbr == 1 ? FL_RED : FL_YELLOW);  // med / slow
		if (val == 3) btnAGC->selection_color(FL_GREEN); // slow
		btnAGC->value(val > 0);
	} else {
		if (val == 0)
			btnAGC->value(0);
		else
			btnAGC->value(1);
	}

	if (agcwas != val) {
		agcwas = val;
	}

	btnAGC->redraw();
}

void setAGC(void *)
{
	if (!selrig->has_agc_control) return;
	redrawAGC();
}

void cbAGC()
{
	if (!selrig->has_agc_control) return;
	guard_lock serial_lock(&mutex_serial);
	progStatus.agc_level = selrig->incr_agc();
	redrawAGC();
}

void setRFGAIN()
{
	rfgain_changed = true;
	if (spnrRFGAIN) sliders.push(SLIDER(RFGAIN, spnrRFGAIN->value()));
	if (sldrRFGAIN) sliders.push(SLIDER(RFGAIN, sldrRFGAIN->value()));
}


void updateALC(void * d)
{
	if (meter_image != ALC_IMAGE) return;
	double data = (long)d;
	sldrRcvSignal->hide();
	sldrSWR->hide();
	sldrALC->value(data);
	sldrALC->redraw();
}

void updateSWR(void * d)
{
	if (meter_image != SWR_IMAGE) return;
	double data = (long)d;
	if (selrig->has_swr_control) {
		sldrRcvSignal->hide();
		sldrALC->hide();
		sldrSWR->show();
	}
	sldrSWR->value(data);
	sldrSWR->redraw();
}

void updateFwdPwr(void *d)
{
	double power = (long)d;
	if (!sldrFwdPwr->visible()) {
		sldrFwdPwr->show();
	}
	if (xcvr_name == rig_FT817.name_) sldrFwdPwr->value(power / 10);
	else sldrFwdPwr->value(power);
	sldrFwdPwr->redraw();
	if (!selrig->has_power_control)
		set_power_controlImage(sldrFwdPwr->peak());
}

void updateSquelch(void *d)
{
	if (sldrSQUELCH) sldrSQUELCH->value((long)d);
	if (sldrSQUELCH) sldrSQUELCH->redraw();
	if (spnrSQUELCH) spnrSQUELCH->value((long)d);
	if (spnrSQUELCH) spnrSQUELCH->redraw();
}

void updateRFgain(void *d)
{
	if (spnrRFGAIN) {
		spnrRFGAIN->value((long)d);
		spnrRFGAIN->redraw();
	}
	if (sldrRFGAIN) {
		sldrRFGAIN->value((long)d);
		sldrRFGAIN->redraw();
	}
}

void zeroXmtMeters(void *d)
{
	sldrFwdPwr->clear();
	sldrALC->clear();
	sldrSWR->clear();
	updateFwdPwr(0);
	updateALC(0);
	updateSWR(0);
}

void setFreqDispA(void *d)
{
	long f = (long)d;
	FreqDispA->value(f);
	FreqDispA->redraw();
}

void setFreqDispB(void *d)
{
	long f = (long)d;
	FreqDispB->value(f);
	FreqDispB->redraw();
}

void updateSmeter(void *d) // 0 to 100;
{
	double smeter = (long)d;
	if (!sldrRcvSignal->visible()) {
		sldrRcvSignal->show();
		sldrFwdPwr->hide();
		sldrALC->hide();
		sldrSWR->hide();
	}
	sldrRcvSignal->value(smeter);
	sldrRcvSignal->redraw();
}

void saveFreqList()
{
	string atag;
	if (!numinlist) {
		remove(defFileName.c_str());
		return;
	}
	ofstream oList(defFileName.c_str());
	if (!oList) {
		fl_message ("Could not write to %s", defFileName.c_str());
		return;
	}
	for (int i = 0; i < numinlist; i++) {
		atag = oplist[i].alpha_tag;
		oList << oplist[i].freq << " " << oplist[i].imode << " " << oplist[i].iBW << " \"" << atag.c_str() << "\"" << endl;

	}
	oList.close();
}

void setPTT( void *d)
{
	int val = (long)d;
	guard_lock ptt_lock(&mutex_ptt, 64);
	quePTT.push(val);
}


void closeRig()
{
	// restore initial rig settings
	if (!progStatus.restore_rig_data) return;

	guard_lock serial_lock(&mutex_serial, 65);
	selrig->selectB();
	selrig->set_vfoB(transceiverB.freq);
	selrig->set_modeB(transceiverB.imode);
	selrig->set_bwB(transceiverB.iBW);
	selrig->selectA();
	selrig->set_vfoA(transceiverA.freq);
	selrig->set_modeA(transceiverA.imode);
	selrig->set_bwA(transceiverA.iBW);
}


void cbExit()
{
	progStatus.freq_A = vfoA.freq;
	progStatus.imode_A = vfoA.imode;
	progStatus.iBW_A = vfoA.iBW;

	progStatus.freq_B = vfoB.freq;
	progStatus.imode_B = vfoB.imode;
	progStatus.iBW_B = vfoB.iBW;

	progStatus.spkr_on = btnVol->value();

	if (spnrPOWER) progStatus.power_level = spnrPOWER->value();
	if (spnrVOLUME) progStatus.volume = spnrVOLUME->value();
	if (spnrRFGAIN) progStatus.rfgain = spnrRFGAIN->value();
	if (spnrMICGAIN) progStatus.mic_gain = spnrMICGAIN->value();
	if (spnrNOTCH) progStatus.notch_val = spnrNOTCH->value();
	if (spnrIFSHIFT) progStatus.shift_val = spnrIFSHIFT->value();
	if (spnrNR) progStatus.noise_reduction_val = spnrNR->value();

	if (sldrPOWER) progStatus.power_level = sldrPOWER->value();
	if (sldrVOLUME) progStatus.volume = sldrVOLUME->value();
	if (sldrRFGAIN) progStatus.rfgain = sldrRFGAIN->value();
	if (sldrMICGAIN) progStatus.mic_gain = sldrMICGAIN->value();
	if (sldrNOTCH) progStatus.notch_val = sldrNOTCH->value();
	if (sldrIFSHIFT) progStatus.shift_val = sldrIFSHIFT->value();
	if (sldrNR) progStatus.noise_reduction_val = sldrNR->value();

	progStatus.notch = btnNotch->value();
	progStatus.shift = btnIFsh->value();
	progStatus.noise_reduction = btnNR->value();
	progStatus.noise = btnNOISE->value();
	progStatus.attenuator = btnAttenuator->value();
	progStatus.preamp = btnPreamp->value();
	progStatus.auto_notch = btnAutoNotch->value();
	progStatus.saveLastState();

	saveFreqList();

// shutdown serial thread

	{
		guard_lock serial_lock(&mutex_serial, 67);
		run_serial_thread = false;
	}
	pthread_join(*serial_thread, NULL);

	if (progStatus.restore_rig_data){
LOG_INFO("Restore Xcvr Data");
		selrig->selectB();
		selrig->set_vfoB(transceiverB.freq);
		selrig->set_modeB(transceiverB.imode);
		selrig->set_bwB(transceiverB.iBW);
		selrig->selectA();
		selrig->set_vfoA(transceiverA.freq);
		selrig->set_modeA(transceiverA.imode);
		selrig->set_bwA(transceiverA.iBW);
	}

	selrig->shutdown();

	// xcvr auto off
	if (selrig->has_xcvr_auto_on_off)
		selrig->set_xcvr_auto_off();

	// close down the serial port
	RigSerial->ClosePort();

	if (dlgDisplayConfig && dlgDisplayConfig->visible())
		dlgDisplayConfig->hide();
	if (dlgXcvrConfig && dlgXcvrConfig->visible())
		dlgXcvrConfig->hide();
	if (dlgMemoryDialog && dlgMemoryDialog->visible())
		dlgMemoryDialog->hide();
	debug::stop();

	exit(0);
}

void cbALC_SWR()
{
	if (!selrig->has_alc_control) return;
	if (meter_image == SWR_IMAGE) {
		btnALC_SWR->image(image_alc);
		meter_image = ALC_IMAGE;
		sldrALC->show();
		{
			guard_lock serial_lock(&mutex_serial, 68);
			selrig->select_alc();
		}
	} else {
		btnALC_SWR->image(image_swr);
		meter_image = SWR_IMAGE;
		sldrSWR->show();
		{
			guard_lock serial_lock(&mutex_serial, 69);
			selrig->select_swr();
		}
	}
	btnALC_SWR->redraw();
}

void update_UI_PTT(void *d)
{
	btnPTT->value(PTT);
	if (!PTT) {
		btnALC_SWR->hide();
		scaleSmeter->show();
		sldrRcvSignal->clear();
	} else {
		btnALC_SWR->show();
		scaleSmeter->hide();
		sldrFwdPwr->clear();
		sldrALC->clear();
		sldrSWR->clear();
	}
}

void adjust_small_ui()
{
	int y = 0;
	btnVol->hide();
	sldrVOLUME->hide();
	sldrRFGAIN->hide();
	btnIFsh->hide();
	sldrIFSHIFT->hide();
	btnNotch->hide();
	sldrNOTCH->hide();
	sldrMICGAIN->hide();
	btnPOWER->hide();
	sldrPOWER->hide();
	btnPOWER->hide();
	sldrSQUELCH->hide();
	btnNR->hide();
	sldrNR->hide();
	btnAGC->hide();
	sldrRFGAIN->redraw_label();

	if (progStatus.schema == 1 && selrig->widgets[0].W != NULL) {
		int i = 0;
		while (selrig->widgets[i].W != NULL) {
			selrig->widgets[i].W->resize(
				selrig->widgets[i].x, selrig->widgets[i].y,
				selrig->widgets[i].w, selrig->widgets[i].W->h() );
			selrig->widgets[i].W->show();
			selrig->widgets[i].W->redraw();
			if (selrig->widgets[i].y > y) y = selrig->widgets[i].y;
			i++;
		}
		if (selrig->has_data_port) {
			sldrMICGAIN->label("");
			sldrMICGAIN->redraw_label();
		}
		if (selrig->has_power_control) {
			btnPOWER->resize( sldrPOWER->x() - 52, sldrPOWER->y(), 50, 18 );
			btnPOWER->redraw();
			btnPOWER->show();
		}
		mnuSchema->set();
	} else {
		mnuSchema->clear();
		y = cntRIT->y() + 2;
		if (selrig->has_volume_control) {
			y += 20;
			btnVol->position( 2, y);
			btnVol->show();
			btnVol->redraw();
			sldrVOLUME->resize( 54, y, 368, 18 );
			sldrVOLUME->show();
			sldrVOLUME->redraw();
		}
		if (selrig->has_rf_control) {
			y += 20;
			sldrRFGAIN->resize( 54, y, 368, 18 );
			sldrRFGAIN->show();
			sldrRFGAIN->redraw();
		}
		if (selrig->has_sql_control) {
			y += 20;
			sldrSQUELCH->resize( 54, y, 368, 18 );
			sldrSQUELCH->show();
			sldrSQUELCH->redraw();
		}
		if (selrig->has_noise_reduction_control) {
			y += 20;
			btnNR->position( 2, y);
			btnNR->show();
			btnNR->redraw();
			sldrNR->resize( 54, y, 368, 18 );
			sldrNR->show();
			sldrNR->redraw();
			if (xcvr_name == rig_TT599.name_) btnNR->deactivate();
		}
		if (selrig->has_ifshift_control) {
			y += 20;
			btnIFsh->position( 2, y);
			btnIFsh->show();
			btnIFsh->redraw();
			sldrIFSHIFT->resize( 54, y, 368, 18 );
			sldrIFSHIFT->show();
			sldrIFSHIFT->redraw();
		}
		if (selrig->has_notch_control) {
			y += 20;
			btnNotch->position( 2, y);
			btnNotch->show();
			btnNotch->redraw();
			sldrNOTCH->resize( 54, y, 368, 18 );
			sldrNOTCH->show();
			sldrNOTCH->redraw();
		}
		if (selrig->has_micgain_control) {
			y += 20;
			if (selrig->has_data_port) {
				sldrMICGAIN->label("");
				sldrMICGAIN->redraw_label();
				btnDataPort->position( 2, y);
				btnDataPort->show();
				btnDataPort->redraw();
			}
			sldrMICGAIN->resize( 54, y, 368, 18 );
			sldrMICGAIN->show();
			sldrMICGAIN->redraw();
		}
		if (selrig->has_power_control) {
			y += 20;
			sldrPOWER->resize( 54, y, 368, 18 );
			sldrPOWER->show();
			sldrPOWER->redraw();
			btnPOWER->resize( 2, y, 50, 18 );
			btnPOWER->show();
		}
	}
	y += 20;
	btn_show_controls->position( btn_show_controls->x(), y );
	btnAttenuator->position( btnAttenuator->x(), y);
	btnAttenuator->redraw();
	btnPreamp->position( btnPreamp->x(), y);
	btnPreamp->redraw();
	btnNOISE->position( btnNOISE->x(), y);
	btnNOISE->redraw();
	btnAutoNotch->position( btnAutoNotch->x(), y);
	btnAutoNotch->redraw();
	btnTune->position( btnTune->x(), y);
	btnTune->redraw();

	if (selrig->has_agc_control) {
		btnAGC->show();
		sldrRFGAIN->label("");
		sldrRFGAIN->redraw_label();
	} else {
		btnAGC->hide();
		sldrRFGAIN->label(_("RF"));
		sldrRFGAIN->redraw_label();
	}

	if (xcvr_name == rig_FT1000MP.name_) {
		y -= 20;
		btnTune->position( btnTune->x(), y);
		btnTune->redraw();
		btnAutoNotch->position( btnAutoNotch->x(), y);
		btnAutoNotch->redraw();
		btnPTT->position( btnPTT->x(), y);
		btnPTT->redraw();
	}

	if (xcvr_name == rig_FT100D.name_ ||
		xcvr_name == rig_FT767.name_  ||
		xcvr_name == rig_FT817.name_  ||
		xcvr_name == rig_FT847.name_  ||
		xcvr_name == rig_FT857D.name_ ||
		xcvr_name == rig_FT890.name_  ||
		xcvr_name == rig_FT897D.name_ ||
		xcvr_name == rig_FT920.name_ ) {
		y -= 20;
		btnPTT->position( btnPTT->x(), y);
		btnPTT->redraw();
	}

	int use_AuxPort = (progStatus.aux_serial_port != "NONE");
	if (use_AuxPort) {
		btnPTT->resize(btnPTT->x(), y, btnPTT->w(), 38);
		btnPTT->redraw();
		y += 20;
		boxControl->position(boxControl->x(), y);
		btnAuxRTS->position(btnAuxRTS->x(), y);
		btnAuxDTR->position(btnAuxDTR->x(), y);
		btnAuxRTS->value(progStatus.aux_rts);
		btnAuxDTR->value(progStatus.aux_dtr);
		boxControl->show();
		btnAuxRTS->show();
		btnAuxDTR->show();
	} else {
		boxControl->hide();
		btnAuxRTS->hide();
		btnAuxDTR->hide();
		btnPTT->resize(btnPTT->x(), y, btnPTT->w(), 18);
		btnPTT->redraw();
	}

	tabs550->resize(tabs550->x(), y + 20, tabs550->w(), tabs550->h());
	tabs550->hide();

	tabsGeneric->resize(tabsGeneric->x(), y + 20, tabsGeneric->w(), tabsGeneric->h());
	tabsGeneric->hide();

	btn_show_controls->label("@-22->");
	btn_show_controls->redraw_label();

	if (selrig->has_extras)
		btn_show_controls->show();
	else
		btn_show_controls->hide();

	y += 20;

	if (progStatus.tooltips) {
		Fl_Tooltip::enable(1);
		mnuTooltips->set();
	} else {
		mnuTooltips->clear();
		Fl_Tooltip::enable(0);
	}

	mainwindow->size( mainwindow->w(), y);
	mainwindow->init_sizes();
	mainwindow->redraw();
}

void adjust_wide_ui()
{
	mainwindow->resize( mainwindow->x(), mainwindow->y(), mainwindow->w(), 130);
	mainwindow->redraw();

	btnVol->show();
	sldrVOLUME->show();
	sldrRFGAIN->show();
	btnIFsh->show();
	sldrIFSHIFT->show();
	btnNotch->show();
	sldrNOTCH->show();
	sldrMICGAIN->show();
	sldrPOWER->show();
	btnPOWER->hide();
	sldrSQUELCH->show();
	btnNR->show();
	sldrNR->show();
	btnAGC->hide();
	sldrRFGAIN->redraw_label();

	if (xcvr_name == rig_TT550.name_) {
		tabs550->show();
		tabsGeneric->hide();
	} else {
		tabs550->hide();
		tabsGeneric->remove(genericAux);
		if (progStatus.aux_serial_port != "NONE" || selrig->has_data_port) {
			if (progStatus.aux_serial_port != "NONE") {
				btnAuxRTS->activate();
				btnAuxDTR->activate();
			} else {
				btnAuxRTS->deactivate();
				btnAuxDTR->deactivate();
			}
			if (selrig->has_data_port)
				btnDataPort->activate();
			else
				btnDataPort->deactivate();
			tabsGeneric->add(genericAux);
		}
		tabsGeneric->remove(genericRXB);
		if (selrig->has_rit || selrig->has_xit || selrig->has_bfo)
			tabsGeneric->add(genericRXB);
		tabsGeneric->show();
		if (selrig->has_agc_control) {
			btnAGC->show();
			sldrRFGAIN->label("");
			sldrRFGAIN->redraw_label();
		} else {
			btnAGC->hide();
			sldrRFGAIN->label(_("RF"));
			sldrRFGAIN->redraw_label();
		}
		if (selrig->has_power_control) {
			btnPOWER->resize(sldrPOWER->x() - 52, sldrPOWER->y(), 50, 18);
			btnPOWER->show();
		}
		else {
			sldrPOWER->deactivate();
		}
	}

	if (progStatus.tooltips) {
		Fl_Tooltip::enable(1);
		mnuTooltips->set();
	} else {
		mnuTooltips->clear();
		Fl_Tooltip::enable(0);
	}

	mainwindow->init_sizes();
	mainwindow->redraw();
}

void adjust_touch_ui()
{
	mainwindow->resize( mainwindow->x(), mainwindow->y(), mainwindow->w(), TOUCH_MAINH);
	mainwindow->redraw();

	if (spnrPOWER) spnrPOWER->show();
	if (sldrPOWER) sldrPOWER->show();
	btnVol->show();
	if (spnrVOLUME) spnrVOLUME->show();
	if (sldrVOLUME) sldrVOLUME->show();

	if (spnrRFGAIN) spnrRFGAIN->show();
	if (sldrRFGAIN) sldrRFGAIN->show();

	btnIFsh->show();
	if (spnrIFSHIFT) spnrIFSHIFT->show();
	if (sldrIFSHIFT) sldrIFSHIFT->show();

	btnNotch->show();
	if (spnrNOTCH) spnrNOTCH->show();
	if (sldrNOTCH) sldrNOTCH->show();

	if (spnrMICGAIN) spnrMICGAIN->show();
	if (sldrMICGAIN) sldrMICGAIN->show();

	if (spnrSQUELCH) spnrSQUELCH->show();
	if (sldrSQUELCH) sldrSQUELCH->show();

	if (selrig->has_agc_control) {
		btnAGC->show();
		sldrRFGAIN->label("");
		sldrRFGAIN->redraw_label();
	} else {
		btnAGC->hide();
		sldrRFGAIN->label(_("RF"));
		sldrRFGAIN->redraw_label();
	}

	btnNR->show();
	if (spnrNR) spnrNR->show();
	if (sldrNR) sldrNR->show();

	if (xcvr_name == rig_TT550.name_) {
		tabs550->show();
		tabsGeneric->hide();
	} else {
		tabs550->hide();
		tabsGeneric->remove(genericAux);
		if (progStatus.aux_serial_port != "NONE" || selrig->has_data_port) {
			if (progStatus.aux_serial_port != "NONE") {
				btnAuxRTS->activate();
				btnAuxDTR->activate();
			} else {
				btnAuxRTS->deactivate();
				btnAuxDTR->deactivate();
			}
			if (selrig->has_data_port)
				btnDataPort->activate();
			else
				btnDataPort->deactivate();
			tabsGeneric->add(genericAux);
		}
		tabsGeneric->remove(genericRXB);
		if (selrig->has_rit || selrig->has_xit || selrig->has_bfo)
			tabsGeneric->add(genericRXB);
		tabsGeneric->show();
	}

	if (progStatus.tooltips) {
		Fl_Tooltip::enable(1);
		mnuTooltips->set();
	} else {
		mnuTooltips->clear();
		Fl_Tooltip::enable(0);
	}

	mainwindow->init_sizes();
	mainwindow->redraw();
}

void adjust_control_positions()
{
	switch (progStatus.UIsize) {
		case small_ui :
			adjust_small_ui();
			break;
		case wide_ui :
			adjust_wide_ui();
			break;
		case touch_ui :
		default :
			adjust_touch_ui();
			break;
	}
	FreqDispA->set_hrd(progStatus.hrd_buttons);
	FreqDispB->set_hrd(progStatus.hrd_buttons);
}

void initXcvrTab()
{
	if (xcvr_name == rig_TT550.name_) {
		spnr_tt550_line_out->value(progStatus.tt550_line_out);
		cbo_tt550_agc_level->index(progStatus.tt550_agc_level);
		spnr_tt550_cw_wpm->value(progStatus.tt550_cw_wpm);
		spnr_tt550_cw_vol->value(progStatus.tt550_cw_vol);
		spnr_tt550_cw_spot->value(progStatus.tt550_cw_spot);
		spnr_tt550_cw_weight->value(progStatus.tt550_cw_weight);
		spnr_tt550_cw_qsk->value(progStatus.tt550_cw_qsk);
		btn_tt550_enable_keyer->value(progStatus.tt550_enable_keyer);
		btn_tt550_vox->value(progStatus.tt550_vox_onoff);
		spnr_tt550_vox_gain->value(progStatus.tt550_vox_gain);
		spnr_tt550_anti_vox->value(progStatus.tt550_vox_anti);
		spnr_tt550_vox_hang->value(progStatus.tt550_vox_hang);
		btn_tt550_CompON->value(progStatus.tt550_compON);
		spnr_tt550_compression->value(progStatus.tt550_compression);
		spnr_tt550_mon_vol->value(progStatus.tt550_mon_vol);
		btn_tt550_enable_xmtr->value(progStatus.tt550_enable_xmtr);
		btn_tt550_enable_tloop->value(progStatus.tt550_enable_tloop);
		btn_tt550_tuner_bypass->value(progStatus.tt550_tuner_bypass);
		btn_tt550_use_xmt_bw->value(progStatus.tt550_use_xmt_bw);
		sel_tt550_encoder_step->value(progStatus.tt550_encoder_step);
		spnr_tt550_encoder_sensitivity->value(progStatus.tt550_encoder_sensitivity);
		sel_tt550_F1_func->value(progStatus.tt550_F1_func);
		sel_tt550_F2_func->value(progStatus.tt550_F2_func);
		sel_tt550_F3_func->value(progStatus.tt550_F3_func);
		mnuRestoreData->clear();
		mnuRestoreData->hide();
		mnuKeepData->clear();
		mnuKeepData->hide();
		progStatus.restore_rig_data = false;
		progStatus.use_rig_data = false;
		op_tt550_XmtBW->clear();
		for (int i = 0; TT550_xmt_widths[i] != NULL; i++) {
			op_tt550_XmtBW->add(TT550_xmt_widths[i]);
		}
		op_tt550_XmtBW->activate();
		op_tt550_XmtBW->index(progStatus.tt550_xmt_bw);

		poll_smeter->activate(); poll_smeter->value(progStatus.poll_smeter);
		poll_pout->activate(); poll_pout->value(progStatus.poll_pout);
		poll_swr->activate(); poll_swr->value(progStatus.poll_swr);
		poll_alc->activate(); poll_alc->value(progStatus.poll_alc);
		poll_frequency->deactivate(); poll_frequency->value(0);
		poll_mode->deactivate(); poll_mode->value(0);
		poll_bandwidth->deactivate(); poll_bandwidth->value(0);
		poll_volume->deactivate(); poll_volume->value(0);
		poll_notch->deactivate(); poll_notch->value(0);
		poll_auto_notch->deactivate(); poll_auto_notch->value(0);
		poll_ifshift->deactivate(); poll_ifshift->value(0);
		poll_power_control->deactivate(); poll_power_control->value(0);
		poll_pre_att->deactivate(); poll_pre_att->value(0);
		poll_squelch->deactivate(); poll_squelch->value(0);
		poll_micgain->deactivate(); poll_micgain->value(0);
		poll_rfgain->deactivate(); poll_rfgain->value(0);
		poll_split->deactivate(); poll_split->value(0);
		poll_nr->deactivate(); poll_nr->value(0);
		poll_noise->deactivate(); poll_noise->value(0);
		poll_all->deactivate(); poll_all->value(0);

		if (progStatus.tt550_at11_inline) {
			tt550_AT_inline->value(1);
			tt550_AT_inline->label("Inline");
			tt550_AT_inline->redraw_label();
			selrig->at11_autotune();
		} else {
			tt550_AT_inline->value(0);
			tt550_AT_inline->label("Bypassed");
			tt550_AT_inline->redraw_label();
			selrig->at11_bypass();
		}
		if (progStatus.tt550_at11_hiZ) {
			selrig->at11_hiZ();
			tt550_AT_Z->value(1);
		} else{
			selrig->at11_loZ();
			tt550_AT_Z->value(0);
		}

	} else {

		tabsGeneric->remove(*genericBands);
		tabsGeneric->remove(*genericCW);
		tabsGeneric->remove(*genericVOX);
		tabsGeneric->remove(*genericSpeech);
		tabsGeneric->remove(*genericRx);
		tabsGeneric->remove(*genericMisc);
		tabsGeneric->remove(*genericUser);

		if (selrig->has_band_selection) {
			if (progStatus.UIsize != touch_ui)
			genericBands->resize(
				tabsGeneric->x() + 2,
				tabsGeneric->y() + 19,
				tabsGeneric->w() - 4,
				tabsGeneric->h() - 21);
			tabsGeneric->insert(*genericBands, 0);
		}

		poll_all->activate();
		poll_all->value(progStatus.poll_all);

		if (selrig->has_cw_wpm || selrig->has_cw_qsk || selrig->has_cw_weight ||
			selrig->has_cw_keyer || selrig->has_cw_spot || selrig->has_cw_spot_tone) {
			if (progStatus.UIsize == wide_ui)
			genericCW->resize(
				tabsGeneric->x() + 2,
				tabsGeneric->y() + 19,
				tabsGeneric->w() - 4,
				tabsGeneric->h() - 21);
			tabsGeneric->insert(*genericCW, 6);
			if (selrig->has_cw_wpm) {
				int min, max;
				selrig->get_cw_wpm_min_max(min, max);
				spnr_cw_wpm->minimum(min);
				spnr_cw_wpm->maximum(max);
				spnr_cw_wpm->value(progStatus.cw_wpm);
				spnr_cw_wpm->show();
				selrig->set_cw_wpm();
			} else spnr_cw_wpm->hide();
			if (selrig->has_cw_qsk) {
				double min, max, step;
				selrig->get_cw_qsk_min_max_step(min, max, step);
				spnr_cw_qsk->minimum(min);
				spnr_cw_qsk->maximum(max);
				spnr_cw_qsk->step(step);
				spnr_cw_qsk->value(progStatus.cw_qsk);
				spnr_cw_qsk->show();
				selrig->set_cw_qsk();
			} else spnr_cw_qsk->hide();
			if (selrig->has_cw_weight) {
				double min, max, step;
				selrig->get_cw_weight_min_max_step( min, max, step );
				spnr_cw_weight->minimum(min);
				spnr_cw_weight->maximum(max);
				spnr_cw_weight->step(step);
				spnr_cw_weight->value(progStatus.cw_weight);
				spnr_cw_weight->show();
				selrig->set_cw_weight();
			} else spnr_cw_weight->hide();
			if (selrig->has_cw_keyer) {
				btn_enable_keyer->show();
				btn_enable_keyer->value(progStatus.enable_keyer);
				selrig->enable_keyer();
			} else btn_enable_keyer->hide();
			if (selrig->has_cw_spot) {
				btnSpot->value(progStatus.cw_spot);
				selrig->set_cw_spot();
				btnSpot->show();
			} else btnSpot->hide();
			if (selrig->has_cw_spot_tone) {
				spnr_cw_spot_tone->show();
				int min, max, step;
				selrig->get_cw_spot_tone_min_max_step(min, max, step);
				spnr_cw_spot_tone->minimum(min);
				spnr_cw_spot_tone->maximum(max);
				spnr_cw_spot_tone->step(step);
				spnr_cw_spot_tone->value(progStatus.cw_spot_tone);
				selrig->set_cw_spot_tone();
			} else spnr_cw_spot_tone->hide();
		}

		if (selrig->has_vox_onoff || selrig->has_vox_gain || selrig->has_vox_anti ||
			selrig->has_vox_hang || selrig->has_vox_on_dataport || selrig->has_cw_spot_tone) {
			if (progStatus.UIsize == wide_ui)
			genericVOX->resize(
				tabsGeneric->x() + 2,
				tabsGeneric->y() + 19,
				tabsGeneric->w() - 4,
				tabsGeneric->h() - 21);
			tabsGeneric->insert(*genericVOX, 6);
			if (selrig->has_vox_onoff) {
				btn_vox->show();
				btn_vox->value(progStatus.vox_onoff);
				selrig->set_vox_onoff();
			} else btn_vox->hide();
			if (selrig->has_vox_gain) {
				spnr_vox_gain->show();
				int min, max, step;
				selrig->get_vox_gain_min_max_step(min, max, step);
				spnr_vox_gain->minimum(min);
				spnr_vox_gain->maximum(max);
				spnr_vox_gain->step(step);
				spnr_vox_gain->value(progStatus.vox_gain);
				selrig->set_vox_gain();
			} else spnr_vox_gain->hide();
			if (selrig->has_vox_anti) {
				spnr_anti_vox->show();
				int min, max, step;
				selrig->get_vox_anti_min_max_step(min, max, step);
				spnr_anti_vox->minimum(min);
				spnr_anti_vox->maximum(max);
				spnr_anti_vox->step(step);
				spnr_anti_vox->value(progStatus.vox_anti);
				selrig->set_vox_anti();
			} else spnr_anti_vox->hide();
			if (selrig->has_vox_hang) {
				spnr_vox_hang->show();
				int min, max, step;
				selrig->get_vox_hang_min_max_step(min, max, step);
				spnr_vox_hang->minimum(min);
				spnr_vox_hang->maximum(max);
				spnr_vox_hang->step(step);
				spnr_vox_hang->value(progStatus.vox_hang);
				selrig->set_vox_hang();
			} else spnr_vox_hang->hide();
			if (selrig->has_vox_on_dataport) {
				btn_vox_on_dataport->show();
				btn_vox_on_dataport->value(progStatus.vox_on_dataport);
				selrig->set_vox_on_dataport();
			} else btn_vox_on_dataport->hide();
		}

		if (selrig->has_compON || selrig->has_compression) {
			if (progStatus.UIsize == wide_ui)
			genericSpeech->resize(
				tabsGeneric->x() + 2,
				tabsGeneric->y() + 19,
				tabsGeneric->w() - 4,
				tabsGeneric->h() - 21);
			tabsGeneric->insert(*genericSpeech, 6);
			if (selrig->has_compON) {
				btnCompON->show();
				btnCompON->value(progStatus.compON);
			} else
				btnCompON->hide();
			if (selrig->has_compression) {
				int min, max, step;
				selrig->get_comp_min_max_step(min, max, step);
				spnr_compression->minimum(min);
				spnr_compression->maximum(max);
				spnr_compression->step(step);
				spnr_compression->show();
				spnr_compression->value(progStatus.compression);
				selrig->set_compression();
			} else
				spnr_compression->hide();
		}
		if (selrig->has_agc_level || selrig->has_nb_level || selrig->has_bpf_center) {
			if (progStatus.UIsize == wide_ui)
			genericRx->resize(
				tabsGeneric->x() + 2,
				tabsGeneric->y() + 19,
				tabsGeneric->w() - 4,
				tabsGeneric->h() - 21);
			tabsGeneric->insert(*genericRx, 6);
			if (selrig->has_agc_level) cbo_agc_level->activate(); else cbo_agc_level->deactivate();
			if (selrig->has_nb_level) cbo_nb_level->activate(); else cbo_nb_level->deactivate();
			if (selrig->has_bpf_center) {
				spnr_bpf_center->value(progStatus.bpf_center);
				spnr_bpf_center->show();
				btn_use_bpf_center->show();
			} else {
				spnr_bpf_center->hide();
				btn_use_bpf_center->hide();
			}
		}

		if (selrig->has_vfo_adj || selrig->has_xcvr_auto_on_off) {
			if (progStatus.UIsize == wide_ui)
			genericMisc->resize(
				tabsGeneric->x() + 2,
				tabsGeneric->y() + 19,
				tabsGeneric->w() - 4,
				tabsGeneric->h() - 21);
			tabsGeneric->insert(*genericMisc, 6);
			if (selrig->has_vfo_adj) {
				int min, max, step;
				selrig->get_vfoadj_min_max_step(min, max, step);
				spnr_vfo_adj->minimum(min);
				spnr_vfo_adj->maximum(max);
				spnr_vfo_adj->step(step);
				progStatus.vfo_adj = selrig->getVfoAdj();
				spnr_vfo_adj->value(progStatus.vfo_adj);
				spnr_vfo_adj->show();
			} else
				spnr_vfo_adj->hide();
			spnr_line_out->hide(); // enable if a lineout control is used by any transceiver

			if (selrig->has_xcvr_auto_on_off) {
				btn_xcvr_auto_on->value(progStatus.xcvr_auto_on);
				btn_xcvr_auto_off->value(progStatus.xcvr_auto_off);
				btn_xcvr_auto_on->show();
				btn_xcvr_auto_off->show();
			} else {
				btn_xcvr_auto_on->hide();
				btn_xcvr_auto_off->hide();
			}

		}
		tabsGeneric->insert(*genericUser, 6);
		if (progStatus.UIsize != touch_ui)
		genericUser->resize(
			tabsGeneric->x() + 2,
			tabsGeneric->y() + 19,
			tabsGeneric->w() - 4,
			tabsGeneric->h() - 21);

		tabsGeneric->redraw();

		if (progStatus.restore_rig_data) {
			mnuRestoreData->set();
		} else {
			mnuRestoreData->clear();
		}
		mnuRestoreData->show();

		if (progStatus.use_rig_data) {
			mnuKeepData->set();
		} else {
			mnuKeepData->clear();
		}
		mnuKeepData->show();

		poll_frequency->activate(); poll_frequency->value(progStatus.poll_frequency);
		poll_mode->activate(); poll_mode->value(progStatus.poll_mode);
		poll_bandwidth->activate(); poll_bandwidth->value(progStatus.poll_bandwidth);

		poll_smeter->activate(); poll_smeter->value(progStatus.poll_smeter);
		poll_pout->activate(); poll_pout->value(progStatus.poll_pout);
		poll_swr->activate(); poll_swr->value(progStatus.poll_swr);
		poll_alc->activate(); poll_alc->value(progStatus.poll_alc);
		poll_volume->activate(); poll_volume->value(progStatus.poll_volume);
		poll_notch->activate(); poll_notch->value(progStatus.poll_notch);
		poll_auto_notch->activate(); poll_auto_notch->value(progStatus.poll_auto_notch);
		poll_ifshift->activate(); poll_ifshift->value(progStatus.poll_ifshift);
		poll_power_control->activate(); poll_power_control->value(progStatus.poll_power_control);
		poll_pre_att->activate(); poll_pre_att->value(progStatus.poll_pre_att);
		poll_squelch->activate(); poll_squelch->value(progStatus.poll_squelch);
		poll_micgain->activate(); poll_micgain->value(progStatus.poll_micgain);
		poll_rfgain->activate(); poll_rfgain->value(progStatus.poll_rfgain);
		poll_split->activate(); poll_split->value(progStatus.poll_split);
		poll_noise->activate(); poll_noise->value(progStatus.poll_noise);
		poll_nr->activate(); poll_nr->value(progStatus.poll_nr);

		if (!selrig->has_bandwidth_control) { poll_bandwidth->deactivate(); poll_bandwidth->value(0); }
		if (!selrig->has_smeter) { poll_smeter->deactivate(); poll_smeter->value(0); }
		if (!selrig->has_power_out) { poll_pout->deactivate(); poll_pout->value(0); }
		if (!selrig->has_swr_control) { poll_swr->deactivate(); poll_swr->value(0); }
		if (!selrig->has_alc_control) { poll_alc->deactivate(); poll_alc->value(0); }
		if (!selrig->has_volume_control) { poll_volume->deactivate(); poll_volume->value(0); }
		if (!selrig->has_notch_control) { poll_notch->deactivate(); poll_notch->value(0); }
		if (!selrig->has_auto_notch ||
			xcvr_name == rig_FT1000MP.name_ )
			{ poll_auto_notch->deactivate(); poll_auto_notch->value(0); }
		if (!selrig->has_ifshift_control) { poll_ifshift->deactivate(); poll_ifshift->value(0); }
		if (!selrig->has_power_control) { poll_power_control->deactivate(); poll_power_control->value(0); }
		if (!selrig->has_preamp_control && !selrig->has_attenuator_control)
			{ poll_pre_att->deactivate(); poll_pre_att->value(0); }
		if (!selrig->has_sql_control) { poll_squelch->deactivate(); poll_squelch->value(0); }
		if (!selrig->has_micgain_control) { poll_micgain->deactivate(); poll_micgain->value(0); }
		if (!selrig->has_rf_control) { poll_rfgain->deactivate(); poll_rfgain->value(0); }
		if (!selrig->has_split) { poll_split->deactivate(); poll_split->value(0); }
		if (!selrig->has_noise_control) {poll_noise->deactivate(); poll_noise->value(0);}
		if (!selrig->has_noise_reduction) {poll_nr->deactivate(); poll_nr->value(0);}

	}

}

void init_TT550()
{
	selrig->selectA();

	vfoB.freq = progStatus.freq_B;
	vfoB.imode = progStatus.imode_B;
	vfoB.iBW = progStatus.iBW_B;
	FreqDispB->value(vfoB.freq);
	selrig->set_vfoB(vfoB.freq);
	selrig->set_modeB(vfoB.imode);
	selrig->set_bwB(vfoB.iBW);

	vfo.freq = vfoA.freq = progStatus.freq_A;
	vfo.imode = vfoA.imode = progStatus.imode_A;
	vfo.iBW = vfoA.iBW = progStatus.iBW_A;
	FreqDispA->value( vfoA.freq );
	selrig->set_vfoA(vfoA.freq);
	selrig->set_modeA(vfoA.imode);

	if (vfoA.iBW == -1) vfoA.iBW = selrig->def_bandwidth(vfoA.imode);
		selrig->set_bwA(vfoA.iBW);

	rigmodes_.clear();
	opMODE->clear();
	for (int i = 0; selrig->modes_[i] != NULL; i++) {
		rigmodes_.push_back(selrig->modes_[i]);
		opMODE->add(selrig->modes_[i]);
	}
	opMODE->activate();
	opMODE->index(vfoA.imode);

	rigbws_.clear();
	opBW->show();
	opBW->clear();
	old_bws = selrig->bandwidths_;
	for (int i = 0; selrig->bandwidths_[i] != NULL; i++) {
		rigbws_.push_back(selrig->bandwidths_[i]);
			opBW->add(selrig->bandwidths_[i]);
		}
	opBW->activate();
	opBW->index(vfoA.iBW);

	spnr_tt550_vfo_adj->value(progStatus.vfo_adj);

	btnPreamp->label("Spot");
	btnPreamp->value(progStatus.tt550_spot_onoff);
	switch (progStatus.UIsize) {
		case small_ui :
			btnPreamp->show();
			break;
		case wide_ui : case touch_ui : default :
			btnPreamp->activate();
	}
}

void init_generic_rig()
{
	if (progStatus.CIV > 0)
		selrig->adjustCIV(progStatus.CIV);

	if (selrig->has_getvfoAorB) {


		int ret = selrig->get_vfoAorB();
		int retry = 10;
		while (ret == -1 && retry--) {
			MilliSleep(50);
			ret = selrig->get_vfoAorB();
		}
		if (ret == -1) ret = 0;

		useB = ret;

		selrig->selectB();
		transceiverB.freq = vfoB.freq = progStatus.freq_B = selrig->get_vfoB();
		transceiverB.imode = vfoB.imode = progStatus.imode_B = selrig->get_modeB();
		transceiverB.iBW = vfoB.iBW = progStatus.iBW_B = selrig->get_bwA();

		selrig->selectA();
		transceiverA.freq = vfoA.freq = progStatus.freq_A = selrig->get_vfoA();
		transceiverA.imode = vfoA.imode = progStatus.imode_B = selrig->get_modeA();
		transceiverA.iBW = vfoA.iBW = progStatus.iBW_B =selrig->get_bwA();

		if (useB) {
			selrig->selectB();
			vfo = vfoB;
		} else {
			vfo = vfoA;
		}
	}
	else {
		selrig->selectA();
		if (selrig->has_get_info)
			selrig->get_info();
		transceiverA.freq = selrig->get_vfoA();
		transceiverA.imode = selrig->get_modeA();
		transceiverA.iBW = selrig->get_bwA();

		if (selrig->has_vfoAB) {
			selrig->selectB();
			if (selrig->has_get_info)
				selrig->get_info();
			transceiverB.freq = selrig->get_vfoB();
			transceiverB.imode = selrig->get_modeB();
			transceiverB.iBW = selrig->get_bwB();
		} else {
			transceiverB.freq = vfoB.freq = progStatus.freq_B;
			transceiverB.imode = vfoB.imode = progStatus.imode_B;
			transceiverB.iBW = vfoB.iBW = progStatus.iBW_B;
		}
	}

	if (progStatus.use_rig_data && (xcvr_name != rig_FT817.name_)) {
LOG_INFO("Use xcvr start values for Vfo A/B");
		vfo.freq = vfoA.freq = progStatus.freq_A = transceiverA.freq;
		vfo.imode = vfoA.imode = progStatus.imode_A = transceiverA.imode;
		vfo.iBW = vfoA.imode = progStatus.iBW_A = transceiverA.iBW;
		vfoB.freq = progStatus.freq_B = transceiverB.freq;
		vfoB.imode = progStatus.imode_B = transceiverB.imode;
		vfoB.iBW = progStatus.iBW_B = transceiverB.iBW;
	}

	rigmodes_.clear();
	opMODE->clear();
	if (selrig->has_mode_control) {
		for (int i = 0; selrig->modes_[i] != NULL; i++) {
			rigmodes_.push_back(selrig->modes_[i]);
			opMODE->add(selrig->modes_[i]);
		}
		opMODE->activate();
		opMODE->index(progStatus.imode_A);
	} else {
		opMODE->add(" ");
		opMODE->index(0);
		opMODE->deactivate();
	}

	rigbws_.clear();
	opBW->show();
	opBW->clear();
	if (selrig->has_bandwidth_control) {
		old_bws = selrig->bandwidths_;
		for (int i = 0; selrig->bandwidths_[i] != NULL; i++) {
			rigbws_.push_back(selrig->bandwidths_[i]);
			opBW->add(selrig->bandwidths_[i]);
		}
		opBW->activate();
		if (progStatus.iBW_A == -1) progStatus.iBW_A = selrig->def_bandwidth(vfoA.imode);
		if (progStatus.iBW_B == -1) progStatus.iBW_B = selrig->def_bandwidth(vfoB.imode);
			opBW->index(progStatus.iBW_A);
	} else {
		opBW->add(" ");
		opBW->index(0);
		opBW->deactivate();
	}
}

void init_rit()
{
	if (selrig->has_rit) {
		int min, max, step;
		selrig->get_RIT_min_max_step(min, max, step);
		cntRIT->minimum(min);
		cntRIT->maximum(max);
		cntRIT->step(step);
		switch (progStatus.UIsize) {
			case small_ui :
				cntRIT->show();
				break;
			case wide_ui : case touch_ui : default :
				cntRIT->activate();
		}
		cntRIT->value(progStatus.rit_freq);
	} else {
		switch (progStatus.UIsize) {
			case small_ui :
				cntRIT->hide();
				break;
			case wide_ui: case touch_ui : default :
				cntRIT->deactivate();
		}
	}
}

void init_xit()
{
	if (selrig->has_xit) {
		int min, max, step;
		selrig->get_XIT_min_max_step(min, max, step);
		cntXIT->minimum(min);
		cntXIT->maximum(max);
		cntXIT->step(step);
		cntXIT->value(progStatus.xit_freq);
		switch (progStatus.UIsize) {
			case small_ui :
				cntXIT->show();
				break;
			case wide_ui : case touch_ui : default :
				cntXIT->activate();
			}
	} else {
		switch (progStatus.UIsize) {
			case small_ui :
				cntXIT->hide();
			case wide_ui : case touch_ui : default :
				cntXIT->deactivate();
		}
	}
}

void init_bfo()
{
	if (selrig->has_bfo) {
		int min, max, step;
		selrig->get_BFO_min_max_step(min, max, step);
		cntBFO->minimum(min);
		cntBFO->maximum(max);
		cntBFO->step(step);
		cntBFO->value(progStatus.bfo_freq);
		switch (progStatus.UIsize) {
			case small_ui :
				cntBFO->show();
				break;
			case wide_ui : case touch_ui : default :
				cntBFO->activate();
		}
	} else {
		switch (progStatus.UIsize) {
			case small_ui :
				cntBFO->hide();
				break;
			case wide_ui : case touch_ui : default :
				cntBFO->deactivate();
		}
	}
}

void init_dsp_controls()
{
	if (selrig->has_dsp_controls) {
		opDSP_lo->clear();
		opDSP_hi->clear();
		btnDSP->label(selrig->SL_label);
		btnDSP->redraw_label();
		for (int i = 0; selrig->dsp_SL[i] != NULL; i++)
			opDSP_lo->add(selrig->dsp_SL[i]);
		opDSP_lo->tooltip(selrig->SL_tooltip);
		for (int i = 0; selrig->dsp_SH[i] != NULL; i++)
			opDSP_hi->add(selrig->dsp_SH[i]);
		opDSP_hi->tooltip(selrig->SH_tooltip);
		if (vfo.iBW > 256) {
			opDSP_lo->index(vfo.iBW & 0xFF);
			opDSP_hi->index((vfo.iBW >> 8) & 0x7F);
			btnDSP->show();
			opDSP_hi->show();
			opDSP_lo->hide();
			opBW->hide();
			opBW->index(0);
		} else {
			opDSP_lo->index(0);
			opDSP_hi->index(0);
			btnDSP->hide();
			opDSP_lo->hide();
			opDSP_hi->hide();
			opBW->show();
		}
	} else {
		btnDSP->hide();
		opDSP_lo->hide();
		opDSP_hi->hide();
		opBW->show();
	}
}

void init_volume_control()
{
	if (selrig->has_volume_control) {
		int min, max, step;
		selrig->get_vol_min_max_step(min, max, step);
		if (spnrVOLUME) {
			spnrVOLUME->minimum(min);
			spnrVOLUME->maximum(max);
			spnrVOLUME->step(step);
			spnrVOLUME->redraw();
		}
		if (sldrVOLUME) {
			sldrVOLUME->minimum(min);
			sldrVOLUME->maximum(max);
			sldrVOLUME->step(step);
			sldrVOLUME->redraw();
		}
		if (progStatus.use_rig_data) {
			progStatus.volume = selrig->get_volume_control();
			if (sldrVOLUME) sldrVOLUME->value(progStatus.volume);
			if (sldrVOLUME) sldrVOLUME->activate();
			btnVol->value(1);
			if (spnrVOLUME) spnrVOLUME->value(progStatus.volume);
			if (spnrVOLUME) spnrVOLUME->activate();
			sldrVOLUME->activate();
		} else {
			if (sldrVOLUME) sldrVOLUME->value(progStatus.volume);
			if (spnrVOLUME) spnrVOLUME->value(progStatus.volume);
			if (progStatus.spkr_on == 0) {
				btnVol->value(0);
				if (sldrVOLUME) sldrVOLUME->deactivate();
				if (spnrVOLUME) spnrVOLUME->deactivate();
				selrig->set_volume_control(0);
			} else {
				btnVol->value(1);
				if (sldrVOLUME) sldrVOLUME->activate();
				if (spnrVOLUME) spnrVOLUME->activate();
				selrig->set_volume_control(progStatus.volume);
			}
		}
		switch (progStatus.UIsize) {
			case small_ui :
				btnVol->show();
				if (sldrVOLUME) sldrVOLUME->show();
				if (spnrVOLUME) spnrVOLUME->show();
				break;
			case wide_ui : case touch_ui : default :
				btnVol->activate();
				if (sldrVOLUME) sldrVOLUME->activate();
				if (spnrVOLUME) spnrVOLUME->activate();
		}
	} else {
		switch (progStatus.UIsize) {
			case small_ui :
				btnVol->hide();
				if (sldrVOLUME) sldrVOLUME->hide();
				if (spnrVOLUME) spnrVOLUME->hide();
				break;
			case wide_ui : case touch_ui : default :
				btnVol->deactivate();
				if (sldrVOLUME) sldrVOLUME->deactivate();
				if (spnrVOLUME) spnrVOLUME->deactivate();
		}
	}
}

void init_rf_control()
{
	if (selrig->has_rf_control) {
		int min, max, step;
		selrig->get_rf_min_max_step(min, max, step);
		if (sldrRFGAIN) sldrRFGAIN->minimum(min);
		if (sldrRFGAIN) sldrRFGAIN->maximum(max);
		if (sldrRFGAIN) sldrRFGAIN->step(step);
		if (sldrRFGAIN) sldrRFGAIN->redraw();
		if (spnrRFGAIN) spnrRFGAIN->minimum(min);
		if (spnrRFGAIN) spnrRFGAIN->maximum(max);
		if (spnrRFGAIN) spnrRFGAIN->step(step);
		if (spnrRFGAIN) spnrRFGAIN->redraw();
		if (progStatus.use_rig_data) {
			progStatus.rfgain = selrig->get_rf_gain();
			if (sldrRFGAIN) sldrRFGAIN->value(progStatus.rfgain);
			if (spnrRFGAIN) spnrRFGAIN->value(progStatus.rfgain);
			switch (progStatus.UIsize) {
				case small_ui :
					if (sldrRFGAIN) sldrRFGAIN->show();
					if (spnrRFGAIN) spnrRFGAIN->show();
					break;
				case wide_ui : case touch_ui : default :
					if (sldrRFGAIN) sldrRFGAIN->activate();
					if (spnrRFGAIN) spnrRFGAIN->activate();
			}
		} else {
			if (sldrRFGAIN) sldrRFGAIN->value(progStatus.rfgain);
			if (spnrRFGAIN) spnrRFGAIN->value(progStatus.rfgain);
			selrig->set_rf_gain(progStatus.rfgain);
			switch (progStatus.UIsize) {
				case small_ui :
					if (sldrRFGAIN) sldrRFGAIN->show();
					if (spnrRFGAIN) spnrRFGAIN->show();
					break;
				case wide_ui : case touch_ui : default :
					if (sldrRFGAIN) sldrRFGAIN->activate();
					if (spnrRFGAIN) spnrRFGAIN->activate();
			}
		}
	} else {
		switch (progStatus.UIsize) {
			case small_ui :
				if (sldrRFGAIN) sldrRFGAIN->hide();
				if (spnrRFGAIN) spnrRFGAIN->hide();
				break;
			case wide_ui : case touch_ui : default :
				if (sldrRFGAIN) sldrRFGAIN->deactivate();
				if (spnrRFGAIN) spnrRFGAIN->deactivate();
		}
	}
}

void init_sql_control()
{
	if (selrig->has_sql_control) {
		int min, max, step;
		selrig->get_squelch_min_max_step(min, max, step);
		if (sldrSQUELCH) sldrSQUELCH->minimum(min);
		if (sldrSQUELCH) sldrSQUELCH->maximum(max);
		if (sldrSQUELCH) sldrSQUELCH->step(step);
		if (sldrSQUELCH) sldrSQUELCH->redraw();
		if (spnrSQUELCH) spnrSQUELCH->minimum(min);
		if (spnrSQUELCH) spnrSQUELCH->maximum(max);
		if (spnrSQUELCH) spnrSQUELCH->step(step);
		if (spnrSQUELCH) spnrSQUELCH->redraw();
		if (progStatus.use_rig_data) {
			progStatus.squelch = selrig->get_squelch();
			if (sldrSQUELCH) sldrSQUELCH->value(progStatus.squelch);
			if (spnrSQUELCH) spnrSQUELCH->value(progStatus.squelch);
			switch (progStatus.UIsize) {
				case small_ui :
					if (sldrSQUELCH) sldrSQUELCH->show();
					if (spnrSQUELCH) spnrSQUELCH->show();
					break;
				case wide_ui : case touch_ui : default:
					if (sldrSQUELCH) sldrSQUELCH->activate();
					if (spnrSQUELCH) spnrSQUELCH->activate();
			}
		} else {
			if (sldrSQUELCH) sldrSQUELCH->value(progStatus.squelch);
			if (spnrSQUELCH) spnrSQUELCH->value(progStatus.squelch);
			selrig->set_squelch(progStatus.squelch);
			switch (progStatus.UIsize) {
				case small_ui :
					if (sldrSQUELCH) sldrSQUELCH->show();
					if (spnrSQUELCH) spnrSQUELCH->show();
					break;
				case wide_ui : case touch_ui : default :
					if (sldrSQUELCH) sldrSQUELCH->activate();
					if (spnrSQUELCH) spnrSQUELCH->activate();
			}
		}
	} else {
		switch (progStatus.UIsize) {
			case small_ui :
				if (sldrSQUELCH) sldrSQUELCH->hide();
				if (spnrSQUELCH) spnrSQUELCH->hide();
				break;
			case wide_ui : case touch_ui : default :
				if (sldrSQUELCH) sldrSQUELCH->deactivate();
				if (spnrSQUELCH) spnrSQUELCH->deactivate();
		}
	}
}

void init_noise_reduction_control()
{
	if (selrig->has_noise_reduction_control) {
		int min, max, step;
		selrig->get_nr_min_max_step(min, max, step);
		if (sldrNR) sldrNR->minimum(min);
		if (sldrNR) sldrNR->maximum(max);
		if (sldrNR) sldrNR->step(step);
		if (sldrNR) sldrNR->redraw();
		if (spnrNR) spnrNR->minimum(min);
		if (spnrNR) spnrNR->maximum(max);
		if (spnrNR) spnrNR->step(step);
		if (spnrNR) spnrNR->redraw();
		switch (progStatus.UIsize) {
			case small_ui :
				btnNR->show();
				if (sldrNR) sldrNR->show();
				if (spnrNR) spnrNR->show();
				break;
			case wide_ui : case touch_ui : default:
				btnNR->activate();
				if (sldrNR) sldrNR->activate();
				if (spnrNR) spnrNR->activate();
				break;
		}
		if (progStatus.use_rig_data) {
			progStatus.noise_reduction = selrig->get_noise_reduction();
			progStatus.noise_reduction_val = selrig->get_noise_reduction_val();
			btnNR->value(progStatus.noise_reduction);
			if (sldrNR) sldrNR->value(progStatus.noise_reduction_val);
			if (spnrNR) spnrNR->value(progStatus.noise_reduction_val);
		} else {
			btnNR->value(progStatus.noise_reduction);
			if (sldrNR) sldrNR->value(progStatus.noise_reduction_val);
			if (spnrNR) spnrNR->value(progStatus.noise_reduction_val);
			selrig->set_noise_reduction(progStatus.noise_reduction);
			selrig->set_noise_reduction_val(progStatus.noise_reduction_val);
		}
	} else {
		switch (progStatus.UIsize) {
			case small_ui :
				btnNR->hide();
				if (sldrNR) sldrNR->hide();
				if (spnrNR) sldrNR->hide();
				break;
			case wide_ui : case touch_ui : default :
				btnNR->deactivate();
				if (sldrNR) sldrNR->deactivate();
				if (spnrNR) spnrNR->deactivate();
				break;
		}
	}
}

void init_if_shift_control()
{
	if (selrig->has_ifshift_control) {

		int min, max, step;
		selrig->get_if_min_max_step(min, max, step);
		if (sldrIFSHIFT) sldrIFSHIFT->minimum(min);
		if (sldrIFSHIFT) sldrIFSHIFT->maximum(max);
		if (sldrIFSHIFT) sldrIFSHIFT->step(step);
		if (sldrIFSHIFT) sldrIFSHIFT->redraw();
		if (spnrIFSHIFT) spnrIFSHIFT->minimum(min);
		if (spnrIFSHIFT) spnrIFSHIFT->maximum(max);
		if (spnrIFSHIFT) spnrIFSHIFT->step(step);
		if (spnrIFSHIFT) spnrIFSHIFT->redraw();
		if (progStatus.use_rig_data) {
			progStatus.shift = selrig->get_if_shift(progStatus.shift_val);
			btnIFsh->value(progStatus.shift);
			if (sldrIFSHIFT) sldrIFSHIFT->value(progStatus.shift_val);
			if (spnrIFSHIFT) spnrIFSHIFT->value(progStatus.shift_val);
		} else {
			if (progStatus.shift) {
				btnIFsh->value(1);
				if (sldrIFSHIFT) sldrIFSHIFT->value(progStatus.shift_val);
				if (spnrIFSHIFT) spnrIFSHIFT->value(progStatus.shift_val);
				selrig->set_if_shift(progStatus.shift_val);
			} else {
				btnIFsh->value(0);
				if (sldrIFSHIFT) sldrIFSHIFT->value(selrig->if_shift_mid);
				if (spnrIFSHIFT) spnrIFSHIFT->value(selrig->if_shift_mid);
				selrig->set_if_shift(selrig->if_shift_mid);
			}
		}
		switch (progStatus.UIsize) {
			case small_ui :
				btnIFsh->show();
				if (sldrIFSHIFT) sldrIFSHIFT->show();
				if (spnrIFSHIFT) spnrIFSHIFT->show();
				break;
			case wide_ui : case touch_ui : default :
				btnIFsh->activate();
				if (sldrIFSHIFT) sldrIFSHIFT->activate();
				if (spnrIFSHIFT) spnrIFSHIFT->activate();
				break;
		}
	} else {
		switch (progStatus.UIsize) {
			case small_ui :
				btnIFsh->hide();
				if (sldrIFSHIFT) sldrIFSHIFT->hide();
				if (spnrIFSHIFT) spnrIFSHIFT->hide();
				break;
			case wide_ui : case touch_ui : default :
				btnIFsh->deactivate();
				if (sldrIFSHIFT) sldrIFSHIFT->deactivate();
				if (spnrIFSHIFT) spnrIFSHIFT->deactivate();
				break;
		}
	}
	if (xcvr_name == rig_TS870S.name_) {
		if (progStatus.imode_A == RIG_TS870S::tsCW ||
			progStatus.imode_A == RIG_TS870S::tsCWR) {
			btnIFsh->activate();
			if (sldrIFSHIFT) sldrIFSHIFT->activate();
			if (spnrIFSHIFT) spnrIFSHIFT->activate();
		} else {
			btnIFsh->deactivate();
			if (sldrIFSHIFT) sldrIFSHIFT->deactivate();
			if (spnrIFSHIFT) spnrIFSHIFT->deactivate();
		}
	}
}

void init_notch_control()
{
	if (selrig->has_notch_control) {
		int min, max, step;
		selrig->get_notch_min_max_step(min, max, step);
		if (sldrNOTCH) sldrNOTCH->minimum(min);
		if (sldrNOTCH) sldrNOTCH->maximum(max);
		if (sldrNOTCH) sldrNOTCH->step(step);
		if (sldrNOTCH) sldrNOTCH->redraw();
		if (spnrNOTCH) spnrNOTCH->minimum(min);
		if (spnrNOTCH) spnrNOTCH->maximum(max);
		if (spnrNOTCH) spnrNOTCH->step(step);
		if (spnrNOTCH) spnrNOTCH->redraw();
		if (progStatus.use_rig_data) {
			progStatus.notch = selrig->get_notch(progStatus.notch_val);
			btnNotch->value(progStatus.notch);
			if (sldrNOTCH) sldrNOTCH->value(progStatus.notch_val);
			if (spnrNOTCH) spnrNOTCH->value(progStatus.notch_val);
		} else {
			btnNotch->value(progStatus.notch);
			if (sldrNOTCH) sldrNOTCH->value(progStatus.notch_val);
			if (spnrNOTCH) spnrNOTCH->value(progStatus.notch_val);
			selrig->set_notch(progStatus.notch, progStatus.notch_val);
		}
		switch (progStatus.UIsize) {
			case small_ui :
				btnNotch->show();
				if (sldrNOTCH) sldrNOTCH->show();
				if (spnrNOTCH) spnrNOTCH->show();
				break;
			case wide_ui : case touch_ui : default :
				btnNotch->activate();
				if (sldrNOTCH) sldrNOTCH->activate();
				if (spnrNOTCH) spnrNOTCH->activate();
				break;
		}
	} else {
		switch (progStatus.UIsize) {
			case small_ui :
				btnNotch->hide();
				if (sldrNOTCH) sldrNOTCH->hide();
				if (spnrNOTCH) spnrNOTCH->hide();
				break;
			case wide_ui : case touch_ui : default :
				btnNotch->deactivate();
				if (sldrNOTCH) sldrNOTCH->deactivate();
				if (spnrNOTCH) spnrNOTCH->deactivate();
				break;
		}
	}
}

void init_micgain_control()
{
	if (selrig->has_micgain_control) {
		int min = 0, max = 0, step = 0;
		selrig->get_mic_min_max_step(min, max, step);
		if (sldrMICGAIN) sldrMICGAIN->minimum(min);
		if (sldrMICGAIN) sldrMICGAIN->maximum(max);
		if (sldrMICGAIN) sldrMICGAIN->step(step);
		if (spnrMICGAIN) spnrMICGAIN->minimum(min);
		if (spnrMICGAIN) spnrMICGAIN->maximum(max);
		if (spnrMICGAIN) spnrMICGAIN->step(step);
		if (progStatus.use_rig_data)
			progStatus.mic_gain = selrig->get_mic_gain();
		else
			selrig->set_mic_gain(progStatus.mic_gain);
		if (sldrMICGAIN) sldrMICGAIN->value(progStatus.mic_gain);
		if (spnrMICGAIN) spnrMICGAIN->value(progStatus.mic_gain);
		switch (progStatus.UIsize) {
			case small_ui :
				if (sldrMICGAIN) sldrMICGAIN->show();
				if (spnrMICGAIN) spnrMICGAIN->show();
				break;
			case wide_ui : case touch_ui : default :
				if (sldrMICGAIN) sldrMICGAIN->activate();
				if (spnrMICGAIN) spnrMICGAIN->activate();
				break;
		}
		if (selrig->has_data_port) {
			btnDataPort->show();
			btnDataPort->value(progStatus.data_port);
			btnDataPort->label(progStatus.data_port ? "Data" : "Mic");
			btnDataPort->redraw_label();
		} else btnDataPort->hide();
	} else {
		switch (progStatus.UIsize) {
			case small_ui :
				if (sldrMICGAIN) sldrMICGAIN->hide();
				if (spnrMICGAIN) spnrMICGAIN->hide();
				break;
			case wide_ui : case touch_ui : default :
				if (sldrMICGAIN) sldrMICGAIN->deactivate();
				if (spnrMICGAIN) spnrMICGAIN->deactivate();
		}
	}
}

void init_power_control()
{
	double min, max, step;
	if (selrig->has_power_control) {
		sldrPOWER->activate();
		if (progStatus.use_rig_data)
			progStatus.power_level = selrig->get_power_control();
		else
			selrig->set_power_control(progStatus.power_level);
		selrig->get_pc_min_max_step(min, max, step);
		if (sldrPOWER) sldrPOWER->minimum(min);
		if (sldrPOWER) sldrPOWER->maximum(max);
		if (sldrPOWER) sldrPOWER->step(step);
		if (sldrPOWER) sldrPOWER->value(progStatus.power_level);
		if (sldrPOWER) sldrPOWER->show();
		if (sldrPOWER) sldrPOWER->redraw();

		if (spnrPOWER) spnrPOWER->minimum(min);
		if (spnrPOWER) spnrPOWER->maximum(max);
		if (spnrPOWER) spnrPOWER->step(step);
		if (spnrPOWER) spnrPOWER->value(progStatus.power_level);
		if (spnrPOWER) spnrPOWER->show();
		if (spnrPOWER) spnrPOWER->redraw();
	} else
		sldrPOWER->deactivate();

	set_power_controlImage(progStatus.power_level);
}

void init_attenuator_control()
{
	if (selrig->has_attenuator_control) {
//		if (progStatus.use_rig_data)
			progStatus.attenuator = selrig->get_attenuator();
//		else
//			selrig->set_attenuator(progStatus.attenuator);
		switch (progStatus.UIsize) {
			case small_ui :
				btnAttenuator->show();
				break;
			case wide_ui : case touch_ui : default :
				btnAttenuator->activate();
		}
	} else {
		switch (progStatus.UIsize) {
			case small_ui :
				btnAttenuator->hide();
				break;
			case wide_ui : case touch_ui : default :
				btnAttenuator->deactivate();
		}
	}
}

void init_agc_control()
{
	if (selrig->has_agc_control) {
		btnAGC->show();
		sldrRFGAIN->label("");
		sldrRFGAIN->redraw_label();
	} else {
		btnAGC->hide();
		sldrRFGAIN->label(_("RF"));
		sldrRFGAIN->redraw_label();
	}
}

void init_preamp_control()
{
	if (selrig->has_preamp_control) {
		progStatus.preamp = selrig->get_preamp();
	switch (progStatus.UIsize) {
			case small_ui :
				btnPreamp->show();
				break;
			case wide_ui : case touch_ui : default :
			btnPreamp->activate();
		}
	} else {
		switch (progStatus.UIsize) {
			case small_ui :
				btnPreamp->hide();
				break;
			case wide_ui : case touch_ui : default :
				btnPreamp->deactivate();
		}
	}
}

void init_noise_control()
{
	if (selrig->has_noise_control) {
		if (xcvr_name == rig_TS990.name_) {
			btnNOISE->label("AGC"); //Set TS990 AGC Label
			btnNR->label("NR1"); //Set TS990 NR Button
		}
		if (progStatus.use_rig_data)
			progStatus.noise = selrig->get_noise();
		else
			selrig->set_noise(progStatus.noise);
		btnNOISE->value(progStatus.noise);
		switch (progStatus.UIsize) {
			case small_ui :
				btnNOISE->show();
				break;
			case wide_ui : case touch_ui : default :
				btnNOISE->activate();
		}
	}
	else {
		switch (progStatus.UIsize) {
			case small_ui :
				btnNOISE->hide();
				break;
			case wide_ui : case touch_ui : default :
				btnNOISE->deactivate();
		}
	}
}

void init_tune_control()
{
	if (selrig->has_tune_control) {
		switch (progStatus.UIsize) {
			case small_ui :
				btnTune->show();
				break;
			case wide_ui : case touch_ui : default :
				btnTune->activate();
		}
	} else {
		switch (progStatus.UIsize) {
			case small_ui :
				btnTune->hide();
				break;
			case wide_ui : case touch_ui : default :
				btnTune->deactivate();
		}
	}
}

void init_ptt_control()
{
	if (selrig->has_ptt_control ||
		progStatus.comm_dtrptt || progStatus.comm_rtsptt ||
		progStatus.sep_dtrptt || progStatus.sep_rtsptt) {
		btnPTT->activate();
	} else {
		btnPTT->deactivate();
	}
}

void init_auto_notch()
{
	if (selrig->has_auto_notch) {

		if (xcvr_name == rig_RAY152.name_) {
			btnAutoNotch->label("AGC");
			btnAutoNotch->tooltip("AGC on/off");
		} else if (xcvr_name == rig_FT1000MP.name_) {
			btnAutoNotch->label("Tuner");
			btnAutoNotch->tooltip("Tuner on/off");
		} else {
			btnAutoNotch->label("AN");
			btnAutoNotch->tooltip("Auto notch on/off");
		}
		if (progStatus.use_rig_data)
			progStatus.auto_notch = selrig->get_auto_notch();
		else
			selrig->set_auto_notch(progStatus.auto_notch);
		btnAutoNotch->value(progStatus.auto_notch);
		switch (progStatus.UIsize) {
			case small_ui :
				btnAutoNotch->show();
				break;
			case wide_ui : case touch_ui : default :
				btnAutoNotch->activate();
		}
	} else {
		switch (progStatus.UIsize) {
			case small_ui :
				btnAutoNotch->hide();
				break;
			case wide_ui : case touch_ui : default :
				btnAutoNotch->deactivate();
		}
	}
}

void init_swr_control()
{
	if (selrig->has_swr_control)
		btnALC_SWR->activate();
	else {
		btnALC_SWR->deactivate();
	}
}

void init_compression_control()
{
	if (selrig->has_compON || selrig->has_compression)
		selrig->set_compression();
}

void init_special_controls()
{
	if (selrig->has_special)
		btnSpecial->show();
	else
		btnSpecial->hide();
}

void init_external_tuner()
{
	if (selrig->has_ext_tuner)
		btn_ext_tuner->show();
	else
		btn_ext_tuner->hide();
}

void init_CIV()
{
	if (selrig->CIV) {
		char hexstr[8];
		snprintf(hexstr, sizeof(hexstr), "0x%2X", selrig->CIV);
		txtCIV->value(hexstr);
		txtCIV->activate();
		btnCIVdefault->activate();
		if (selrig->name_ == rig_IC7200.name_ ||
			selrig->name_ == rig_IC7300.name_ ||
			selrig->name_ == rig_IC7600.name_ ||
			selrig->name_ == rig_IC7800.name_ ) {
			btnUSBaudio->value(progStatus.USBaudio = true);
			btnUSBaudio->activate();
		} else
			btnUSBaudio->deactivate();
	} else {
		txtCIV->value("");
		txtCIV->deactivate();
		btnCIVdefault->deactivate();
		btnUSBaudio->value(false);
		btnUSBaudio->deactivate();
	}
}

void init_VFOs()
{
	if (selrig->name_ == rig_TT550.name_) return;
	if (selrig->name_ == rig_FT817.name_) {
		FreqDispA->value(vfoA.freq);
		FreqDispB->value(vfoB.freq);
		updateBandwidthControl();
		highlight_vfo(NULL);
		return;
	}

	vfoA.freq = progStatus.freq_A;
	vfoA.imode = progStatus.imode_A;
	vfoA.iBW = progStatus.iBW_A;

	if (vfoA.iBW == -1) vfoA.iBW = selrig->def_bandwidth(vfoA.imode);
	FreqDispA->value( vfoA.freq );

	vfoB.freq = progStatus.freq_B;
	vfoB.imode = progStatus.imode_B;
	vfoB.iBW = progStatus.iBW_B;

	if (vfoB.iBW == -1) vfoB.iBW = selrig->def_bandwidth(vfoB.imode);
	FreqDispB->value(vfoB.freq);

	if (selrig->has_vfoAB) {
		selrig->selectB();
		selrig->set_vfoB(vfoB.freq);
		selrig->set_modeB(vfoB.imode);
		selrig->set_bwB(vfoB.iBW);
	}

	selrig->selectA();
	selrig->set_vfoA(vfoA.freq);
	selrig->set_modeA(vfoA.imode);
	selrig->set_bwA(vfoA.iBW);

	vfo = vfoA;
	updateBandwidthControl();

	useB = false;
	highlight_vfo((void *)0);
}

void init_IC7300_special()
{
	if (selrig->name_ ==  rig_IC7300.name_) {
		selrig->enable_break_in();
		redrawAGC();
	}
}

void init_TS990_special()
{
	if (xcvr_name == rig_TS990.name_) { // Setup TS990 Mon Button
		btnIFsh->label("MON");
	}
}

void init_K3_KX3_special()
{
	if (xcvr_name == rig_K3.name_) {
		btnB->hide();
		btnA->hide();
		btn_KX3_swapAB->hide();
		btn_K3_swapAB->show();
	} else if (xcvr_name == rig_KX3.name_) {
		btnB->hide();
		btnA->hide();
		btn_K3_swapAB->hide();
		btn_KX3_swapAB->show();
	} else {
		btn_K3_swapAB->hide();
		btn_KX3_swapAB->hide();
		btnB->show();
		btnA->show();
	}
}

void initRig()
{
	btnInitializing->show();
	mainwindow->redraw();

	flrig_abort = false;

	sldrRcvSignal->aging(progStatus.rx_peak);
	sldrRcvSignal->avg(progStatus.rx_avg);
	sldrFwdPwr->aging(progStatus.pwr_peak);
	sldrFwdPwr->avg(progStatus.pwr_avg);

	if (progStatus.use_tcpip) {
		try {
			connect_to_remote();
		} catch (...) {
			btnInitializing->hide();
			mainwindow->redraw();
			return;
		}
	}

// disable the serial thread
	{
		guard_lock gl_serial(&mutex_serial, 71);

// Xcvr Auto Power on as soon as possible
		if (selrig->has_xcvr_auto_on_off)
			selrig->set_xcvr_auto_on();

		selrig->initialize();

		if (flrig_abort) goto failed;

		FreqDispA->set_precision(selrig->precision);
		FreqDispA->set_ndigits(selrig->ndigits);
		FreqDispB->set_precision(selrig->precision);
		FreqDispB->set_ndigits(selrig->ndigits);

		FreqDispB->set_precision(selrig->precision);
		FreqDispB->set_ndigits(selrig->ndigits);
		FreqDispB->set_precision(selrig->precision);
		FreqDispB->set_ndigits(selrig->ndigits);

		if (xcvr_name == rig_TT550.name_)
			init_TT550();
		else
			init_generic_rig();

		init_special_controls();
		init_external_tuner();
		init_rit();
		init_xit();
		init_bfo();
		init_dsp_controls();
		init_volume_control();
		init_rf_control();
		init_sql_control();
		init_noise_reduction_control();
		init_if_shift_control();
		init_notch_control();
		init_micgain_control();
		init_power_control();
		init_attenuator_control();
		init_agc_control();
		init_preamp_control();
		init_noise_control();
		init_tune_control();
		init_ptt_control();
		init_auto_notch();
		init_swr_control();
		init_compression_control();

		adjust_control_positions();

		initXcvrTab();

		buildlist();

		init_CIV();

		init_VFOs();

		selrig->post_initialize();

		init_IC7300_special();
		init_TS990_special();
		init_K3_KX3_special();

		btnAswapB->show();

	}  // enable the serial thread


	bypass_serial_thread_loop = false;

	btnInitializing->hide();
	mainwindow->redraw();

	return;

failed:
	btnInitializing->hide();
	mainwindow->redraw();

	bypass_serial_thread_loop = true;
	fl_alert2(_("Transceiver not responding"));
	return;
}

void init_title()
{
	title = PACKAGE;
	title += " ";
	title.append(selrig->name_);
	mainwindow->label(title.c_str());
}

void initConfigDialog()
{
	int picked = selectRig->index();
	rigbase *srig = rigs[picked];
	string xcvr_name = srig->name_;
	selectCommPort->index(0);

	selectCommPort->value(progStatus.xcvr_serial_port.c_str());
	btnOneStopBit->value( progStatus.stopbits == 1 );
	btnTwoStopBit->value( progStatus.stopbits == 2 );

	mnuBaudrate->index( srig->comm_baudrate );
	btnOneStopBit->value( srig->stopbits == 1 );
	btnTwoStopBit->value( srig->stopbits == 2 );
	cntRigCatRetries->value( srig->comm_retries );
	cntRigCatTimeout->value( srig->comm_timeout );
	cntRigCatWait->value( srig->comm_wait );
	btnRigCatEcho->value( srig->comm_echo );
	btncatptt->value( srig->comm_catptt );
	btnrtsptt->value( srig->comm_rtsptt );
	btndtrptt->value( srig->comm_dtrptt );
	chkrtscts->value( srig->comm_rtscts );
	btnrtsplus->value( srig->comm_rtsplus );
	btndtrplus->value( srig->comm_dtrplus );

	if (xcvr_name.find("IC") == 0) {
		char hexstr[8];
		snprintf(hexstr, sizeof(hexstr), "0x%2X", srig->CIV);
		txtCIV->value(hexstr);
		txtCIV->activate();
		btnCIVdefault->activate();
		if (xcvr_name == rig_IC7200.name_ ||
			xcvr_name == rig_IC7300.name_ ||
			xcvr_name == rig_IC7600.name_ ||
			xcvr_name == rig_IC7800.name_) {
			btnUSBaudio->value(progStatus.USBaudio = true);
			btnUSBaudio->activate();
		} else
			btnUSBaudio->deactivate();
	} else {
		txtCIV->value("");
		txtCIV->deactivate();
		btnCIVdefault->deactivate();
		btnUSBaudio->value(false);
		btnUSBaudio->deactivate();
	}
}

void initStatusConfigDialog()
{
	if (progStatus.CIV) selrig->adjustCIV(progStatus.CIV);

	selectRig->value(xcvr_name.c_str());

	mnuBaudrate->index( progStatus.comm_baudrate );

	selectCommPort->value( progStatus.xcvr_serial_port.c_str() );
	selectAuxPort->value( progStatus.aux_serial_port.c_str() );
	selectSepPTTPort->value( progStatus.sep_serial_port.c_str() );
	btnOneStopBit->value( progStatus.stopbits == 1 );
	btnTwoStopBit->value( progStatus.stopbits == 2 );

	cntRigCatRetries->value( progStatus.comm_retries );
	cntRigCatTimeout->value( progStatus.comm_timeout );
	cntRigCatWait->value( progStatus.comm_wait );
	btnRigCatEcho->value( progStatus.comm_echo );

	btncatptt->value( progStatus.comm_catptt );
	btnrtsptt->value( progStatus.comm_rtsptt );
	btndtrptt->value( progStatus.comm_dtrptt );
	chkrtscts->value( progStatus.comm_rtscts );
	btnrtsplus->value( progStatus.comm_rtsplus );
	btndtrplus->value( progStatus.comm_dtrplus );

	btnSepDTRplus->value(progStatus.sep_dtrplus);
	btnSepDTRptt->value(progStatus.sep_dtrptt);
	btnSepRTSplus->value(progStatus.sep_rtsplus);
	btnSepRTSptt->value(progStatus.sep_rtsptt);

	init_title();

	if (progStatus.use_tcpip) {
		box_xcvr_connect->color(FL_BACKGROUND2_COLOR);
		box_xcvr_connect->redraw();
	} else {
		if (!startXcvrSerial()) {
			if (progStatus.xcvr_serial_port.compare("NONE") == 0) {
				LOG_WARN("No comm port ... test mode");
			} else {
				LOG_WARN("%s cannot be accessed", progStatus.xcvr_serial_port.c_str());
				progStatus.xcvr_serial_port = "NONE";
				selectCommPort->value(progStatus.xcvr_serial_port.c_str());
			}
			box_xcvr_connect->color(FL_BACKGROUND2_COLOR);
			box_xcvr_connect->redraw();
		} else {
			selectCommPort->value(progStatus.xcvr_serial_port.c_str());
			box_xcvr_connect->color(FL_GREEN);
			box_xcvr_connect->redraw();
		}
		if (!startAuxSerial()) {
			if (progStatus.aux_serial_port.compare("NONE") != 0) {
				LOG_WARN("%s cannot be accessed", progStatus.aux_serial_port.c_str());
				progStatus.aux_serial_port = "NONE";
				selectAuxPort->value(progStatus.aux_serial_port.c_str());
			}
		}
		if (!startSepSerial()) {
			if (progStatus.sep_serial_port.compare("NONE") != 0) {
				LOG_WARN("%s cannot be accessed", progStatus.sep_serial_port.c_str());
				progStatus.sep_serial_port = "NONE";
				selectSepPTTPort->value(progStatus.sep_serial_port.c_str());
			}
		}
	}

	initRig();

}

void initRigCombo()
{
	selectRig->clear();
	int i = 0;
	while (rigs[i] != NULL)
		selectRig->add(rigs[i++]->name_.c_str());

	selectRig->index(0);
}

//----------------------------------------------------------------------
// button label and label state changes
// Note that an additional level of service request is made to insure
// that the main thread is actually changing the widget

// noise reduction
static std::string nr_label_;
static bool nr_state_;

void do_nr_label(void *)
{
	btnNR->value(nr_state_ ? 1 : 0);
	btnNR->label(nr_label_.c_str());
	btnNR->redraw_label();
}

void nr_label(const char *l, bool on = false)
{
	nr_label_ = l;
	nr_state_ = on;
	Fl::awake(do_nr_label);
}

// noise blanker
static std::string nb_label_;
static bool nb_state_;

void do_nb_label(void *)
{
	btnNOISE->value(nb_state_ ? 1 : 0);
	btnNOISE->label(nb_label_.c_str());
	btnNOISE->redraw_label();
}

void nb_label(const char * l, bool on = false)
{
	nb_label_ = l;
	nb_state_ = on;
	Fl::awake(do_nb_label);
}

// preamp label
static std::string preamp_label_;
static bool preamp_state_;

void do_preamp_label(void *)
{
	btnPreamp->value(preamp_state_ ? 1 : 0);
	btnPreamp->label(preamp_label_.c_str());
	btnPreamp->redraw_label();
}

void preamp_label(const char * l, bool on = false)
{
	preamp_label_ = l;
	preamp_state_ = on;
	Fl::awake(do_preamp_label);
}

// atten label
static std::string atten_label_;
static bool atten_state_;

void do_atten_label(void *)
{
	btnAttenuator->value(atten_state_ ? 1 : 0);
	btnAttenuator->label(atten_label_.c_str());
	btnAttenuator->redraw_label();
}

void atten_label(const char * l, bool on = false)
{
	atten_label_ = l;
	atten_state_ = on;
	Fl::awake(do_atten_label);
}

// break-in label
static std::string bkin_label_;

void do_bkin_label(void *)
{
	btnBreakIn->label(bkin_label_.c_str());
	btnBreakIn->redraw_label();
}

void break_in_label(const char *l)
{
	bkin_label_ = l;
	Fl::awake(do_bkin_label);
}

// autonotch label
static std::string auto_notch_label_;
static bool auto_notch_state_;

void do_auto_notch_label(void *)
{
	btnAutoNotch->value(auto_notch_state_ ? 1 : 0);
	btnAutoNotch->label(auto_notch_label_.c_str());
	btnAutoNotch->redraw_label();
}

void auto_notch_label(const char * l, bool on = false)
{
	auto_notch_label_ = l;
	auto_notch_state_ = on;
	Fl::awake(do_auto_notch_label);
}
//----------------------------------------------------------------------
// dynamic reassignment of IF shift range based on Bandwidth selection
// only used by Icom transceivers with Inner/Outer passband controls

int if_shift_bw_ = 0;

void do_if_shift_range(void *)
{
	if (sldrIFSHIFT) sldrIFSHIFT->minimum(-if_shift_bw_);
	if (sldrIFSHIFT) sldrIFSHIFT->maximum(if_shift_bw_);
	if (sldrIFSHIFT) sldrIFSHIFT->step(50);
	if (sldrIFSHIFT) sldrIFSHIFT->value(0);
	if (sldrIFSHIFT) sldrIFSHIFT->redraw();
	if (spnrIFSHIFT) spnrIFSHIFT->minimum(-if_shift_bw_);
	if (spnrIFSHIFT) spnrIFSHIFT->maximum(if_shift_bw_);
	if (spnrIFSHIFT) spnrIFSHIFT->step(50);
	if (spnrIFSHIFT) spnrIFSHIFT->value(0);
	if (spnrIFSHIFT) spnrIFSHIFT->redraw();
}

void if_shift_range(int bw)
{
	if_shift_bw_ = bw / 2;
	Fl::awake(do_if_shift_range);
}

//----------------------------------------------------------------------

void cbAuxPort()
{
	AuxSerial->setRTS(progStatus.aux_rts);
	AuxSerial->setDTR(progStatus.aux_dtr);
}

void cb_agc_level()
{
	guard_lock serial_lock(&mutex_serial, 72);
	selrig->set_agc_level();
}

void cb_cw_wpm()
{
	guard_lock serial_lock(&mutex_serial, 73);
	selrig->set_cw_wpm();
}

void cb_cw_vol()
{
//	guard_lock serial_lock(&mutex_serial);
//		selrig->set_cw_vol();
}

void cb_cw_spot()
{
	int ret;
	guard_lock serial_lock(&mutex_serial, 74);
	ret = selrig->set_cw_spot();
	if (!ret) btnSpot->value(0);
}

void cb_cw_spot_tone()
{
	guard_lock serial_lock(&mutex_serial, 75);
	selrig->set_cw_spot_tone();
}


void cb_vox_gain()
{
	guard_lock serial_lock(&mutex_serial, 76);
	selrig->set_vox_gain();
}

void cb_vox_anti()
{
	guard_lock serial_lock(&mutex_serial, 77);
	selrig->set_vox_anti();
}

void cb_vox_hang()
{
	guard_lock serial_lock(&mutex_serial, 78);
	selrig->set_vox_hang();
}

void cb_vox_onoff()
{
	guard_lock serial_lock(&mutex_serial, 79);
	selrig->set_vox_onoff();
}

void cb_vox_on_dataport()
{
	guard_lock serial_lock(&mutex_serial, 80);
	selrig->set_vox_on_dataport();
}

void cb_compression()
{
	guard_lock serial_lock(&mutex_serial, 81);
	selrig->set_compression();
}

void cb_auto_notch()
{
	progStatus.auto_notch = btnAutoNotch->value();
	guard_lock serial_lock(&mutex_serial, 82);
	selrig->set_auto_notch(progStatus.auto_notch);
}

void cb_vfo_adj()
{
	if (xcvr_name == rig_TT550.name_)
		progStatus.vfo_adj = spnr_tt550_vfo_adj->value();
	else
		progStatus.vfo_adj = spnr_vfo_adj->value();
	guard_lock serial_lock(&mutex_serial, 83);
	selrig->setVfoAdj(progStatus.vfo_adj);
}

void cb_line_out()
{
}

void cb_bpf_center()
{
	guard_lock serial_lock(&mutex_serial, 84);
	selrig->set_if_shift(selrig->pbt);
}

void cb_special()
{
	guard_lock serial_lock(&mutex_serial, 85);
	selrig->set_special(btnSpecial->value());
}

void cb_nb_level()
{
	guard_lock serial_lock(&mutex_serial, 86);
	progStatus.nb_level = cbo_nb_level->index();
	selrig->set_nb_level();
}

void cb_spot()
{
	guard_lock serial_lock(&mutex_serial, 87);
	selrig->set_cw_spot();
}

void cb_enable_keyer()
{
	guard_lock serial_lock(&mutex_serial, 88);
	selrig->enable_keyer();
}

void cb_enable_break_in()
{
	guard_lock serial_lock(&mutex_serial, 89);
	selrig->enable_break_in();
}

void cb_cw_weight()
{
	guard_lock serial_lock(&mutex_serial, 89);
	selrig->set_cw_weight();
}

void cb_cw_qsk()
{
	guard_lock serial_lock(&mutex_serial, 90);
	selrig->set_cw_qsk();
}

void cb_cw_delay()
{
	guard_lock serial_lock(&mutex_serial, 90);
	selrig->set_cw_delay();
}

void cbBandSelect(int band)
{
// bypass local
	{ guard_lock gl_serial(&mutex_serial, 91);
		bypass_serial_thread_loop = true;
	}
	{
		guard_lock gl_serial(&mutex_serial, 92);

	selrig->set_band_selection(band);
	MilliSleep(100);	// rig sync-up
// get freqmdbw
	if (!useB) {
		vfoA.freq = selrig->get_vfoA();
		if (selrig->has_mode_control)
			vfoA.imode = selrig->get_modeA();
		if (selrig->has_bandwidth_control) {
			selrig->adjust_bandwidth(vfoA.imode);
			vfoA.iBW = selrig->get_bwA();
		}
		vfo = vfoA;
	} else {
		vfoB.freq = selrig->get_vfoB();
		if (selrig->has_mode_control)
			vfoB.imode = selrig->get_modeB();
		if (selrig->has_bandwidth_control) {
			selrig->adjust_bandwidth(vfoB.imode);
			vfoB.iBW = selrig->get_bwB();
		}
		vfo = vfoB;
	}
// local display freqmdbw
	if (selrig->has_mode_control)
		Fl::awake(setModeControl);
	if (selrig->has_bandwidth_control) {
		set_bandwidth_control();
		Fl::awake(setBWControl);
	}
	if (!useB) Fl::awake(setFreqDispA, (void *)vfo.freq);
	else Fl::awake(setFreqDispB, (void *)vfo.freq);

	MilliSleep(100);	// local sync-up
	}

// enable local
	guard_lock gl_serial(&mutex_serial, 94);
	bypass_serial_thread_loop = false;
}

void enable_bandselect_btn(int btn_num, bool enable)
{
	switch (btn_num) {
		case 1:
		case 9:
			break;
		case 10:	// 6m
			if (enable) btnBandSelect_10->show();
			else btnBandSelect_10->hide();
			break;
		case 11:	// GEN
			if (enable) btnBandSelect_11->show();
			else btnBandSelect_11->hide();
			break;
		case 13:
			if (enable) opSelect60->show();
			else opSelect60->hide();
			break;
		default:
			break;
	}
}

// trim leading and trailing whitspace and double quote
const string lt_trim(const string& pString, const string& pWhitespace)
{
    size_t beginStr, endStr, range;
	beginStr = pString.find_first_not_of(pWhitespace);
    if (beginStr == string::npos) return "";	// no content
    endStr = pString.find_last_not_of(pWhitespace);
    range = endStr - beginStr + 1;

    return pString.substr(beginStr, range);
}

void editAlphaTag()
{
	int indx;
	string atag;
	if (FreqSelect->value() < 1) {
		inAlphaTag->value("");
		return;	// no memory selected
	}
	indx = FreqSelect->value() - 1;
	atag = inAlphaTag->value();
// delete leading, trailing spaces
	atag = lt_trim(atag);
	memset(oplist[indx].alpha_tag, 0, ATAGSIZE);
	snprintf(oplist[indx].alpha_tag, ATAGSIZE, "%s", atag.c_str());
// update browser list
	updateSelect();
	FreqSelect->value(indx + 1);
	inAlphaTag->value(oplist[indx].alpha_tag);
}

