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

#include "ICbase.h"
#include "debug.h"
#include "support.h"

//=============================================================================

void RIG_ICOM::adjustCIV(uchar adr)
{
	CIV = adr;
	pre_to[2] = ok[3] = bad[3] = pre_fm[3] = CIV;
}

void RIG_ICOM::checkresponse()
{
	if (!progStatus.use_tcpip && !RigSerial->IsOpen())
		return;

	if (replystr.rfind(ok) != string::npos)
		return;

	string s1 = str2hex(cmd.c_str(), cmd.length());
	string s2 = str2hex(replystr.c_str(), replystr.length());
	LOG_ERROR("\nsent  %s\nreply %s", s1.c_str(), s2.c_str());
}

bool RIG_ICOM::sendICcommand(string cmd, int nbr)
{
	int ret = sendCommand(cmd);

	if (!progStatus.use_tcpip && !RigSerial->IsOpen())
		return false;

	if (ret < nbr) {
		LOG_ERROR("Expected %d received %d", nbr, ret);
		return false;
	}

	if (ret > nbr) replystr.erase(0, ret - nbr);

// look for preamble at beginning
	if (replystr.rfind(pre_fm) == string::npos)  {
		LOG_ERROR("preamble: %s not in %s", pre_fm.c_str(), cmd.c_str());
		return false;
	}

// look for postamble
	if (replystr.rfind(post) == string::npos) {
		LOG_ERROR("postample: %s not at end of %s", post.c_str(), cmd.c_str());
		return false;
	}
	return true;
}

void RIG_ICOM::delayCommand(string cmd, int wait)
{
	int oldwait = progStatus.comm_wait;
	progStatus.comm_wait += wait;
	sendCommand(cmd);
	progStatus.comm_wait = oldwait;
}

bool RIG_ICOM::waitFB(const char *sz)
{
	char sztemp[50];
	string returned = "";
	string tosend = cmd;

	if (!progStatus.use_tcpip && !RigSerial->IsOpen()) {
		replystr = returned;
		snprintf(sztemp, sizeof(sztemp), "%s TEST", sz);
		showresp(INFO, HEX, sztemp, tosend, returned);
		return false;
	}
	int cnt = 0, repeat = 0, num = cmd.length() + ok.length();
	int wait_msec = (int)(num*11000.0/RigSerial->Baud() +
			progStatus.use_tcpip ? progStatus.tcpip_ping_delay : 0) / 10;
	for (repeat = 0; repeat < progStatus.comm_retries; repeat++) {
		sendCommand(cmd, 0);
		returned = "";
		for ( cnt = 0; cnt < wait_msec; cnt++) {
			readResponse();
			returned.append(replystr);
			if (returned.find(ok) != string::npos) {
				replystr = returned;
				waited = 10 * (repeat * wait_msec + cnt);
				snprintf(sztemp, sizeof(sztemp), "%s ans in %d ms, OK", sz, waited);
				showresp(INFO, HEX, sztemp, tosend, returned);
				return true;
			}
			if (returned.find(bad) != string::npos) {
				replystr = returned;
				waited = 10 * (repeat * wait_msec + cnt);
				snprintf(sztemp, sizeof(sztemp), "%s ans in %d ms, FAIL", sz, waited);
				showresp(ERR, HEX, sztemp, tosend, returned);
				return false;
			}
			MilliSleep(10);
			Fl::awake();
		}
	}
	replystr = returned;
	waited = 10 * repeat * wait_msec;
	snprintf(sztemp, sizeof(sztemp), "%s TIMED OUT in %d ms", sz, waited);
	showresp(INFO, HEX, sztemp, tosend, returned);
	return false;
}

bool RIG_ICOM::waitFOR(size_t n, const char *sz)
{
	char sztemp[50];
	string returned = "";
	string tosend = cmd;
	int cnt = 0, repeat = 0;
	size_t num = n + cmd.length();
	int delay =  (int)(num * 11000.0 / RigSerial->Baud() + 
		progStatus.use_tcpip ? progStatus.tcpip_ping_delay : 0) / 10;
	if (!progStatus.use_tcpip && !RigSerial->IsOpen()) {
		replystr = returned;
		snprintf(sztemp, sizeof(sztemp), "%s TEST", sz);
		showresp(INFO, HEX, sztemp, tosend, returned);
		return false;
	}
	for (repeat = 0; repeat < progStatus.comm_retries; repeat++) {
		sendCommand(tosend, 0);
		returned = "";
		for ( cnt = 0; cnt < delay; cnt++) {
			readResponse();
			returned.append(replystr);
			if (returned.length() >= num) {
				replystr = returned;
				waited = 10 * (repeat * delay + cnt);
				snprintf(sztemp, sizeof(sztemp), "%s ans in %d ms, OK  ", sz, waited);
				showresp(INFO, HEX, sztemp, tosend, returned);
				return true;
			}
			MilliSleep(10);
			Fl::awake();
		}
	}
	replystr = returned;
	waited = 10 * repeat * delay;
	snprintf(sztemp, sizeof(sztemp), "%s TIMED OUT in %d ms", sz, waited);
	showresp(ERR, HEX, sztemp, tosend, returned);
	return false;
}

// exchange & equalize are available in these Icom xcvrs
// 718 706MKIIG 746 746PRO 756 756PRO 756PROII 756PROIII
// 910 7000 7100 7200 7300 7410 7600 7700 7800 9100

void RIG_ICOM::swapvfos()
{
	cmd = pre_to;
	cmd += 0x07; cmd += 0xB0;
	cmd.append(post);
	waitFB("Exchange vfos");
}

void RIG_ICOM::A2B()
{
	cmd = pre_to;
	cmd += 0x07; cmd += 0xA0;
	cmd.append(post);
	waitFB("Equalize vfos");
}

