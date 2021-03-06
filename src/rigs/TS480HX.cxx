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

#include "TS480HX.h"
#include "support.h"

static const char TS480HXname_[] = "TS-480HX";

static const char *TS480HXmodes_[] = {
		"LSB", "USB", "CW", "FM", "AM", "FSK", "CW-R", "FSK-R", NULL};
static const char TS480HX_mode_chr[] =  { '1', '2', '3', '4', '5', '6', '7', '9' };
static const char TS480HX_mode_type[] = { 'L', 'U', 'U', 'U', 'U', 'L', 'L', 'U' };

static const char *TS480HX_empty[] = { "N/A", NULL };
static int TS480HX_bw_vals[] = {1, WVALS_LIMIT};

// SL command is lo cut when menu 045 OFF
static const char *TS480HX_SL[] = {
  "0",   "50", "100", "200", "300", 
"400",  "500", "600", "700", "800", 
"900", "1000", NULL };
static const char *TS480HX_SL_tooltip = "lo cut";
static const char *TS480HX_btn_SL_label = "L";

// SH command is hi cut when menu 045 OFF
static const char *TS480HX_SH[] = {
"1000", "1200", "1400", "1600", "1800", 
"2000", "2200", "2400", "2600", "2800", 
"3000", "3400", "4000", "5000", NULL };
static int TS480HX_HI_bw_vals[] = { 1,2,3,4,5,6,7,8,9,10,11,12,13,14,WVALS_LIMIT};

static const char *TS480HX_SH_tooltip = "hi cut";
static const char *TS480HX_btn_SH_label = "H";

// SL command is width when menu 045 ON
static const char *TS480HX_dataW[] = {
"50", "100", "250", "500", "1000", "1500", "2400", NULL };
static int TS480HX_data_bw_vals[] = {1,2,3,4,5,6,7, WVALS_LIMIT};

static const char *TS480HX_dataW_tooltip = "width";
static const char *TS480HX_dataW_label = "W";

// SH command is center when menu 045 ON
static const char *TS480HX_dataC[] = {
"1000", "1500", "2210", NULL };
static const char *TS480HX_dataC_tooltip = "center";
static const char *TS480HX_dataC_label = "C";

static const char *TS480HX_AM_SL[] = {
"10", "100", "200", "500",
NULL };

static const char *TS480HX_AM_SH[] = {
"2500", "3000", "4000", "5000",
NULL };
//static int TS480HX_AM_bw_vals[] = {1,2,3,4,WVALS_LIMIT};

static const char *TS480HX_CWwidths[] = {
"50", "80", "100", "150", "200", 
"300", "400", "500", "600", "1000", 
"2000", NULL};
static int TS480HX_CW_bw_vals[] = {1,2,3,4,5,6,7,8,9,10,11,WVALS_LIMIT};


static const char *TS480HX_CWbw[] = {
"FW0050;", "FW0080;", "FW0100;", "FW0150;", "FW0200;", 
"FW0300;", "FW0400;", "FW0500;", "FW0600;", "FW1000;", 
"FW2000;" };

static const char *TS480HX_FSKwidths[] = {
"250", "500", "1000", "1500", NULL};
static int TS480HX_FSK_bw_vals[] = { 1,2,3,4,WVALS_LIMIT};

static const char *TS480HX_FSKbw[] = {
"FW0250;", "FW0500;", "FW1000;", "FW1500;" };

static GUI rig_widgets[]= {
	{ (Fl_Widget *)btnVol,        2, 125,  50 },
	{ (Fl_Widget *)sldrVOLUME,   54, 125, 156 },
	{ (Fl_Widget *)sldrRFGAIN,   54, 145, 156 },
	{ (Fl_Widget *)btnIFsh,     214, 105,  50 },
	{ (Fl_Widget *)sldrIFSHIFT, 266, 105, 156 },
	{ (Fl_Widget *)btnDataPort, 214, 125,  50 },
	{ (Fl_Widget *)sldrMICGAIN, 266, 125, 156 },
	{ (Fl_Widget *)sldrPOWER,   266, 145, 156 },
	{ (Fl_Widget *)NULL,          0,   0,   0 }
};

