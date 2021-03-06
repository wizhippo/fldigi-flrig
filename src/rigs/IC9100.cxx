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

#include "IC9100.h"

bool IC9100_DEBUG = true;

//=============================================================================
// IC-9100

const char IC9100name_[] = "IC-9100";

const char *IC9100modes_[] = {
"LSB", "USB", "AM", "CW", "RTTY", "FM", "CW-R", "RTTY-R", "DV", 
"LSB-D", "USB-D", NULL};

const char IC9100_mode_type[] = {
	'L', 'U', 'U', 'U', 'L', 'U', 'L', 'U', 'U',
	'L', 'U' };

const char IC9100_mode_nbr[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x07, 0x08, 0x17,
	0x00, 0x01 };

const char *IC9100_ssb_bws[] = {
"50",    "100",  "150",  "200",  "250",  "300",  "350",  "400",  "450",  "500",
"600",   "700",  "800",  "900", "1000", "1100", "1200", "1300", "1400", "1500",
"1600", "1700", "1800", "1900", "2000", "2100", "2200", "2300", "2400", "2500",
"2600", "2700", "2800", "2900", "3000", "3100", "3200", "3300", "3400", "3500",
"3600", NULL };

static int IC9100_bw_vals_SSB[] = {
 1, 2, 3, 4, 5, 6, 7, 8, 9,10,
11,12,13,14,15,16,17,18,19,20,
21,22,23,24,25,26,27,28,29,30,
31,32,33,34,35,36,37,38,39,40,
41, WVALS_LIMIT};

const char *IC9100_rtty_bws[] = {
"50",    "100",  "150",  "200",  "250",  "300",  "350",  "400",  "450",  "500",
"600",   "700",  "800",  "900", "1000", "1100", "1200", "1300", "1400", "1500",
"1600", "1700", "1800", "1900", "2000", "2100", "2200", "2300", "2400", "2500",
"2600", "2700", NULL };

static int IC9100_bw_vals_RTTY[] = {
 1, 2, 3, 4, 5, 6, 7, 8, 9,10,
11,12,13,14,15,16,17,18,19,20,
21,22,23,24,25,26,27,28,29,30,
31,32, WVALS_LIMIT};

const char *IC9100_am_bws[] = {
"200",   "400",  "600",  "800", "1000", "1200", "1400", "1600", "1800", "2000",
"2200", "2400", "2600", "2800", "3000", "3200", "3400", "3600", "3800", "4000",
"4200", "4400", "4600", "4800", "5000", "5200", "5400", "5600", "5800", "6000",
"6200", "6400", "6600", "6800", "7000", "7200", "7400", "9100", "7800", "8000",
"8200", "8400", "8600", "8800", "9000", "9200", "9400", "9600", "9800", "10000", NULL };

static int IC9100_bw_vals_AM[] = {
 1, 2, 3, 4, 5, 6, 7, 8, 9,10,
11,12,13,14,15,16,17,18,19,20,
21,22,23,24,25,26,27,28,29,30,
31,32,33,34,35,36,37,38,39,40,
41,42,43,44,45,46,47,48,49,50,
WVALS_LIMIT};

const char *IC9100_fm_bws[] = {"FIXED", NULL };
static int IC9100_bw_vals_FM[] = {1, WVALS_LIMIT};

static GUI IC9100_widgets[]= {
	{ (Fl_Widget *)btnVol, 2, 125,  50 },
	{ (Fl_Widget *)sldrVOLUME, 54, 125, 156 },
	{ (Fl_Widget *)btnAGC, 2, 145, 50 },
	{ (Fl_Widget *)sldrRFGAIN, 54, 145, 156 },
	{ (Fl_Widget *)sldrSQUELCH, 54, 165, 156 },
	{ (Fl_Widget *)btnNR, 2, 185,  50 },
	{ (Fl_Widget *)sldrNR, 54, 185, 156 },
	{ (Fl_Widget *)btnIFsh, 214, 125,  50 },
	{ (Fl_Widget *)sldrIFSHIFT, 266, 125, 156 },
	{ (Fl_Widget *)btnNotch, 214, 145,  50 },
	{ (Fl_Widget *)sldrNOTCH, 266, 145, 156 },
	{ (Fl_Widget *)sldrMICGAIN, 266, 165, 156 },
	{ (Fl_Widget *)sldrPOWER, 266, 185, 156 },
	{ (Fl_Widget *)NULL, 0, 0, 0 }
};

