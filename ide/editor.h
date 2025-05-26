#pragma once

namespace ide {

class editor {
public:
	editor();
	void setFile(char *buffer,size_t length,size_t maxLength,bool readOnly = false) {
		m_document = buffer;
		m_documentSize = length;
		m_documentCapacity = maxLength;
		m_readOnly = readOnly; // if readonly, can be in flash
	}
	void draw();

	char *m_document;
	size_t m_documentSize, m_documentCapacity;
	uint32_t m_topLine;
	uint32_t m_cursorLine, m_cursorColumn;
	bool m_readOnly;
};

}