void RIG_TS480HX::initialize()
{
	rig_widgets[0].W = btnVol;
	rig_widgets[1].W = sldrVOLUME;
	rig_widgets[2].W = sldrRFGAIN;
	rig_widgets[3].W = btnIFsh;
	rig_widgets[4].W = sldrIFSHIFT;
	rig_widgets[5].W = btnDataPort;
	rig_widgets[6].W = sldrMICGAIN;
	rig_widgets[7].W = sldrPOWER;

	if (progStatus.UIsize == small_ui) has_extras = false;
	progStatus.rfgain = 100;
	progStatus.volume = 25;
	progStatus.power_level = 20;
	progStatus.mic_gain = 25;
	check_menu_45();
};

RIG_TS480HX::RIG_TS480HX() {
// base class values
	name_ = TS480HXname_;
	modes_ = TS480HXmodes_;
	_mode_type  = TS480HX_mode_type;
	bandwidths_ = TS480HX_SH;
	bw_vals_ = TS480HX_HI_bw_vals;

	dsp_SL     = TS480HX_SL;
	SL_tooltip = TS480HX_SL_tooltip;
	SL_label   = TS480HX_btn_SL_label;

	dsp_SH     = TS480HX_SH;
	SH_tooltip = TS480HX_SL_tooltip;
	SH_label   = TS480HX_btn_SH_label;

	widgets = rig_widgets;

	comm_baudrate = BR4800;
	stopbits = 2;
	comm_retries = 2;
	comm_wait = 5;
	comm_timeout = 50;
	comm_rtscts = true;
	comm_rtsplus = false;
	comm_dtrplus = false;
	comm_catptt = true;
	comm_rtsptt = false;
	comm_dtrptt = false;
	B.imode = A.imode = 1;
	B.iBW = A.iBW = 0x8A03;
	B.freq = A.freq = 14070000;

	has_tune_control = false;
	has_noise_control = false;
	has_notch_control = false;

	has_extras = true;
	has_split = true;
	has_split_AB = true;
	has_data_port = true;
	has_micgain_control = true;
	has_ifshift_control = true;
	has_rf_control = true;
	has_power_out = true;
	has_dsp_controls = true;
	has_smeter = true;
	has_swr_control = true;
	has_alc_control = true;
	has_attenuator_control = true;
	has_preamp_control = true;
	has_mode_control = true;
	has_bandwidth_control = true;
	has_volume_control = true;
	has_power_control = true;
	has_tune_control =  true;
	has_ptt_control = true;

	precision = 1;
	ndigits = 8;

}

const char * RIG_TS480HX::get_bwname_(int n, int md) 
{
	static char bwname[20];
	if (n > 256) {
		int hi = (n >> 8) & 0x7F;
		int lo = n & 0xFF;
		snprintf(bwname, sizeof(bwname), "%s/%s",
			(md == 0 || md == 1 || md == 3) ? dsp_SL[lo] : TS480HX_AM_SL[lo],
			(md == 0 || md == 1 || md == 3) ? dsp_SH[hi] : TS480HX_AM_SH[hi] );
	} else {
		snprintf(bwname, sizeof(bwname), "%s",
			(md == 2 || md == 6) ? TS480HX_CWwidths[n] : TS480HX_FSKwidths[n]);
	}
	return bwname;
}