RIG_IC9100::RIG_IC9100() {
	defaultCIV = 0x7C;
	name_ = IC9100name_;
	modes_ = IC9100modes_;

	bandwidths_ = IC9100_ssb_bws;
	bw_vals_ = IC9100_bw_vals_SSB;

	_mode_type = IC9100_mode_type;
	adjustCIV(defaultCIV);

	widgets = IC9100_widgets;

	has_extras =

	has_cw_wpm =
	has_cw_spot_tone =
	has_cw_qsk =

	has_vox_onoff =
	has_vox_gain =
	has_vox_anti =
	has_vox_hang =

	has_smeter =
	has_power_out =
	has_alc_control =
	has_swr_control =
	has_agc_control =

	has_compON =
	has_compression =

	has_auto_notch = 
	has_notch_control = 

	has_split =

	has_micgain_control =
	has_bandwidth_control = true;

	precision = 1;
	ndigits = 9;

};

//======================================================================
// IC9100 unique commands
//======================================================================

void RIG_IC9100::initialize()
{
	IC9100_widgets[0].W = btnVol;
	IC9100_widgets[1].W = sldrVOLUME;
	IC9100_widgets[2].W = btnAGC;
	IC9100_widgets[3].W = sldrRFGAIN;
	IC9100_widgets[4].W = sldrSQUELCH;
	IC9100_widgets[5].W = btnNR;
	IC9100_widgets[6].W = sldrNR;
	IC9100_widgets[7].W = btnIFsh;
	IC9100_widgets[8].W = sldrIFSHIFT;
	IC9100_widgets[9].W = btnNotch;
	IC9100_widgets[10].W = sldrNOTCH;
	IC9100_widgets[11].W = sldrMICGAIN;
	IC9100_widgets[12].W = sldrPOWER;
	cmd = pre_to;
	cmd += '\x1A'; cmd += '\x05';
	cmd += '\x00'; cmd += '\x58'; cmd += '\x00';
	cmd.append(post);
	sendICcommand(cmd,6);
	checkresponse();
}

void RIG_IC9100::selectA()
{
	cmd = pre_to;
	cmd += '\x07';
	cmd += '\xD0';
	cmd.append(post);
	waitFB("select A");
}

void RIG_IC9100::selectB()
{
	cmd = pre_to;
	cmd += '\x07';
	cmd += '\xD1';
	cmd.append(post);
	waitFB("select B");
}

