/*
 * Icom 7200, 7700 ... drivers
 *
 * a part of flrig
 *
 * Copyright 2009, Dave Freese, W1HKJ
 *
 */

#include "IC7200.h"

bool DEBUG_7200 = true;

//=============================================================================
// IC-7200

const char IC7200name_[] = "IC-7200";
const char *IC7200modes_[] = { "LSB", "USB", "AM", "CW", "RTTY", "CW-R", "RTTY-R", NULL};
const char IC7200_mode_type[] = { 'L', 'U', 'U', 'L', 'L', 'U', 'U'};
const char *IC7200_SSBwidths[] = {
  "50",  "100",  "150",  "200",  "250",  "300",  "350",  "400",  "450",  "500",
"600",   "700",  "800",  "900", "1000", "1100", "1200", "1300", "1400", "1500",
"1600", "1700", "1800", "1900", "2000", "2100", "2200", "2300", "2400", "2500",
"2600", "2700", "2800", "2900", "3000", "3100", "3200", "3300", "3400", "3500",
"3600",
NULL};

const char *IC7200_RTTYwidths[] = {
  "50",  "100",  "150",  "200",  "250",  "300",  "350",  "400",  "450",  "500",
 "600",  "700",  "800",  "900", "1000", "1100", "1200", "1300", "1400", "1500",
"1600", "1700", "1800", "1900", "2000", "2100", "2200", "2300", "2400", "2500",
"2600", "2700",
NULL};

const char *IC7200_AMwidths[] = {
  "200",  "400",  "600",  "800", "1000", "1200", "1400", "1600", "1800", "2000",
 "2200", "2400", "2600", "2800", "3000", "3200", "3400", "3600", "3800", "4000",
 "4200", "4400", "4600", "4800", "5000", "5200", "5400", "5600", "5800", "5000",
 "6200", "6400", "6600", "6800", "6000", "6200", "6400", "6600", "6800", "6000",
NULL};


//======================================================================
// IC7200 unique commands
//======================================================================

RIG_IC7200::RIG_IC7200() {
	name_ = IC7200name_;
	modes_ = IC7200modes_;
	_mode_type = IC7200_mode_type;
	bandwidths_ = IC7200_SSBwidths;

	comm_baudrate = BR9600;
	stopbits = 1;
	comm_retries = 2;
	comm_wait = 5;
	comm_timeout = 50;
	comm_echo = true;
	comm_rtscts = false;
	comm_rtsplus = true;
	comm_dtrplus = true;
	comm_catptt = true;
	comm_rtsptt = false;
	comm_dtrptt = false;
	mode_ = 1;
	bw_ = 0;
	filter_nbr = 1;

	has_power_control = true;
	has_volume_control = true;
	has_mode_control = true;
	has_bandwidth_control = true;
	has_micgain_control = true;
	has_attenuator_control = true;
	has_preamp_control = true;
	has_noise_control = true;
	has_noise_reduction = true;
	has_noise_reduction_control = true;
	has_auto_notch = true;
	has_rf_control = true;
	has_compON = true;
	has_vox_onoff = true;
	has_ptt_control = true;

	defaultCIV = 0x76;
	adjustCIV(defaultCIV);
};

//=============================================================================

long RIG_IC7200::get_vfoA ()
{
	cmd = pre_to;
	cmd += '\x03';
	cmd.append( post );
	if (!sendCommand(cmd, 11)) {
		checkresponse(11);
		return freq_;
	}
	freq_ = fm_bcd_be(&replystr[5], 10);
	return freq_;
}