void RIG_TS480HX::check_menu_45()
{
// read current switch 45 setting
	menu_45 = false;
	cmd = "EX0450000;";
	int ret = wait_char(';', 11, 100, "Check menu item 45", ASC);
	if (ret >= 11) {
		size_t p = replystr.rfind("EX045");
		if (p != string::npos)
			menu_45 = (replystr[p+9] == '1');
	}

	if (menu_45) {
		dsp_SL     = TS480HX_dataW;
		SL_tooltip = TS480HX_dataW_tooltip;
		SL_label   = TS480HX_dataW_label;
		dsp_SH     = TS480HX_dataC;
		SH_tooltip = TS480HX_dataC_tooltip;
		SH_label   = TS480HX_dataC_label;
		B.iBW = A.iBW = 0x8106;
	} else {
		dsp_SL     = TS480HX_SL;
		SL_tooltip = TS480HX_SL_tooltip;
		SL_label   = TS480HX_btn_SL_label;
		dsp_SH     = TS480HX_SH;
		SH_tooltip = TS480HX_SH_tooltip;
		SH_label   = TS480HX_btn_SH_label;
		B.iBW = A.iBW = 0x8A03;
	}
}

void RIG_TS480HX::shutdown()
{
}

void RIG_TS480HX::selectA()
{
	cmd = "FR0;FT0;";
	sendCommand(cmd);
	showresp(WARN, ASC, "Rx on A, Tx on A", cmd, "");
}

void RIG_TS480HX::selectB()
{
	cmd = "FR1;FT1;";
	sendCommand(cmd);
	showresp(WARN, ASC, "Rx on B, Tx on B", cmd, "");
}

void RIG_TS480HX::set_split(bool val) 
{
	split = val;
	if (useB) {
		if (val) {
			cmd = "FR1;FT0;";
			sendCommand(cmd);
			showresp(WARN, ASC, "Rx on B, Tx on A", cmd, "");
		} else {
			cmd = "FR1;FT1;";
			sendCommand(cmd);
			showresp(WARN, ASC, "Rx on B, Tx on B", cmd, "");
		}
	} else {
		if (val) {
			cmd = "FR0;FT1;";
			sendCommand(cmd);
			showresp(WARN, ASC, "Rx on A, Tx on B", cmd, "");
		} else {
			cmd = "FR0;FT0;";
			sendCommand(cmd);
			showresp(WARN, ASC, "Rx on A, Tx on A", cmd, "");
		}
	}
	Fl::awake(highlight_vfo, (void *)0);
}

bool RIG_TS480HX::can_split()
{
	return true;
}

int RIG_TS480HX::get_split()
{
	size_t p;
	int split = 0;
	char rx = 0, tx = 0;
// tx vfo
	cmd = rsp = "FT";
	cmd.append(";");
	if (wait_char(';', 4, 100, "get split tx vfo", ASC) == 4) {
		p = replystr.rfind(rsp);
		if (p == string::npos) return split;
			tx = replystr[p+2];
	}

// rx vfo
	cmd = rsp = "FR";
	cmd.append(";");
	if (wait_char(';', 4, 100, "get split rx vfo", ASC) == 4) {
		p = replystr.rfind(rsp);
		if (p == string::npos) return split;
		rx = replystr[p+2];
// split test
		split = (tx == '1' ? 2 : 0) + (rx == '1' ? 1 : 0);
	}

	return split;
}

long RIG_TS480HX::get_vfoA ()
{
	cmd = "FA;";
	if (wait_char(';', 14, 100, "get vfo A", ASC) != 14) return A.freq;

	size_t p = replystr.rfind("FA");
	if (p != string::npos && (p + 12 < replystr.length())) {
		int f = 0;
		for (size_t n = 2; n < 13; n++)
			f = f*10 + replystr[p+n] - '0';
		A.freq = f;
	}
	return A.freq;
}

void RIG_TS480HX::set_vfoA (long freq)
{
	A.freq = freq;
	cmd = "FA00000000000;";
	for (int i = 12; i > 1; i--) {
		cmd[i] += freq % 10;
		freq /= 10;
	}
	sendCommand(cmd);
	showresp(WARN, ASC, "set vfo A", cmd, "");
}