void RIG_IC9100::set_modeA(int val)
{
	A.imode = val;
	cmd = pre_to;
	cmd += '\x06';
	cmd += IC9100_mode_nbr[val];
	cmd.append( post );
	if (IC9100_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
	waitFB("set mode A");

	cmd = pre_to;
	cmd += '\x1A'; cmd += '\x06';
	if (A.imode > 8) cmd += '\x01';
	else cmd += '\x00';
	cmd.append( post);
	waitFB("data mode");
}

int RIG_IC9100::get_modeA()
{
	int md = 0;
	cmd = pre_to;
	cmd += '\x04';
	cmd.append(post);
	string resp = pre_fm;
	resp += '\x04';
	if (waitFOR(8, "get mode/bw A")) {
		for (md = 0; md < 9; md++) if (replystr[5] == IC9100_mode_nbr[md]) break;
	} else {
		checkresponse();
		return A.imode;
	}
	if (md > 8) md = 1; // force USB

	cmd = pre_to;
	cmd += "\x1A\x06";
	cmd.append(post);
	resp = pre_fm;
	resp += "\x1A\x06";
	if (waitFOR(9, "data mode?")) {
		size_t p = replystr.rfind(resp);
		if (p != string::npos) {
			if ((replystr[p+6] & 0x01) == 0x01) {
				if (md == 0) md = 9;
				if (md == 1) md = 10;
			}
		}
	}

	A.imode = md;

	return A.imode;
}

void RIG_IC9100::set_modeB(int val)
{
	B.imode = val;
	cmd = pre_to;
	cmd += '\x06';
	cmd += IC9100_mode_nbr[val];
	cmd.append( post );
	if (IC9100_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
	waitFB("set mode B");

	cmd = pre_to;
	cmd += '\x1A'; cmd += '\x06';
	if (B.imode > 8) cmd += '\x01';
	else cmd += '\x00';
	cmd.append( post);
	waitFB("data mode");
}

int RIG_IC9100::get_modeB()
{
	int md = 0;
	cmd = pre_to;
	cmd += '\x04';
	cmd.append(post);
	string resp = pre_fm;
	resp += '\x04';
	if (waitFOR(8, "get mode A")) {
		for (md = 0; md < 9; md++) if (replystr[5] == IC9100_mode_nbr[md]) break;
	} else {
		checkresponse();
		return B.imode;
	}
	if (md > 8) md = 1; // force USB

	cmd = pre_to;
	cmd += "\x1A\x06";
	cmd.append(post);
	resp = pre_fm;
	resp += "\x1A\x06";
	if (waitFOR(9, "data mode?")) {
		size_t p = replystr.rfind(resp);
		if (p != string::npos) {
			if ((replystr[p+6] & 0x01) == 0x01) {
				if (md == 0) md = 9;
				if (md == 1) md = 10;
			}
		}
	}

	B.imode = md;

	return A.imode;
}

int RIG_IC9100::get_bwA()
{
	if (A.imode == 5) return 0;

	cmd = pre_to;
	cmd.append("\x1a\x03");
	cmd.append(post);
	string resp = pre_fm;
	resp.append("\x1a\x03");
	if (waitFOR(8, "get bw A")) {
		size_t p = replystr.rfind(resp);
		A.iBW = fm_bcd(replystr.substr(p+6), 2);
	}
	return A.iBW;
}

void RIG_IC9100::set_bwA(int val)
{
	A.iBW = val;
	if (A.imode == 5) return;
	cmd = pre_to;
	cmd.append("\x1a\x03");
	cmd.append(to_bcd(A.iBW, 2));
	cmd.append(post);
	if (IC9100_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
	waitFB("set bw A");
}

int RIG_IC9100::get_bwB()
{
	if (B.imode == 5) return 0;

	cmd = pre_to;
	cmd.append("\x1a\x03");
	cmd.append(post);
	string resp = pre_fm;
	resp.append("\x1a\x03");
	if (waitFOR(8, "get bw A")) {
		size_t p = replystr.rfind(resp);
		B.iBW = fm_bcd(replystr.substr(p+6), 2);
	}
	return B.iBW;
}

void RIG_IC9100::set_bwB(int val)
{
	B.iBW = val;
	if (B.imode == 5) return;
	cmd = pre_to;
	cmd.append("\x1a\x03");
	cmd.append(to_bcd(A.iBW, 2));
	cmd.append(post);
	if (IC9100_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
	waitFB("set bw B");
}

// LSB, USB, AM, CW, RTTY, FM, CW-R, RTTY-R, DV, LSB-D, USB-D
//  0    1    2   3   4     5   6      7      8    9     10
int RIG_IC9100::adjust_bandwidth(int m)
{
	int bw = 0;
	switch (m) {
		case 2: // AM
			bandwidths_ = IC9100_am_bws;
			bw_vals_ = IC9100_bw_vals_AM;
			bw = 19;
			break;
		case 5: // FM
			bandwidths_ = IC9100_fm_bws;
			bw_vals_ = IC9100_bw_vals_FM;
			bw = 0;
			break;
		case 4: case 7: // RTTY
			bandwidths_ = IC9100_rtty_bws;
			bw_vals_ = IC9100_bw_vals_RTTY;
			bw = 12;
			break;
		case 3: case 6: // CW
			bandwidths_ = IC9100_ssb_bws;
			bw_vals_ = IC9100_bw_vals_SSB;
			bw = 12;
			break;
		case 8:          // DV
		case 0: case 1:  // SSB
		case 9: case 10: // DATA
		default:
			bandwidths_ = IC9100_ssb_bws;
			bw_vals_ = IC9100_bw_vals_SSB;
			bw = 34;
	}
	return bw;
}

int RIG_IC9100::def_bandwidth(int m)
{
	int bw = 0;
	switch (m) {
		case 2:          // AM
			bw = 19;
			break;
		case 5:          // FM
			bw = 0;
			break;
		case 4: case 7:  // RTTY
			bw = 12;
			break;
		case 3: case 6:  // CW
			bw = 12;
			break;
		case 8:          // DV
		case 0: case 1:  // SSB
		case 9: case 10: // DATA MODES
		default:
			bw = 34;
	}
	return bw;
}

void RIG_IC9100::set_mic_gain(int v)
{
	ICvol = (int)(v * 255 / 100);
	if (!progStatus.USBaudio) {
		cmd = pre_to;
		cmd.append("\x14\x0B");
		cmd.append(to_bcd(ICvol, 3));
		cmd.append( post );
	} else {
		cmd = pre_to;
		cmd += '\x1A'; cmd += '\x05';
		cmd += '\x00'; cmd += '\x29';
		cmd.append(to_bcd(ICvol, 3));
		cmd.append( post );
	}
	waitFB("set mic gain");
	if (IC9100_DEBUG)
		LOG_WARN("%s", str2hex(cmd.data(), cmd.length()));
}

void RIG_IC9100::set_attenuator(int val)
{
	int cmdval = 0;
	if (atten_level == 0) {
		atten_level = 1;
		atten_label("20 dB", true);
		cmdval = 0x20;
	} else {
		atten_level = 0;
		atten_label("Att", false);
		cmdval = 0x00;
	}
	cmd = pre_to;
	cmd += '\x11';
	cmd += cmdval;
	cmd.append( post );
	waitFB("set attenuator");
	if (IC9100_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

int RIG_IC9100::get_attenuator()
{
	cmd = pre_to;
	cmd += '\x11';
	cmd.append( post );
	string resp = pre_fm;
	resp += '\x11';
	if (waitFOR(7, "get attenuator")) {
		if (replystr[5] == 0x20) {
			atten_level = 1;
			atten_label("20 dB", true);
		} else {
			atten_level = 0;
			atten_label("Att", false);
		}
	}
	return atten_level;
}

void RIG_IC9100::set_compression()
{
	if (progStatus.compON) {
		cmd.assign(pre_to).append("\x14\x0E");
		cmd.append(to_bcd(progStatus.compression * 255 / 100, 3));
		cmd.append( post );
		waitFB("set comp");

		cmd = pre_to;
		cmd.append("\x16\x44");
		cmd += '\x01';
		cmd.append(post);
		waitFB("set Comp ON");

	} else{
		cmd.assign(pre_to).append("\x16\x44");
		cmd += '\x00';
		cmd.append(post);
		waitFB("set Comp OFF");
	}
}

void RIG_IC9100::set_vox_onoff()
{
	cmd.assign(pre_to).append("\x16\x46");
	if (progStatus.vox_onoff) {
		cmd += '\x01';
		cmd.append( post );
		waitFB("set vox ON");
	} else {
		cmd += '\x00';
		cmd.append( post );
		waitFB("set vox OFF");
	}
}

void RIG_IC9100::set_vox_gain()
{
	cmd.assign(pre_to).append("\x1A\x05");
	cmd +='\x01';
	cmd +='\x25';
	cmd.append(to_bcd((int)(progStatus.vox_gain * 2.55), 3));
	cmd.append( post );
	waitFB("SET vox gain");
}

void RIG_IC9100::set_vox_anti()
{
	cmd.assign(pre_to).append("\x1A\x05");
	cmd +='\x01';
	cmd +='\x26';
	cmd.append(to_bcd((int)(progStatus.vox_anti * 2.55), 3));
	cmd.append( post );
	waitFB("SET anti-vox");
}

void RIG_IC9100::set_vox_hang()
{
	cmd.assign(pre_to).append("\x1A\x05");	//values 00-20 = 0.0 - 2.0 sec
	cmd +='\x01';
	cmd +='\x27';
	cmd.append(to_bcd((int)(progStatus.vox_hang / 10 ), 2));
	cmd.append( post );
	waitFB("SET vox hang");
}

// these need to be written and tested
void  RIG_IC9100::get_vox_onoff()
{
}

void  RIG_IC9100::get_vox_gain()
{
}

void  RIG_IC9100::get_vox_anti()
{
}

void  RIG_IC9100::get_vox_hang()
{
}

// CW controls

void RIG_IC9100::set_cw_wpm()
{
	cmd.assign(pre_to).append("\x14\x0C"); // values 0-255
	cmd.append(to_bcd(round((progStatus.cw_wpm - 6) * 255 / (60 - 6)), 3));
	cmd.append( post );
	waitFB("SET cw wpm");
}

void RIG_IC9100::set_cw_qsk()
{
	int n = round(progStatus.cw_qsk * 10); // values 0-255
	cmd.assign(pre_to).append("\x14\x0F");
	cmd.append(to_bcd(n, 3));
	cmd.append(post);
	waitFB("Set cw qsk delay");
}

void RIG_IC9100::set_cw_spot_tone()
{
	cmd.assign(pre_to).append("\x14\x09"); // values 0=300Hz 255=900Hz
	int n = round((progStatus.cw_spot_tone - 300) * 255.0 / 600.0);
	if (n > 255) n = 255;
	if (n < 0) n = 0;
	cmd.append(to_bcd(n, 3));
	cmd.append( post );
	waitFB("SET cw spot tone");
}

void RIG_IC9100::set_cw_vol()
{
	cmd.assign(pre_to);
	cmd.append("\x1A\x05");
	cmd += '\x00';
	cmd += '\x24';
	cmd.append(to_bcd((int)(progStatus.cw_vol * 2.55), 3));
	cmd.append( post );
	waitFB("SET cw sidetone volume");
}

int RIG_IC9100::get_smeter()
{
	string cstr = "\x15\x02";
	string resp = pre_fm;
	resp.append(cstr);
	cmd = pre_to;
	cmd.append(cstr);
	cmd.append( post );
	int mtr= -1;
	if (waitFOR(9, "get smeter")) {
		size_t p = replystr.rfind(resp);
		if (p != string::npos) {
			mtr = fm_bcd(replystr.substr(p+6), 3);
			mtr = (int)ceil(mtr /2.40);
			if (mtr > 100) mtr = 100;
		}
	}
	return mtr;
}

int RIG_IC9100::get_power_out()
{
	string cstr = "\x15\x11";
	string resp = pre_fm;
	resp.append(cstr);
	cmd = pre_to;
	cmd.append(cstr);
	cmd.append( post );
	int mtr= -1;
	if (waitFOR(9, "get pout")) {
		size_t p = replystr.rfind(resp);
		if (p != string::npos) {
			mtr = fm_bcd(replystr.substr(p+6), 3);
			mtr = (int)ceil(mtr /2.15);
			if (mtr > 100) mtr = 100;
		}
	}
	return mtr;
}

int RIG_IC9100::get_swr()
{
	string cstr = "\x15\x12";
	string resp = pre_fm;
	resp.append(cstr);
	cmd = pre_to;
	cmd.append(cstr);
	cmd.append( post );
	int mtr= -1;
	if (waitFOR(9, "get SWR")) {
		size_t p = replystr.rfind(resp);
		if (p != string::npos) {
			mtr = fm_bcd(replystr.substr(p+6), 3);
			mtr = (int)ceil(mtr /2.40);
			if (mtr > 100) mtr = 100;
		}
	}
	return mtr;
}

int RIG_IC9100::get_alc()
{
	string cstr = "\x15\x13";
	string resp = pre_fm;
	resp.append(cstr);
	cmd = pre_to;
	cmd.append(cstr);
	cmd.append( post );
	int mtr= -1;
	if (waitFOR(9, "get alc")) {
		size_t p = replystr.rfind(resp);
		if (p != string::npos) {
			mtr = fm_bcd(replystr.substr(p+6), 3);
			mtr = (int)ceil(mtr /2.55);
			if (mtr > 100) mtr = 100;
		}
	}
	return mtr;
}

void RIG_IC9100::set_notch(bool on, int val)
{
	int notch = (int)(val * 256.0 / 3000.0);

	cmd = pre_to;
	cmd.append("\x16\x48");
	cmd += on ? '\x01' : '\x00';
	cmd.append(post);
	waitFB("set notch");

	cmd = pre_to;
	cmd.append("\x14\x0D");
	cmd.append(to_bcd(notch,3));
	cmd.append(post);
	waitFB("set notch val");

}

bool RIG_IC9100::get_notch(int &val)
{
	bool on = false;
	val = 0;

	string cstr = "\x16\x48";
	string resp = pre_fm;
	resp.append(cstr);
	cmd = pre_to;
	cmd.append(cstr);
	cmd.append( post );
	if (waitFOR(8, "get notch")) {
		size_t p = replystr.rfind(resp);
		if (p != string::npos)
			on = replystr[p + 6];
		cmd = pre_to;
		resp = pre_fm;
		cstr = "\x14\x0D";
		cmd.append(cstr);
		resp.append(cstr);
		cmd.append(post);
		if (waitFOR(9, "notch val")) {
			size_t p = replystr.rfind(resp);
			if (p != string::npos)
				val = (int)ceil(fm_bcd(replystr.substr(p+6),3) * 3000.0 / 255.0);
		}
	}
	return on;
}

void RIG_IC9100::get_notch_min_max_step(int &min, int &max, int &step)
{
	min = 0;
	max = 3000;
	step = 20;
}

void RIG_IC9100::set_auto_notch(int val)
{
	cmd = pre_to;
	cmd += '\x16';
	cmd += '\x41';
	cmd += val ? 0x01 : 0x00;
	cmd.append( post );
	waitFB("set AN");
}

int RIG_IC9100::get_auto_notch()
{
	string cstr = "\x16\x41";
	string resp = pre_fm;
	resp.append(cstr);
	cmd = pre_to;
	cmd.append(cstr);
	cmd.append( post );
	if (waitFOR(8, "get AN")) {
		size_t p = replystr.rfind(resp);
		if (p != string::npos) {
			if (replystr[p+6] == 0x01) {
				auto_notch_label("AN", true);
				return true;
			} else {
				auto_notch_label("AN", false);
				return false;
			}
		}
	}
	return progStatus.auto_notch;
}

void RIG_IC9100::set_split(bool val)
{
	cmd = pre_to;
	cmd += 0x0F;
	cmd += val ? 0x01 : 0x00;
	cmd.append(post);
	waitFB("set split");
}

int  RIG_IC9100::get_split()
{
	LOG_WARN("%s", "get split - not implemented");
	return progStatus.split;

	cmd = pre_to;
	cmd += 0x0F;
	cmd.append(post);
	string resp = pre_fm;
	resp += 0x0F;
	if (waitFOR(8, "get split")) {
		size_t p = replystr.rfind(resp);
		if (p != string::npos)
			return (replystr[p+6] == 0x01 ? 1 : 0);
	}
	return 0;
}

static int agcval = 1;
int  RIG_IC9100::get_agc()
{
	cmd = pre_to;
	cmd.append("\x16\x12");
	cmd.append(post);
	if (waitFOR(8, "get AGC")) {
		size_t p = replystr.find(pre_fm);
		if (p == string::npos) return agcval;
		return (agcval = replystr[p+6]); // 1 = FAST, 2 = MID, 3 = SLOW
	}
	return agcval;
}

int RIG_IC9100::incr_agc()
{
	agcval++;
	if (agcval == 4) agcval = 1;
	cmd = pre_to;
	cmd.append("\x16\x12");
	cmd += agcval;
	cmd.append(post);
	waitFB("set AGC");
	return agcval;
}


static const char *agcstrs[] = {"FST", "MID", "SLO"};
const char *RIG_IC9100::agc_label()
{
	return agcstrs[agcval - 1];
}

int  RIG_IC9100::agc_val()
{
	return (agcval);
}
