/*
 * Icom IC-746, 746PRO
 * 
 * a part of flrig
 *
 * Copyright 2009, Dave Freese, W1HKJ
 *
 */

#include "IC746.h"

//=============================================================================
// IC-746, IC746PRO
//=============================================================================

const char IC746name_[] = "IC-746";

const char *IC746modes_[] = {
		"LSB", "USB", "AM", "CW", "RTTY", "FM", "CW-R", "RTTY-R", NULL};
// mode values are 0, 1, 2, 3, 4, 5, 7, 8
const char IC746_mode_type[] =
	{ 'L', 'U', 'U', 'U', 'L', 'U', 'L', 'U'};

const char *IC746_widths[] = { "NORM", "NARR", NULL};

RIG_IC746::RIG_IC746() {
	defaultCIV = 0x56;
	name_ = IC746name_;
	modes_ = IC746modes_;
	bandwidths_ = IC746_widths;
	_mode_type = IC746_mode_type;

	comm_baudrate = BR9600;
	stopbits = 2;
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
	deffreq_ = 14070000L;
	def_mode = 1;
	defbw_ = 0;
	filter_nbr = 1;
	ICvol = 0;

	has_power_control = true;
	has_volume_control = true;
	has_mode_control = true;
	has_bandwidth_control = true;
	has_micgain_control = true;
	has_notch_control = true;
	has_attenuator_control = true;
	has_preamp_control = true;
	has_ifshift_control = true;
	has_ptt_control = true;
	has_tune_control = true;
	has_swr_control = true;
	has_noise_control = true;
	has_noise_reduction = true;
	has_noise_reduction_control = true;
	has_alc_control = true;
	has_rf_control = true;
	has_sql_control = true;
	restore_mbw = true;

};

//=============================================================================
// 746PRO
const char IC746PROname_[] = "IC-746PRO";

const char *IC746PROmodes_[] = {
		"LSB", "USB", "AM", "CW", "RTTY", "FM", "CW-R", "RTTY-R",
		"D-LSB", "D-USB", "D-FM", NULL};

const char IC746PRO_mode_type[] =
	{ 'L', 'U', 'U', 'U', 'L', 'U', 'L', 'U',
	  'L', 'U', 'U' };

const char *IC746PRO_SSBwidths[] = {
  "50",  "100",  "150",  "200",  "250",  "300",  "350",  "400",  "450",  "500",
"600",   "700",  "800",  "900", "1000", "1100", "1200", "1300", "1400", "1500",
"1600", "1700", "1800", "1900", "2000", "2100", "2200", "2300", "2400", "2500",
"2600", "2700", "2800", "2900", "3000", "3100", "3200", "3300", "3400", "3500",
"3600",
NULL};

const char *IC746PRO_RTTYwidths[] = {
  "50",  "100",  "150",  "200",  "250",  "300",  "350",  "400",  "450",  "500",
 "600",  "700",  "800",  "900", "1000", "1100", "1200", "1300", "1400", "1500",
"1600", "1700", "1800", "1900", "2000", "2100", "2200", "2300", "2400", "2500",
"2600", "2700",
NULL};

const char *IC746PRO_AMFMwidths[] = { "FILT-1", "FILT-2", "FILT-3", NULL };

RIG_IC746PRO::RIG_IC746PRO() {
	defaultCIV = 0x66;
	name_ = IC746PROname_;
	modes_ = IC746PROmodes_;
	bandwidths_ = IC746PRO_SSBwidths;
	_mode_type = IC746PRO_mode_type;
	atten_level = 0;
	preamp_level = 0;
	def_mode = 9;
	defbw_ = 32;
	deffreq_ = 14070000;
	adjustCIV(defaultCIV);
};

long RIG_IC746::get_vfoA ()
{
	cmd = pre_to;
	cmd += '\x03';
	cmd.append( post );
	if (!sendICcommand(cmd, 11))
		return freq_;
	freq_ = fm_bcd_be(&replystr[5], 10);
	return freq_;
}