long RIG_TS480HX::get_vfoB ()
{
	cmd = "FB;";
	if (wait_char(';', 14, 100, "get vfo B", ASC) != 14) return B.freq;

	size_t p = replystr.rfind("FB");
	if (p != string::npos && (p + 12 < replystr.length())) {
		int f = 0;
		for (size_t n = 2; n < 13; n++)
			f = f*10 + replystr[p+n] - '0';
		B.freq = f;
	}
	return B.freq;
}

void RIG_TS480HX::set_vfoB (long freq)
{
	B.freq = freq;
	cmd = "FB00000000000;";
	for (int i = 12; i > 1; i--) {
		cmd[i] += freq % 10;
		freq /= 10;
	}
	sendCommand(cmd);
	showresp(WARN, ASC, "set vfo B", cmd, "");
}

// SM cmd 0 ... 100 (rig values 0 ... 15)
int RIG_TS480HX::get_smeter()
{
	int mtr = 0;
	cmd = "SM0;";
	if (wait_char(';', 8, 100, "get Smeter", ASC) != 8) return 0;

	size_t p = replystr.rfind("SM");
	if (p != string::npos)
		mtr = 5 * atoi(&replystr[p + 3]);
	return mtr;
}

int RIG_TS480HX::get_power_out()
{
	int mtr = 0;
	cmd = "SM0;";
	if (wait_char(';', 8, 100, "get power", ASC) != 8) return mtr;

	size_t p = replystr.rfind("SM");
	if (p != string::npos) {
		mtr = 5 * atoi(&replystr[p + 3]);
		if (mtr > 100) mtr = 100;
	}
	return mtr;
}

// RM cmd 0 ... 100 (rig values 0 ... 8)
// User report of RM; command using Send Cmd tab
// RM10000;RM20000;RM30000;
// RM1nnnn; => SWR
// RM2nnnn; => COMP
// RM3nnnn; => ALC

int RIG_TS480HX::get_swr()
{
	int mtr = 0;
	cmd = "RM;";
	if (wait_char(';', 8, 100, "get SWR/ALC", ASC) != 8) return (int)mtr;

	size_t p = replystr.rfind("RM1");
	if (p != string::npos)
		mtr = 66 * atoi(&replystr[p + 3]) / 10;
	p = replystr.rfind("RM3");
	if (p != string::npos)
		alc = 66 * atoi(&replystr[p+3]) / 10;
	else
		alc = 0;
	swralc_polled = true;
	return mtr;
}

int  RIG_TS480HX::get_alc(void) 
{
	if (!swralc_polled) get_swr();
	swralc_polled = false;
	return alc;
}

// Tranceiver PTT on/off
void RIG_TS480HX::set_PTT_control(int val)
{
	if (val) {
		if (progStatus.data_port) cmd = "TX1;"; // DTS transmission using ANI input
		else cmd = "TX0;"; // mic input
	} else cmd = "RX;";
	sendCommand(cmd);
	showresp(WARN, ASC, "set PTT", cmd, "");
}

int RIG_TS480HX::set_widths(int val)
{
	int bw;
	if (val == 0 || val == 1 || val == 3) {
		if (menu_45) {
			bw = 0x8106; // 1500 Hz 2400 wide
			dsp_SL     = TS480HX_dataW;
			SL_tooltip = TS480HX_dataW_tooltip;
			SL_label   = TS480HX_dataW_label;
			dsp_SH     = TS480HX_dataC;
			SH_tooltip = TS480HX_dataC_tooltip;
			SH_label   = TS480HX_dataC_label;
			bandwidths_ = TS480HX_dataW;
			bw_vals_ = TS480HX_data_bw_vals;
		} else {
			bw = 0x8A03; // 200 ... 3000 Hz
			dsp_SL     = TS480HX_SL;
			SL_tooltip = TS480HX_SL_tooltip;
			SL_label   = TS480HX_btn_SL_label;
			dsp_SH     = TS480HX_SH;
			SH_tooltip = TS480HX_SH_tooltip;
			SH_label   = TS480HX_btn_SH_label;
			bandwidths_ = TS480HX_SH;
			bw_vals_ = TS480HX_HI_bw_vals;
		}
	} else if (val == 2 || val == 6) {
		bandwidths_ = TS480HX_CWwidths;
		bw_vals_ = TS480HX_CW_bw_vals;
		dsp_SL = TS480HX_empty;
		dsp_SH = TS480HX_empty;
		bw = 7;
	} else if (val == 5 || val == 7) {
		bandwidths_ = TS480HX_FSKwidths;
		bw_vals_ = TS480HX_FSK_bw_vals;
		dsp_SL = TS480HX_empty;
		dsp_SH = TS480HX_empty;
		bw = 1;
	} else { // val == 4 ==> AM
		bandwidths_ = TS480HX_empty;
		bw_vals_ = TS480HX_bw_vals;
		dsp_SL = TS480HX_AM_SL;
		dsp_SH = TS480HX_AM_SH;
		bw = 0x8201;
	}
	return bw;
}

