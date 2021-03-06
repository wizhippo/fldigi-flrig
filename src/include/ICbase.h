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

#ifndef _ICbase_H
#define _ICbase_H

#include "rigbase.h"
#include "debug.h"
#include "support.h"

class RIG_ICOM : public rigbase {
protected:
	string pre_to;
	string pre_fm;
	string post;
	string ok;
	string bad;
	int    waited;
	const char *_mode_type;
public:
	RIG_ICOM() {
		CIV = 0x56;
		pre_to = "\xFE\xFE\x56\xE0";
		pre_fm = "\xFE\xFE\xE0\x56";
		post = "\xFD";
		ok = "\xFE\xFE\xE0\x56\xFB\xFD";
		bad = "\xFE\xFE\xE0\x56\xFA\xFD";
	}
	virtual ~RIG_ICOM() {}
	void checkresponse();
	bool sendICcommand(string str, int nbr);
	void delayCommand(string cmd, int wait);
	bool  waitFB(const char *sz);
	bool  waitFOR(size_t n, const char *sz);
	void adjustCIV(uchar adr);

	virtual void swapvfos();
	virtual void A2B();

};

#endif
