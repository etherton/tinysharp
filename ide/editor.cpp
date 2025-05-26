#include "editor.h"
#include "hal/video.h"
#include "hal/storage.h"

#include <string.h>

using namespace hal;

namespace ide {

editor::editor(storage *s) {
    m_storage = s;
    m_x = m_y = 0;
    m_width = video::getScreenWidth() / video::getFontWidth();
    m_height = video::getScreenHeight() / video::getFontHeight();

    newFile();
}

void editor::newFile() {
    memset(&blockPadding,0,sizeof(blockPadding));
    ss.m_magic = savestate::magicValue;
    ss.m_topLine = 0;
    ss.m_cursorLine = 0;
    ss.m_cursorColumn = 0;

    m_document = new char[512];
    m_resident = true;
    ss.m_documentSize = 0;
    ss.m_documentCapacity = 512;
    ss.m_readOnly = false;
}

bool editor::quickSave() {
    if (!m_storage->writeBlock(0,&ss))
        return false;
    size_t blockCount = (ss.m_documentCapacity + m_storage->getBlockSize() - 1) / m_storage->getBlockSize();
    for (size_t i=0; i<blockCount; i++)
        if (!m_storage->writeBlock(i+1,m_document + i * m_storage->getBlockSize()))
            return false;
    m_storage->flush();
    return true;
}

bool editor::quickLoad(bool readOnly) {
    ss.m_magic = 0;
    if (!m_storage->readBlock(0,&ss) || ss.m_magic != savestate::magicValue)
        return false;
    size_t blockCount = (ss.m_documentSize + m_storage->getBlockSize() - 1) / m_storage->getBlockSize();
    if (m_resident)
        delete[] m_document;
    if (readOnly && (m_document = (char*)m_storage->memoryMap(1,blockCount)) != nullptr)
        m_resident = false;
    else {
        m_document = new char[ss.m_documentCapacity];
        if (!m_document)
            return false;
        for (size_t i=0; i<blockCount; i++)
            if (!m_storage->readBlock(i+1,m_document + m_storage->getBlockSize() * i)) {
                delete[] m_document;
                m_document = nullptr;
                return false;
            }
    }
    ss.m_readOnly = readOnly;
    return true;  
}

}