const char **RIG_TS480HX::bwtable(int m)
{
	if (m == 0 || m == 1 || m == 3)
		return TS480HX_empty;
	else if (m == 2 || m == 6)
		return TS480HX_CWwidths;
	else if (m == 5 || m == 7)
		return TS480HX_FSKwidths;
//else AM m == 4
	return TS480HX_empty;
}

const char **RIG_TS480HX::lotable(int m)
{
	if (m == 0 || m == 1 || m == 3)
		return TS480HX_SL;
	else if (m == 2 || m == 6)
		return NULL;
	else if (m == 5 || m == 7)
		return NULL;
	return TS480HX_AM_SL;
}

const char **RIG_TS480HX::hitable(int m)
{
	if (m == 0 || m == 1 || m == 3)
		return TS480HX_SH;
	else if (m == 2 || m == 6)
		return NULL;
	else if (m == 5 || m == 7)
		return NULL;
	return TS480HX_AM_SH;
}

void RIG_TS480HX::set_modeA(int val)
{
	A.imode = val;
	cmd = "MD";
	cmd += TS480HX_mode_chr[val];
	cmd += ';';
	sendCommand(cmd);
	showresp(WARN, ASC, "set mode", cmd, "");
	A.iBW = set_widths(val);
}

int RIG_TS480HX::get_modeA()
{
	cmd = "MD;";
	if (wait_char(';', 14, 100, "get vfo A", ASC) < 14) return A.imode;

	size_t p = replystr.rfind("MD");
	if (p != string::npos && (p + 2 < replystr.length())) {
		int md = replystr[p+2];
		md = md - '1';
		if (md == 8) md = 7;
		A.imode = md;
		A.iBW = set_widths(A.imode);
	}
	return A.imode;
}

void RIG_TS480HX::set_modeB(int val)
{
	B.imode = val;
	cmd = "MD";
	cmd += TS480HX_mode_chr[val];
	cmd += ';';
	sendCommand(cmd);
	showresp(WARN, ASC, "set mode B", cmd, "");
	B.iBW = set_widths(val);
}

int RIG_TS480HX::get_modeB()
{
	cmd = "MD;";
	if (wait_char(';', 14, 100, "get vfo B", ASC) < 14) return B.imode;

	size_t p = replystr.rfind("MD");
	if (p != string::npos && (p + 2 < replystr.length())) {
		int md = replystr[p+2];
		md = md - '1';
		if (md == 8) md = 7;
		B.imode = md;
		B.iBW = set_widths(B.imode);
	}
	return B.imode;
}

int RIG_TS480HX::get_modetype(int n)
{
	return _mode_type[n];
}

