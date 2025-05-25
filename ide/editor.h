#pragma once

namespace ide {

class editor {
public:
	editor();
	void setFile(char *buffer,size_t length,size_t maxLength);
	void update();
};

}