void RIG_IC746::set_vfoA (long freq)
{
	freq_ = freq;
	cmd = pre_to;
	cmd += '\x05';
	cmd.append( to_bcd_be( freq, 10 ) );
	cmd.append( post );
	sendICcommand(cmd, 6);
	checkresponse(6);
	if (RIG_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

int RIG_IC746::get_smeter()
{
	cmd = pre_to;
	cmd.append("\x15\x02");
	cmd.append( post );
	if (sendICcommand (cmd, 9)) {
		return fm_bcd(&replystr[6], 3) / 2.55;
	} else
		return 0;
}

// Volume control val 0 ... 100

void RIG_IC746::set_volume_control(int val)
{
	ICvol = (int)(val);
	cmd = pre_to;
	cmd.append("\x14\x01");
	cmd.append(to_bcd(ICvol, 3));
	cmd.append( post );
	sendICcommand (cmd, 6);
	checkresponse(6);
	if (RIG_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

int RIG_IC746::get_volume_control()
{
	cmd = pre_to;
	cmd.append("\x14\x01");
	cmd.append( post );
	if(sendICcommand (cmd, 9))
		return ((int)(fm_bcd(&replystr[6],3)));
	checkresponse(9);
	return ICvol;
}

void RIG_IC746::get_vol_min_max_step(int &min, int &max, int &step)
{
	min = 0; max = 255; step = 1;
}

// Tranceiver PTT on/off
void RIG_IC746::set_PTT_control(int val)
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

void RIG_IC746::set_attenuator(int val)
{
	cmd = pre_to;
	cmd += '\x11';
	cmd += val ? 0x20 : 0x00;
	cmd.append( post );
	sendICcommand(cmd,6);
	checkresponse(6);
	if (!val) {
		preamp_level--;
		if (preamp_level < 0) preamp_level = 2;
		set_preamp(1);
	}
	if (RIG_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

int RIG_IC746::get_attenuator()
{
	cmd = pre_to;
	cmd += '\x11';
	cmd.append( post );
	if (sendICcommand(cmd,7))
		return (replystr[5] ? 1 : 0);
	return 0;
}

void RIG_IC746::set_preamp(int val)
{
	if (preamp_level == 0) {
		preamp_level = 1;
		preamp_label("Pre 1", true);
	} else if (preamp_level == 1) {
		preamp_level = 2;
		preamp_label("Pre 2", true);
	} else if (preamp_level == 2) {
		preamp_level = 0;
		preamp_label("Pre", false);
	}
	cmd = pre_to;
	cmd += '\x16';
	cmd += '\x02';
	cmd += (unsigned char) preamp_level;
	cmd.append( post );
	sendICcommand (cmd, 6);
	checkresponse(6);
	if (RIG_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

int RIG_IC746::get_preamp()
{
	cmd = pre_to;
	cmd += '\x16';
	cmd += '\x02';
	cmd.append( post );
	if (sendICcommand (cmd, 8)) {
		if (replystr[6] == 0x01) {
			preamp_label("Pre 1", true);
			preamp_level = 1;
		} else if (replystr[6] == 0x02) {
			preamp_label("Pre 2", true);
			preamp_level = 2;
		} else {
			preamp_label("Pre", false);
			preamp_level = 0;
		}
	}
	return preamp_level;
}

// changed noise blanker to noise reduction
void RIG_IC746::set_noise(bool val)
{
	cmd = pre_to;
	cmd.append("\x16\x22");
	cmd += val ? 1 : 0;
	cmd.append(post);
	sendICcommand(cmd, 6);
	checkresponse(6);
	if (RIG_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

int RIG_IC746::get_noise()
{
	cmd = pre_to;
	cmd.append("\x16\x22");
	cmd.append(post);
	if (sendICcommand(cmd, 8))
		return (replystr[6] ? 1 : 0);
	return 0;
}

void RIG_IC746::set_noise_reduction(int val)
{
	cmd = pre_to;
	cmd.append("\x16\x40");
	cmd += val ? 1 : 0;
	cmd.append(post);
	sendICcommand(cmd, 6);
	checkresponse(6);
	if (RIG_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

int RIG_IC746::get_noise_reduction()
{
	cmd = pre_to;
	cmd.append("\x16\x40");
	cmd.append(post);
	if (sendICcommand(cmd, 8))
		return (replystr[6] ? 1 : 0);
	return 0;
}

// 0 < val < 100
void RIG_IC746::set_noise_reduction_val(int val)
{
	cmd = pre_to;
	cmd.append("\x14\x06");
	cmd.append(to_bcd(val * 255 / 100, 3));
	cmd.append(post);
	sendICcommand(cmd,6);
	checkresponse(6);
	if (RIG_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

int RIG_IC746::get_noise_reduction_val()
{
	cmd = pre_to;
	cmd.append("\x14\x06");
	cmd.append(post);
	if(sendICcommand (cmd, 9))
		return ((int)(fm_bcd(&replystr[6],3) / 2.55));
	checkresponse(9);
	return 0;
}

void RIG_IC746::set_mode(int val)
{
	mode_ = val;
	cmd = pre_to;
	cmd += '\x06';
	cmd += val > 5 ? val + 1 : val;
	cmd += filter_nbr; // filter #1
	cmd.append( post );
	sendICcommand (cmd, 6);
	checkresponse(6);
	if (RIG_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

int RIG_IC746::get_mode()
{
	cmd = pre_to;
	cmd += '\x04';
	cmd.append(post);
	if (sendICcommand (cmd, 8 )) {
		mode_ = replystr[5];
		if (mode_ > 6) mode_--;
		filter_nbr = replystr[6];
	}
	return mode_;
}

void RIG_IC746::set_bandwidth(int val)
{
	filter_nbr = val + 1;
	set_mode(mode_);
	if (RIG_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

int RIG_IC746::get_modetype(int n)
{
	return _mode_type[n];
}

int RIG_IC746::get_bandwidth()
{
	return filter_nbr - 1;
}

void RIG_IC746::set_mic_gain(int val)
{
	val = (int)(val * 255 / 100);
	cmd = pre_to;
	cmd.append("\x14\x0B");
	cmd.append(to_bcd(val,3));
	cmd.append(post);
	sendICcommand(cmd, 6);
	checkresponse(6);
	if (RIG_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

void RIG_IC746::get_mic_gain_min_max_step(int &min, int &max, int &step)
{
	min = 0;
	max = 100;
	step = 1;
}

void RIG_IC746::set_if_shift(int val)
{
	int shift = (int)((val + 50) * 2.56 );
	if (shift == 256) shift = 255;
	cmd = pre_to;
	cmd.append("\x14\x07");
	cmd.append(to_bcd(shift, 3));
	cmd.append(post);
	sendICcommand(cmd,6);
	if (RIG_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
	checkresponse(6);

	cmd = pre_to;
	cmd.append("\x14\x08");
	cmd.append(to_bcd(shift, 3));
	cmd.append(post);
	sendICcommand(cmd,6);
	if (RIG_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
	checkresponse(6);
}

void RIG_IC746::get_if_min_max_step(int &min, int &max, int &step)
{
	min = -50;
	max = +50;
	step = 2;
}

int ICsql = 0;
void RIG_IC746::set_squelch(int val)
{
	ICsql = (int)(val * 255 / 100);
	cmd = pre_to;
	cmd.append("\x14\x03");
	cmd.append(to_bcd(ICsql, 3));
	cmd.append( post );
	sendICcommand (cmd, 6);
	checkresponse(6);
	if (RIG_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

int ICrfg = 0;
void RIG_IC746::set_rf_gain(int val)
{
	ICrfg = (int)(val * 255 / 100);
	cmd = pre_to;
	cmd.append("\x14\x02");
	cmd.append(to_bcd(ICrfg, 3));
	cmd.append( post );
	sendICcommand (cmd, 6);
	checkresponse(6);
	if (RIG_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

void RIG_IC746::set_power_control(double val)
{
	cmd = pre_to;
	cmd.append("\x14\x0A");
	cmd.append(to_bcd((int)(val * 255 / 100), 3));
	cmd.append( post );
	sendICcommand (cmd, 6);
	checkresponse(6);
	if (RIG_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

//int RIG_IC746::get_mic_gain()
//{
//	cmd = pre_to;
//	cmd.append("\x14\x0B");
//	cmd.append(post);
//	if (sendICcommand (cmd, 9))
//		return ((int)(fm_bcd(&replystr[6],3) / 2.55));
//	checkresponse(9);
//	return 0;
//}

//bool RIG_IC746::get_if_shift(int val)
//{
//	val = 0;
//	cmd = pre_to;
//	cmd.append("\x14\x07");
//	cmd.append(post);
//	if (sendICcommand(cmd,9)) {
//		val = (int)((fm_bcd(&replystr[6],3) - 128) * 50 / 128.0);
//		return true;
//	}
//	return false;
//}

//int  RIG_IC746::get_squelch()
//{
//	cmd = pre_to;
//	cmd.append("\x14\x03");
//	cmd.append( post );
//	if(sendICcommand (cmd, 9))
//		return ((int)(fm_bcd(&replystr[6],3) / 2.55));
//	checkresponse(9);
//	return ICvol;
//	return 0;
//}

//int  RIG_IC746::get_rf_gain()
//{
//	cmd = pre_to;
//	cmd.append("\x14\x02");
//	cmd.append( post );
//	if(sendICcommand (cmd, 9))
//		return ((int)(fm_bcd(&replystr[6],3) / 2.55));
//	checkresponse(9);
//	return ICvol;
//	return 0;
//}

//======================================================================
// IC746PRO unique commands
//======================================================================

void RIG_IC746PRO::set_mode(int val)
{
	mode_ = val;
	bool datamode = false;
	switch (val) {
		case 10 : val = 5; datamode = true; break;
		case 9  : val = 1; datamode = true; break;
		case 8  : val = 0; datamode = true; break;
		case 7  : val = 8; break;
		case 6  : val = 7; break;
		default: break;
	}
	cmd = pre_to;
	cmd += '\x06';
	cmd += val;
	cmd += filter_nbr;
	cmd.append( post );
	sendICcommand (cmd, 6);
	if (RIG_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
	checkresponse(6);
	if (datamode) { // LSB / USB ==> use DATA mode
		cmd = pre_to;
		cmd.append("\x1A\x06\x01");
		cmd.append(post);
		sendICcommand(cmd, 6);
		checkresponse(6);
	if (RIG_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
	}
}

int RIG_IC746PRO::get_mode()
{
	int md;
	cmd = pre_to;
	cmd += '\x04';
	cmd.append(post);
	if (sendICcommand (cmd, 8 )) {
		md = replystr[5];
		if (md > 6) md--;
		filter_nbr = replystr[6];
		cmd = pre_to;
		cmd.append("\x1A\x06");
		cmd.append(post);
		if (sendICcommand(cmd, 8)) {
			if (replystr[6]) {
				switch (md) {
					case 0 : md = 8; break;
					case 1 : md = 9; break;
					case 5 : md = 10; break;
					default : break;
				}
			}
			mode_ = md;
		} else {
			checkresponse(8);
		}
	} else {
		checkresponse(8);
	}
	return mode_;
}

int RIG_IC746PRO::adjust_bandwidth(int m)
{
	if (m == 0 || m == 1 || m == 8 || m == 9) { //SSB
		bandwidths_ = IC746PRO_SSBwidths;
		return (bw_ = 32);
	}
	if (m == 3 || m == 6) { //CW
		bandwidths_ = IC746PRO_SSBwidths;
		return (bw_ = 14);
	}
	if (m == 4 || m == 7) { //RTTY
		bandwidths_ = IC746PRO_RTTYwidths;
		return (bw_ = 28);
	}
	bandwidths_ = IC746PRO_AMFMwidths;
	filter_nbr = 1;
	return (bw_ = 0);
}

int RIG_IC746PRO::get_swr()
{
	cmd = pre_to;
	cmd.append("\x15\x12");
	cmd.append( post );
	sendICcommand (cmd, 9);
	return fm_bcd(&replystr[6],3) / 3.6;
}

int RIG_IC746PRO::get_alc()
{
	cmd = pre_to;
	cmd.append("\x15\x13");
	cmd.append( post );
	sendICcommand (cmd, 9);
	return fm_bcd(&replystr[6],3);
}

// Transceiver power level return power in watts
int RIG_IC746PRO::get_power_out()
{
	cmd = pre_to;
	cmd.append("\x15\x11");
	cmd.append( post );
	if (sendICcommand (cmd, 9)) {
		int pwr = (int)(fm_bcd(&replystr[6],3) / 2.55 );
		return pwr;
	}
	return 0;
}

void RIG_IC746PRO::tune_rig()
{
	cmd = pre_to;
	cmd.append("\x1c\x01\x02");
	cmd.append( post );
	sendICcommand (cmd, 6);
	checkresponse(6);
	if (RIG_DEBUG)
		if (RIG_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

void RIG_IC746PRO::set_bandwidth(int val)
{
	if (bandwidths_ == IC746PRO_AMFMwidths) {
		filter_nbr = val + 1;
		set_mode(mode_);
		return;
	}

	bw_ = val;
	cmd = pre_to;
	cmd.append("\x1A\x03");
	cmd.append(to_bcd(val, 2));
	cmd.append( post );
	sendICcommand(cmd, 6);
	checkresponse(6);
	if (RIG_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

int  RIG_IC746PRO::get_bandwidth()
{
	if (bandwidths_ == IC746PRO_AMFMwidths) {
		bw_ = filter_nbr - 1;
		return bw_;
	}
	cmd = pre_to;
	cmd += "\x1A\x03";
	cmd.append( post );
	if (sendICcommand(cmd, 8))
		bw_ = (fm_bcd(&replystr[6],2));
	return bw_;
}

int RIG_IC746PRO::get_attenuator()
{
	cmd = pre_to;
	cmd += '\x11';
	cmd.append( post );
	if (sendICcommand(cmd,7))
		return (replystr[5] ? 1 : 0);
	return 0;
}

void RIG_IC746PRO::set_attenuator(int val)
{
	int cmdval = val ? 0x20 : 0x00;
	cmd = pre_to;
	cmd += '\x11';
	cmd += cmdval;
	cmd.append( post );
	sendICcommand(cmd,6);
	checkresponse(6);
	if (RIG_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));
}

bool IC_notchon = false;
void RIG_IC746PRO::set_notch(bool on, int val)
{
	int notch = (int)(val/20.0 + 128);
	if (notch > 256) notch = 255;
	if (on != IC_notchon) {
		cmd = pre_to;
		cmd.append("\x16\x48");
		cmd += on ? '\x01' : '\x00';
		cmd.append(post);
		sendICcommand(cmd,6);
		checkresponse(6);
		IC_notchon = on;
	}

	if (on) {
		cmd = pre_to;
		cmd.append("\x14\x0D");
		cmd.append(to_bcd(notch,3));
		cmd.append(post);
		sendICcommand(cmd,6);
		checkresponse(6);
	}
	if (RIG_DEBUG)
		LOG_INFO("%s", str2hex(cmd.data(), cmd.length()));

}

bool RIG_IC746PRO::get_notch(int &val)
{
	bool on = false;
	val = 0;
	cmd = pre_to;
	cmd.append("\x16\x48");
	cmd.append(post);
	if (sendICcommand(cmd,8)) {
		on = replystr[6] ? 1 : 0;
		cmd = pre_to;
		cmd.append("\x14\x0D");
		cmd.append(post);
		if (sendICcommand(cmd,9))
			val = 20*(fm_bcd(&replystr[6],3) - 128);
		else
			checkresponse(9);
		return on;
	}
	return on;
}

void RIG_IC746PRO::get_notch_min_max_step(int &min, int &max, int &step)
{
	min = -1280;
	max = 1280;
	step = 20;
}