void RIG_TS480HX::set_bwA(int val)
{
	if (A.imode == 0 || A.imode == 1 || A.imode == 3 || A.imode == 4) {
		if (val < 256) return;
		A.iBW = val;
		cmd = "SL";
		cmd.append(to_decimal(A.iBW & 0xFF, 2)).append(";");
		sendCommand(cmd);
		showresp(WARN, ASC, SL_tooltip, cmd, "");
		cmd = "SH";
		cmd.append(to_decimal(((A.iBW >> 8) & 0x7F), 2)).append(";");
		sendCommand(cmd);
		showresp(WARN, ASC, SH_tooltip, cmd, "");
	}
	if (val > 256) return;
	else if (A.imode == 2 || A.imode == 6) {
		A.iBW = val;
		cmd = TS480HX_CWbw[A.iBW];
		sendCommand(cmd);
		showresp(WARN, ASC, "set CW bw", cmd, "");
	}else if (A.imode == 5 || A.imode == 7) {
		A.iBW = val;
		cmd = TS480HX_FSKbw[A.iBW];
		sendCommand(cmd);
		showresp(WARN, ASC, "set FSK bw", cmd, "");
	}
}

int RIG_TS480HX::get_bwA()
{
	int i = 0;
	size_t p;

	bool menu45 = menu_45;

	check_menu_45();
	if (menu45 != menu_45)
		Fl::awake(updateBandwidthControl);

	if (A.imode == 0 || A.imode == 1 || A.imode == 3 || A.imode == 4) {
		int lo = A.iBW & 0xFF, hi = (A.iBW >> 8) & 0x7F;
		cmd = "SL;";
		if (wait_char(';', 5, 100, "get SL", ASC) == 5) {
			p = replystr.rfind("SL");
			if (p != string::npos)
				lo = fm_decimal(replystr.substr(p+2), 2);
		}
		cmd = "SH;";
		if (wait_char(';', 5, 100, "get SH", ASC) == 5) {
			p = replystr.rfind("SH");
			if (p != string::npos)
				hi = fm_decimal(replystr.substr(p+2), 2);
			A.iBW = ((hi << 8) | (lo & 0xFF)) | 0x8000;
		}
	} else if (A.imode == 2 || A.imode == 6) {
		cmd = "FW;";
		if (wait_char(';', 7, 100, "get FW", ASC) == 7) {
			p = replystr.rfind("FW");
			if (p != string::npos) {
				for (i = 0; i < 11; i++)
					if (replystr.find(TS480HX_CWbw[i]) == p)
						break;
				if (i == 11) i = 10;
				A.iBW = i;
			}
		}
	} else if (A.imode == 5 || A.imode == 7) {
		cmd = "FW;";
		if (wait_char(';', 7, 100, "get FW", ASC) == 7) {
			p = replystr.rfind("FW");
			if (p != string::npos) {
				for (i = 0; i < 4; i++)
					if (replystr.find(TS480HX_FSKbw[i]) == p)
						break;
				if (i == 4) i = 3;
				A.iBW = i;
			}
		}
	}
	return A.iBW;
}

void RIG_TS480HX::set_bwB(int val)
{
	if (B.imode == 0 || B.imode == 1 || B.imode == 3 || B.imode == 4) {
		if (val < 256) return;
		B.iBW = val;
		cmd = "SL";
		cmd.append(to_decimal(B.iBW & 0xFF, 2)).append(";");
		sendCommand(cmd);
		showresp(WARN, ASC, SL_tooltip, cmd, "");
		cmd = "SH";
		cmd.append(to_decimal(((B.iBW >> 8) & 0x7F), 2)).append(";");
		sendCommand(cmd);
		showresp(WARN, ASC, SH_tooltip, cmd, "");
	}
	if (val > 256) return;
	else if (B.imode == 2 || B.imode == 6) { // CW
		B.iBW = val;
		cmd = TS480HX_CWbw[B.iBW];
		sendCommand(cmd);
		showresp(WARN, ASC, "set CW bw", cmd, "");
	}else if (B.imode == 5 || B.imode == 7) {
		B.iBW = val;
		cmd = TS480HX_FSKbw[B.iBW];
		sendCommand(cmd);
		showresp(WARN, ASC, "set FSK bw", cmd, "");
	}
}

