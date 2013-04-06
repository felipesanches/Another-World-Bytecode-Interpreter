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

#include <math.h>
#include "bank.h"
#include "file.h"
#include "resource.h"


Bank::Bank(const char *dataDir)
	: _dataDir(dataDir) {
}

bool Bank::write(const MemEntry *me, uint8_t *buf, bool packit) {

	bool ret = false;
	char bankName[10];
  uint32_t rawsize = me->size;
	_startRaw = buf;
  _endRaw = buf + rawsize - 1;

	sprintf(bankName, "bank%02x", me->bankId);
	File f;

	if (!f.open(bankName, _dataDir))
		error("Bank::write() unable to open '%s'", bankName);
	
	f.seek(me->bankOffset);

	if (packit) {
	  _raw = _endRaw;
	  _startPacked = (uint8_t*) malloc(rawsize*sizeof(uint8_t));
    _endPacked = _startPacked + rawsize - 1;
    _packed = _endPacked;
    ret = pack(me->size);
		f.write(_packed, me->packedSize);
    free(_startPacked);
	} else {
		f.write(_startRaw, me->size);
		ret = true;
	}
	
	return ret;
}

void Bank::bitshift(uint8_t numBits, uint32_t data){
  //TODO: Implement-me!
  while (numBits){
    shiftBuffer <<= 1;
    shiftBuffer |= (data & 1);
    data >>= 1;
    numBits--;
    countBits++;
    if (countBits == 32){
      _ctx.crc ^= shiftBuffer;
      WRITE_BE_UINT32(_packed, shiftBuffer);
      _packed -= 4;
      shiftBuffer = 0;
    } 
  }
}

bool Bank::EncodeByteSequence(uint8_t minSize, uint8_t sizeBits) {
	uint16_t count = getCode(sizeBits) + minSize;
	debug(DBG_BANK, "Bank::decodeByteSequence(minSize=%d, sizeBits=%d) count=%d", minSize, sizeBits, count);
	_ctx.datasize -= count;
	while (count--) {
		assert(_raw >= _packed && _raw >= _startBuf);
		*_raw = (uint8_t)getCode(8);
		--_raw;
	}
}

bool Bank::tryCopyingPattern8bit(uint8_t offsetBits){
  uint8_t* raw = _raw;
  uint8_t* found = NULL;

  uint8_t maxlength=4; //must have at least 5 bytes in order to have data compression
  uint32_t maxoffset = pow(2, offsetBits);
  uint8_t* search = MIN(_endRaw, _raw + maxoffset);

  while(search > _raw){
    uint8_t length=0;
    uint8_t* s = search;
    while(*s == *raw && s > _raw){
      length++;
      raw--;
      s--;
    }
    if (length > maxlength){
      maxlength=length;
      found = search + length;
    }
    search--;
  }

  if (found){
    //encode the pattern we've found
    bitshift(1, 1);
    bitshift(2, 0b11);
    bitshift(8, maxlength);

    while (maxlength){
      bitshift(8, *found);
      found--;
      maxlength--;
    }
  }

  return found != NULL;
}

bool Bank::pack(int32_t rawsize) {
  countBits=0;
  shiftBuffer=0;

  do {
		if (tryCopyingPattern8bit(12)) continue; //efficiency: 1,21 up to 62,06
//		if (tryEncodingByteSequence(9, 8)) continue; //efficiency: between 1,02 and 6,54
/*
		if (tryCopyingPattern(4, 10)) continue; //efficiency: 2,46
		if (tryCopyingPattern(3, 9)) continue; //efficiency: 2,00
		if (tryCopyingPattern(2, 8)) continue; //efficiency: 1,60
		if (tryEncodingByteSequence(1, 3)) continue; //efficiency: between 1,04 and 1,60
*/

	} while (rawsize > 0);
  return true;
}


bool Bank::read(const MemEntry *me, uint8_t *buf) {

	bool ret = false;
	char bankName[10];
	sprintf(bankName, "bank%02x", me->bankId);
	File f;

	if (!f.open(bankName, _dataDir))
		error("Bank::read() unable to open '%s'", bankName);

	
	f.seek(me->bankOffset);

	// Depending if the resource is packed or not we
	// can read directly or unpack it.
	if (me->packedSize == me->size) {
		f.read(buf, me->packedSize);
		ret = true;
	} else {
		f.read(buf, me->packedSize);
		_startBuf = buf;
		_packed = buf + me->packedSize - 4;
		ret = unpack();
	}
	
	return ret;
}

void Bank::decodeByteSequence(uint8_t minSize, uint8_t sizeBits) {
	uint16_t count = getCode(sizeBits) + minSize;
	debug(DBG_BANK, "Bank::decodeByteSequence(minSize=%d, sizeBits=%d) count=%d", minSize, sizeBits, count);
	_ctx.datasize -= count;
	while (count--) {
		assert(_raw >= _packed && _raw >= _startBuf);
		*_raw = (uint8_t)getCode(8);
		--_raw;
	}
}