void RIG_IC7200::set_vfoA (long freq)
{
	freq_ = freq;
	cmd = pre_to;
	cmd += '\x05';
	cmd.append( to_bcd_be( freq, 10 ) );
	cmd.append( post );
	sendICcommand(cmd, 6);
	checkresponse(6);
	if (DEBUG_7200)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

// Tranceiver PTT on/off
void RIG_IC7200::set_PTT_control(int val)
{
	cmd = pre_to;
	cmd += '\x1c';
	cmd += '\x00';
	cmd += (unsigned char) val;
	cmd.append( post );
	sendICcommand (cmd, 6);
	checkresponse(6);
	if (RIG_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}


// Volume control val 0 ... 100
void RIG_IC7200::set_volume_control(int val)
{
	int ICvol = (int)(val * 255 / 100);
	cmd = pre_to;
	cmd.append("\x14\x01");
	cmd.append(to_bcd(ICvol, 3));
	cmd.append( post );
	sendICcommand (cmd, 6);
	checkresponse(6);
	if (DEBUG_7200)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

int RIG_IC7200::get_volume_control()
{
	cmd = pre_to;
	cmd.append("\x14\x01");
	cmd.append( post );
	if(sendICcommand (cmd, 9))
		return ((int)(fm_bcd(&replystr[6],3) * 100 / 255));
	checkresponse(9);
	return 0;
}

void RIG_IC7200::get_vol_min_max_step(int &min, int &max, int &step)
{
	min = 0; max = 100; step = 1;
}

int RIG_IC7200::get_smeter()
{
	cmd = pre_to;
	cmd.append("\x15\x02");
	cmd.append( post );
	if (sendICcommand (cmd, 9)) {
		int mtr = fm_bcd(&replystr[6], 3);
		mtr = (int)(1.3 * mtr - 36.0);
		if (mtr < 0) mtr = 0;
		if (mtr > 255) mtr = 255;
		return (mtr * 100 / 255);
	} else
		return 0;
}

void RIG_IC7200::set_attenuator(int val)
{
	cmd = pre_to;
	cmd += '\x11';
	cmd += val ? '\x20' : '\x00';
	cmd.append( post );
	sendCommand (cmd, 6);
	checkresponse(6);
	if (DEBUG_7200)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

void RIG_IC7200::set_noise(bool val)
{
	cmd = pre_to;
	cmd.append("\x16\x22");
	cmd += val ? 1 : 0;
	cmd.append(post);
	sendICcommand(cmd, 6);
	checkresponse(6);
	if (DEBUG_7200)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

int RIG_IC7200::get_noise()
{
	cmd = pre_to;
	cmd.append("\x16\x22");
	cmd.append(post);
	if (sendICcommand(cmd, 8))
		return (replystr[6] ? 1 : 0);
	return 0;
}

void RIG_IC7200::set_noise_reduction(int val)
{
	cmd = pre_to;
	cmd.append("\x16\x40");
	cmd += val ? 1 : 0;
	cmd.append(post);
	sendICcommand(cmd, 6);
	checkresponse(6);
	if (DEBUG_7200)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

int RIG_IC7200::get_noise_reduction()
{
	cmd = pre_to;
	cmd.append("\x16\x40");
	cmd.append(post);
	if (sendICcommand(cmd, 8))
		return (replystr[6] ? 1 : 0);
	return 0;
}

// 0 < val < 100
void RIG_IC7200::set_noise_reduction_val(int val)
{
	cmd = pre_to;
	cmd.append("\x14\x06");
	cmd.append(to_bcd(val * 255 / 100, 3));
	cmd.append(post);
	sendICcommand(cmd,6);
	checkresponse(6);
	if (DEBUG_7200)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

int RIG_IC7200::get_noise_reduction_val()
{
	cmd = pre_to;
	cmd.append("\x14\x06");
	cmd.append(post);
	if(sendICcommand (cmd, 9))
		return ((int)(fm_bcd(&replystr[6],3) * 100 / 255));
	checkresponse(9);
	return 0;
}

void RIG_IC7200::set_preamp(int val)
{
	cmd = pre_to;
	cmd += '\x16';
	cmd += '\x02';
	cmd += (unsigned char)val;
	cmd.append( post );
	sendICcommand (cmd, 6);
	checkresponse(6);
	if (DEBUG_7200)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

int RIG_IC7200::get_preamp()
{
	cmd = pre_to;
	cmd += '\x16';
	cmd += '\x02';
	cmd.append( post );
	if (sendICcommand (cmd, 8)) {
		if (replystr[6] == 0x01) {
			preamp_label("Pre", true);
		} else {
			preamp_label("Pre", false);
		}
	}
	return replystr[6];
}

void RIG_IC7200::set_rf_gain(int val)
{
	int ICrfg = (int)(val * 255 / 100);
	cmd = pre_to;
	cmd.append("\x14\x02");
	cmd.append(to_bcd(ICrfg, 3));
	cmd.append( post );
	sendICcommand (cmd, 6);
	checkresponse(6);
	if (DEBUG_7200)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

int RIG_IC7200::get_rf_gain()
{
	cmd = pre_to;
	cmd.append("\x14\x02").append(post);
	if (sendICcommand (cmd, 9))
		return ((int)(fm_bcd(&replystr[6],3) * 100 / 255));
	checkresponse(9);
	return 0;
}

void RIG_IC7200::set_squelch(int val)
{
	int ICsql = (int)(val * 255 / 100);
	cmd = pre_to;
	cmd.append("\x14\x03");
	cmd.append(to_bcd(ICsql, 3));
	cmd.append( post );
	sendICcommand (cmd, 6);
	checkresponse(6);
	if (DEBUG_7200)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

void RIG_IC7200::set_power_control(double val)
{
	cmd = pre_to;
	cmd.append("\x14\x0A");
	cmd.append(to_bcd((int)(val * 255 / 100), 3));
	cmd.append( post );
	sendICcommand (cmd, 6);
	checkresponse(6);
	if (DEBUG_7200)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

void RIG_IC7200::set_mic_gain(int val)
{
	val = (int)(val * 255 / 100);
	cmd = pre_to;
	cmd.append("\x14\x0B");
	cmd.append(to_bcd(val,3));
	cmd.append(post);
	sendICcommand(cmd, 6);
	checkresponse(6);
	if (DEBUG_7200)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

void RIG_IC7200::set_mode(int val)
{
	mode_ = val;
	cmd = pre_to;
	cmd += '\x06';
	cmd += val > 5 ? val + 2 : val;
	cmd += filter_nbr;
	cmd.append( post );
	sendICcommand (cmd, 6);
	checkresponse(6);
	if (RIG_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

int RIG_IC7200::get_mode()
{
	cmd = pre_to;
	cmd += '\x04';
	cmd.append(post);
	if (sendICcommand (cmd, 8 )) {
		mode_ = replystr[5];
		if (mode_ > 6) mode_ -= 2;
		filter_nbr = replystr[6];
	}
	return mode_;
}

int RIG_IC7200::get_modetype(int n)
{
	return _mode_type[n];
}

int RIG_IC7200::adjust_bandwidth(int m)
{
	if (m == 0 || m == 1 ) { //SSB
		bandwidths_ = IC7200_SSBwidths;
		return (bw_ = 32);
	}
	if (m == 3 || m == 5) { //CW
		bandwidths_ = IC7200_SSBwidths;
		return (bw_ = 14);
	}
	if (m == 4 || m == 7) { //RTTY
		bandwidths_ = IC7200_RTTYwidths;
		return (bw_ = 28);
	}
	bandwidths_ = IC7200_AMwidths;
	return (bw_ = 0);
}

void RIG_IC7200::set_bandwidth(int val)
{
	bw_ = val;
	cmd = pre_to;
	cmd.append("\x1A\x02");
	cmd.append(to_bcd(val, 2));
	cmd.append( post );
	sendICcommand(cmd, 6);
	checkresponse(6);
	if (RIG_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

int  RIG_IC7200::get_bandwidth()
{
	cmd = pre_to;
	cmd += "\x1A\x02";
	cmd.append( post );
	if (sendICcommand(cmd, 8))
		bw_ = (fm_bcd(&replystr[6],2));
	return bw_;
}

void RIG_IC7200::set_auto_notch(int val)
{
	cmd = pre_to;
	cmd += '\x16';
	cmd += '\x41';
	cmd += (unsigned char)val;
	cmd.append( post );
	sendICcommand (cmd, 6);
	checkresponse(6);
	if (DEBUG_7200)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

int RIG_IC7200::get_auto_notch()
{
	cmd = pre_to;
	cmd += '\x16';
	cmd += '\x41';
	cmd.append( post );
	if (sendICcommand (cmd, 8)) {
		if (replystr[6] == 0x01)
			auto_notch_label("AN", true);
		else
			auto_notch_label("AN", false);
	}
	return replystr[6];
}

void RIG_IC7200::set_compression()
{
	if (progStatus.compON) {
		cmd = pre_to;
		cmd.append("\x16\x44");
		cmd += '\x01';
		cmd.append(post);
		sendICcommand(cmd, 6);
		checkresponse(6);
	} else {
		cmd = pre_to;
		cmd.append("\x16\x44");
		cmd += '\x00';
		cmd.append(post);
		sendICcommand(cmd, 6);
		checkresponse(6);
	}
	if (RIG_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

void RIG_IC7200::set_vox_onoff()
{
	if (progStatus.vox_onoff) {
		cmd = pre_to;
		cmd.append("\x16\x46");
		cmd += '\x01';
		cmd.append(post);
		sendICcommand(cmd, 6);
		checkresponse(6);
	} else {
		cmd = pre_to;
		cmd.append("\x16\x46");
		cmd += '\x00';
		cmd.append(post);
		sendICcommand(cmd, 6);
		checkresponse(6);
	}
	if (RIG_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