int RIG_TS480HX::get_bwB()
{
	int i = 0;
	size_t p;
	bool menu45 = menu_45;

	check_menu_45();
	if (menu45 != menu_45)
		Fl::awake(updateBandwidthControl);

	if (B.imode == 0 || B.imode == 1 || B.imode == 3 || B.imode == 4) {
		int lo = B.iBW & 0xFF, hi = (B.iBW >> 8) & 0x7F;
		cmd = "SL;";
		if (wait_char(';', 5, 100, "get SL", ASC) == 5) {
			p = replystr.rfind("SL");
			if (p != string::npos)
				lo = fm_decimal(replystr.substr(p+2), 2);
		}
		cmd = "SH;";
		if (wait_char(';', 5, 100, "get SH", ASC) == 5) {
			p = replystr.rfind("SH");
			if (p != string::npos)
				hi = fm_decimal(replystr.substr(p+2), 2);
			B.iBW = ((hi << 8) | (lo & 0xFF)) | 0x8000;
		}
	} else if (B.imode == 2 || B.imode == 6) {
		cmd = "FW;";
		if (wait_char(';', 7, 100, "get FW", ASC) == 7) {
			sendCommand(cmd);
			showresp(WARN, ASC, "get FW", cmd, replystr);
			p = replystr.rfind("FW");
			if (p != string::npos) {
				for (i = 0; i < 11; i++)
					if (replystr.find(TS480HX_CWbw[i]) == p)
						break;
				if (i == 11) i = 10;
				B.iBW = i;
			}
		}
	} else if (B.imode == 5 || B.imode == 7) {
		cmd = "FW;";
		if (wait_char(';', 7, 100, "get FW", ASC) == 7) {
			p = replystr.rfind("FW");
			if (p != string::npos) {
				for (i = 0; i < 4; i++)
					if (replystr.find(TS480HX_FSKbw[i]) == p)
						break;
				if (i == 4) i = 3;
				B.iBW = i;
			}
		}
	}
	return B.iBW;
}

int RIG_TS480HX::adjust_bandwidth(int val)
{
	int bw = 0;
	if (val == 0 || val == 1 || val == 3)
		bw = 0x8A03;
	else if (val == 4)
		bw = 0x8201;
	else if (val == 2 || val == 6)
		bw = 7;
	else if (val == 5 || val == 7)
		bw = 1;
	return bw;
}

int RIG_TS480HX::def_bandwidth(int val)
{
	return adjust_bandwidth(val);
}

void RIG_TS480HX::set_volume_control(int val)
{
	cmd = "AG";
	char szval[5];
	snprintf(szval, sizeof(szval), "%04d", val * 255 / 100);
	cmd += szval;
	cmd += ';';
	sendCommand(cmd);
	showresp(WARN, ASC, "set vol", cmd, "");
}

int RIG_TS480HX::get_volume_control()
{
	int val = progStatus.volume;
	cmd = "AG0;";
	if (wait_char(';', 7, 100, "get vol", ASC) < 7) return val;

	size_t p = replystr.rfind("AG");
	if (p == string::npos) return val;
	replystr[p + 6] = 0;
	val = atoi(&replystr[p + 3]);
	val = val * 100 / 255;
	return val;
}

void RIG_TS480HX::set_power_control(double val)
{
	cmd = "PC";
	char szval[4];
	if (modeA == 4 && val > 50) val = 50; // AM mode limitation
	snprintf(szval, sizeof(szval), "%03d", (int)val);
	cmd += szval;
	cmd += ';';
	sendCommand(cmd);
	showresp(WARN, ASC, "set power", cmd, "");
}

int RIG_TS480HX::get_power_control()
{
	int val = progStatus.power_level;
	cmd = "PC;";
	if (wait_char(';', 6, 100, "get power", ASC) < 6) return val;

	size_t p = replystr.rfind("PC");
	if (p == string::npos) return val;

	val = atoi(&replystr[p + 2]);

	return val;
}

