#include "editor.h"
#include "hal/keyboard.h"
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
    g_video->setColor(m_palette[0],hal::green,hal::black);

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
    m_readOnly = false;
    ss.m_insert = true;

    updateCursor();
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
    m_readOnly = readOnly;
    updateCursor();
    return true;  
}

void editor::draw() {
    uint32_t i = m_topOffset;
    uint8_t fw = video::getFontWidth(), fh = video::getFontHeight();
    int row = m_y;
    while (i < ss.m_documentSize) {
        uint32_t j=i;
        while (m_document[j]!=10 && j<ss.m_documentSize)
            j++;
        g_video->drawString(m_x,row,m_palette[0],m_document + i,j-i);
        if (j-i < m_width)
            g_video->fill(m_x + (j-i)*fw,row,(m_width-(j-i))*fw,fh,hal::black);
        if (m_document[j]==10)
            ++j;
        row+=fh;
        if (row >= m_height * fh)
            break;
        i=j;
    }
}

void editor::updateCursor() {
    // this could be much smarter.
    m_cursorOffset = m_topOffset = 0;
    uint32_t line = 0, column = 0;
    for (;m_cursorOffset < ss.m_documentSize && (line != ss.m_cursorLine || column != ss.m_cursorColumn);++m_cursorOffset) {
        if (!column && line==ss.m_topLine)
            m_topOffset = m_cursorOffset;
        if (m_document[m_cursorOffset]==10)
            column=0,++line;
        else
            ++column;
    }
}

void editor::update(uint16_t event) {
    if (!(event & modifier::PRESSED_BIT)) // todo: could update status here
        return;
    uint8_t key = uint8_t(event);
    bool error = false;
    if (key==10 || (key >= 32 && key < 127)) {
        if (m_readOnly || ss.m_documentSize == ss.m_documentCapacity)
            error = true;
        else {
            memmove(m_document + m_cursorOffset + 1,m_document + m_cursorOffset,ss.m_documentSize - m_cursorOffset);
            m_document[m_cursorOffset++] = key;
            ss.m_cursorColumn++;
        }
    }
    else if (key == key::UP) {
        if (ss.m_cursorLine) {
            if (ss.m_topLine == ss.m_cursorLine)
                --ss.m_topLine;
            --ss.m_cursorLine;
            updateCursor();
        }
        else
            error = true;
    }
    if (error) {
        g_video->fill(m_x,m_y,m_width * video::getFontWidth(),m_height * video::getFontHeight(),hal::red);
        draw();
    }
}

}
