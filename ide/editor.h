#pragma once

#include <stddef.h>
#include <stdint.h>

namespace hal { class storage; }

namespace ide {

class editor {
	struct savestate {
		static const size_t magicValue = 0xDCE00001;
		size_t m_magic;
		size_t m_documentSize, m_documentCapacity;
		uint32_t m_topLine;
		uint32_t m_cursorLine, m_cursorColumn; // these are zero-based but display as one-based.
		bool m_readOnly;
		bool m_showLineNumbers;
	};
public:
	editor(hal::storage *s);
	void setFile(char *buffer,size_t length,size_t maxLength,bool readOnly = false) {
		m_document = buffer;
		ss.m_documentSize = length;
		ss.m_documentCapacity = maxLength;
		ss.m_readOnly = readOnly; // if readonly, can be in flash
	}
	void newFile();
	bool quickSave();
	bool quickLoad(bool readOnly);
	void draw();

	hal::storage *m_storage;
	char *m_document;
	bool m_resident;
	union {
		savestate ss;
		char blockPadding[512];
	};
	uint16_t m_x, m_y, m_width, m_height;
};

} // namespace ide



