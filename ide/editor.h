#pragma once

#include <stddef.h>
#include <stdint.h>

#include "hal/video.h" // for palette

namespace hal { class storage; }

namespace ide {

class editor {
	struct savestate {
		static const size_t magicValue = 0xDCE00002;
		size_t m_magic;
		size_t m_documentSize, m_documentCapacity;
		uint32_t m_topLine;
		uint32_t m_cursorLine, m_cursorColumn; // these are zero-based but display as one-based.
		uint32_t m_desiredCursorColumn; // for when we're on a line shorter than the one before.
		bool m_showLineNumbers;
		bool m_insert;
		bool m_asHex;
	};
public:
	editor(hal::storage *s);
	void setFile(char *buffer,size_t length,size_t maxLength,bool readOnly = false) {
		m_document = buffer;
		ss.m_documentSize = length;
		ss.m_documentCapacity = maxLength;
		m_readOnly = readOnly; // if readonly, can be in flash
		updateCursor();
	}
	void newFile();
	bool quickSave();
	bool quickLoad(bool readOnly);
	void draw();
	void drawCursor();
	void update(uint16_t keyEvent);
private:
	void drawText();
	void drawHex();
	void postDraw(int rowChars);
	void updateCursor();
	void updateCursorFromOffset();
	void updateCursorFromVerticalMove();
	void updateVisibleRegion();
	hal::storage *m_storage;
	char *m_document;
	hal::palette m_palette[4];
	enum { TEXT, LNUM, STATUS, CURSOR };
	bool m_resident, m_readOnly;
	union {
		savestate ss;
		char blockPadding[512];
	};
	uint32_t m_topOffset, m_cursorOffset;
	uint16_t m_xPix, m_yPix, m_widthChars, m_heightChars, m_statusYPix;
};

} // namespace ide