/*
  Note from fab:
    This look like run-length encoding.

  Note from Felipe Sanches:
    This function decodes a portion of the data by making a copy of a chunk of data that is already present in a region of the previously decoded data.

    The size of the data chunk is given by the first parameter: <size>.
    The position from where data is copied is defined by an offset that is 
      read from the encoded data stream by reading its next <offsetBits> bits.

*/
void Bank::CopyPattern(uint16_t count, uint8_t offsetBits) {
	uint16_t offset = getCode(offsetBits);
	debug(DBG_BANK, "Bank::CopyPattern(count=%d, offsetBits=%d) offset=%d", count, offsetBits, offset);
	_ctx.datasize -= count;
	while (count--) {
		assert(_raw >= _packed && _raw >= _startBuf);
		*_raw = *(_raw + offset);
		--_raw;
	}
}

/*
	Most resource in the banks are compacted.
*/
bool Bank::unpack() {
	_ctx.datasize = READ_BE_UINT32(_packed); _packed -= 4;
	_raw = _startBuf + _ctx.datasize - 1;
	_ctx.crc = READ_BE_UINT32(_packed); _packed -= 4;
	_ctx.chk = READ_BE_UINT32(_packed); _packed -= 4;
	_ctx.crc ^= _ctx.chk;
	do {
		if (!nextBit()) {
			if (!nextBit()) {
				decodeByteSequence(1, 3); // 1 + [3bits] == decode at least 1 byte / up to 8 bytes

        //Packing efficiency: between 1,04 and 1,60
        //Efficiency Formula: (8 + 8N)/(5 + 8N) for N=[0 to 7]
        //Data size: 1+N bytes

        //It takes 2 + 3 + 8*[0 to 2^3-1] = 5 to 61 bits of encoded data to represent
        // at least 8 bits / up to 64 bits of raw data.
			} else {
        //copy 2 bytes previously occurring in the decoded data
				CopyPattern(2, 8); //up to 2^8-1 = 255 bytes far away

        //Packing efficiency: 1,60
        //It takes 10 bits of encoded data to represent 16 bits of raw data.
			}
		} else {
			uint16_t c = getCode(2);
      switch(c){
        case 0:
          //copy 3 bytes from a pattern previously occurring in the decoded data
					CopyPattern(3, 9); //up to 2^9 = 512 bytes far away

          //Packing efficiency: 2,00
          //It takes 12 bits to encode 24bits
          break;
        case 1:
          //copy 4 bytes from a pattern previously occurring in the decoded data
					CopyPattern(4, 10); //up to 2^10 = 1kbytes far away

          //Packing efficiency: 2,46
          //It takes 13 bits to encode 32bits
          break;
        case 2:
          //copy up to 256 bytes (ammount defined by 8bit code)
          // from a pattern previously occurring in the decoded data
					CopyPattern(getCode(8)+1, 12); //up to 2^12 = 4kbytes far away

          //Packing efficiency: 1,21 up to 62,06
          //Efficiency Fórmula: 8N / 33 for N=[1 to 256]
          //Data size: N bytes

          //It takes 3+8+12 = 23 bits to encode 8 bits up to 8*256 bits
          //This is only useful for encoding a sequence of at least 5 bytes, otherwise, the resulting encoded data would take up more space than the raw data.
          break;
        case 3:
  				decodeByteSequence(9, 8); // 9 + [8bits] == decode at least 9 bytes / 
                                    //  up to 9+255 = 264 bytes

          //Packing efficiency: between 1,02 and 6,54
          //Efficiency Fórmula: (72 + 8N) / (11 + 8N) for N=[0 to 255]
          //Data size: 9+N bytes

          //It takes 3 + 8 + 8*[0 to 2^8-1] = 11 + 8*[0 to 255] bits
          // of encoded data to represent
          // at least 9*8=72 bits / up to 264*8 bits of raw data.
          break;
      }
		}
	} while (_ctx.datasize > 0);
	return (_ctx.crc == 0);
}

uint16_t Bank::getCode(uint8_t numBits) {
	uint16_t c = 0;
	while (numBits--) {
		c <<= 1;
		if (nextBit()) {
			c |= 1;
		}			
	}
	return c;
}

bool Bank::nextBit() {
	bool CF = rcr(false);
	if (_ctx.chk == 0) {
		assert(_packed >= _startBuf);
		_ctx.chk = READ_BE_UINT32(_packed); _packed -= 4;
		_ctx.crc ^= _ctx.chk;
		CF = rcr(true);
	}
	return CF;
}

bool Bank::rcr(bool CF) {
	bool rCF = (_ctx.chk & 1);
	_ctx.chk >>= 1;
	if (CF) _ctx.chk |= 0x80000000;
	return rCF;
}
