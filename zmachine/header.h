#include <stdint.h>

struct word {
	uint8_t hi, lo;

	uint16_t getU() const { return lo | (hi << 8); }
	int16_t  getS() const { return (int16_t)getU(); }
	uint32_t getU2() const { return getU()<<1; }
	bool notZero() const { return hi || lo; }

	void setByte(uint8_t b) { lo = b; hi = 0; }
	void setHL(uint8_t h,uint8_t l) { hi = h; lo = l; }
	void set(int x) { setHL(x>>8,x); }
	int16_t inc() { set(getS()+1); return getS(); }
	int16_t dec() { set(getS()-1); return getS(); }
};

inline word byte2word(uint8_t b) {
	word w;
	w.setByte(b);
	return w;
}

struct storyHeader {
	uint8_t version; // 1-8
	uint8_t flags; // bit 1: 0=score/turns, 1=time
	word pad0;
	word highMemoryAddr;
	word initialPCAddr; // V6: packed address of "main"

	word dictionaryAddr;
	word objectTableAddr;
	word globalVarsTableAddr;
	word staticMemoryAddr;

	word flags2;
	char serial[6];

	word abbreviationsAddr;
	word storyLength; // V1-3: *2; V4-5: *4; V6/7/8: *8
	word checksum;
	uint8_t interpreterNumber;
	uint8_t interpreterVersion;

	uint8_t screenHeightLines;
	uint8_t screenWidthChars;
	word screenWidthUnits;
	word screenHeightUnits;
	uint8_t fontWidthUnits;		// these two are swapped in V6
	uint8_t fontHeightUnits;

	word routinesOffsetDiv8;	// V6
	word staticStringsOffsetDiv8;	// V6
	uint8_t defaultBackgroundColor;
	uint8_t defaultForegroundColor;
	word terminatingCharactersTablesAddr;

	word widthOfTextSentToStream3;
	word standardRevisionNumber;
	word alphabetTableAddress;
	word headerExtensionTableAddress;
};
