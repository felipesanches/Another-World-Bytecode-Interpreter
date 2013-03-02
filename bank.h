/* Raw - Another World Interpreter
 * Copyright (C) 2004 Gregory Montoir
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __BANK_H__
#define __BANK_H__

#include "intern.h"

struct MemEntry;

struct UnpackContext {
	uint32_t crc;
	uint32_t chk;
	int32_t datasize;
};

struct Bank {
	UnpackContext _ctx;
	const char *_dataDir;
  uint8_t *_startBuf;
	uint8_t *_startPacked, *_endPacked, *_packed;
	uint8_t *_startRaw, *_endRaw, *_raw; 
  uint32_t shiftBuffer;
  uint8_t countBits;

	Bank(const char *dataDir);

  bool tryCopyingPattern8bit(uint8_t offsetBits);
  void bitshift(uint8_t numBits, uint32_t data);

	bool read(const MemEntry *me, uint8_t *buf);
  bool write(const MemEntry *me, uint8_t *buf, bool pack);
	void decodeByteSequence(uint8_t minSize, uint8_t sizeBits);
	void CopyPattern(uint16_t size, uint8_t offsetBits);
	bool unpack();
	bool pack(int32_t size);
	uint16_t getCode(uint8_t numBits);
	bool nextBit();
	bool rcr(bool CF);
};

#endif