void RIG_TS480HX::set_attenuator(int val)
{
	if (val)	cmd = "RA01;";
	else		cmd = "RA00;";
	LOG_WARN("%s", cmd.c_str());
	sendCommand(cmd);
}

int RIG_TS480HX::get_attenuator()
{
	cmd = "RA;";
	if (wait_char(';', 7, 100, "get attenuator", ASC) < 7) return progStatus.attenuator;

	size_t p = replystr.rfind("RA");
	if (p == string::npos) return progStatus.attenuator;
	if (replystr[p+3] == '1') return 1;
	return 0;
}

void RIG_TS480HX::set_preamp(int val)
{
	if (val)	cmd = "PA1;";
	else		cmd = "PA0;";
	LOG_WARN("%s", cmd.c_str());
	sendCommand(cmd);
}

int RIG_TS480HX::get_preamp()
{
	cmd = "PA;";
	if (wait_char(';', 5, 100, "get preamp", ASC)  < 5) return progStatus.preamp;

	size_t p = replystr.rfind("PA");
	if (p == string::npos) return progStatus.preamp;
	if (replystr[p+2] == '1') return 1;
	return 0;
}

void RIG_TS480HX::tune_rig()
{
	cmd = "AC111;";
	sendCommand(cmd);
	showresp(WARN, ASC, "tune", cmd, "");
}

void RIG_TS480HX::set_if_shift(int val)
{
	cmd = "IS+";
	if (val < 0) cmd[2] = '-';
	cmd.append(to_decimal(abs(val),4)).append(";");
	sendCommand(cmd);
	showresp(WARN, ASC, "set IF shift", cmd, "");
}

bool RIG_TS480HX::get_if_shift(int &val)
{
	cmd = "IS;";
	if (wait_char(';', 8, 100, "get IF shift", ASC) == 8) {
		size_t p = replystr.rfind("IS");
		if (p != string::npos) {
			val = fm_decimal(replystr.substr(p+3), 4);
			if (replystr[p+2] == '-') val *= -1;
			return (val != 0);
		}
	}
	val = progStatus.shift_val;
	return progStatus.shift;
}

void RIG_TS480HX::get_if_min_max_step(int &min, int &max, int &step)
{
	if_shift_min = min = -1100;
	if_shift_max = max = 1100;
	if_shift_step = step = 10;
	if_shift_mid = 0;
}

void RIG_TS480HX::set_mic_gain(int val)
{
	cmd = "MG";
	cmd.append(to_decimal(val,3)).append(";");
	sendCommand(cmd);
	showresp(WARN, ASC, "set mic gain", cmd, "");
}

int  RIG_TS480HX::get_mic_gain()
{
	int val = progStatus.mic_gain;
	cmd = "MG;";
	if (wait_char(';', 6, 100, "get mic gain", ASC) < 6) return val;

	size_t p = replystr.rfind("MG");
	if (p != string::npos)
		val = fm_decimal(replystr.substr(p+2), 3);
	return val;
}

void RIG_TS480HX::get_mic_min_max_step(int &min, int &max, int &step)
{
	min = 0; max = 100; step = 1;
}

void RIG_TS480HX::set_rf_gain(int val)	
{
	cmd = "RG";
	cmd.append(to_decimal(val,3)).append(";");
	sendCommand(cmd);
	showresp(WARN, ASC, "set rf gain", cmd, "");
}

int  RIG_TS480HX::get_rf_gain()
{
	int val = progStatus.rfgain;
	cmd = "RG;";
	if (wait_char(';', 6, 100, "get rf gain", ASC) < 6) return val;

	size_t p = replystr.rfind("RG");
	if (p != string::npos)
		val = fm_decimal(replystr.substr(p+2), 3);
	return val;
}

void RIG_TS480HX::get_rf_min_max_step(int &min, int &max, int &step)
{
	min = 0; max = 100; step = 1;
}

