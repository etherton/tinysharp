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
    m_height = video::getScreenHeight() / video::getFontHeight() - 1;
    m_statusY = video::getScreenHeight() - video::getFontHeight();
    g_video->setColor(m_palette[TEXT],hal::green,hal::black);
    g_video->setColor(m_palette[LNUM],hal::blue,hal::black);
    g_video->setColor(m_palette[STATUS],hal::black,hal::white);

    newFile();
}

void editor::newFile() {
    memset(&blockPadding,0,sizeof(blockPadding));
    ss.m_magic = savestate::magicValue;
    ss.m_topLine = 0;
    ss.m_cursorLine = 0;
    ss.m_cursorColumn = 0;
    ss.m_showLineNumbers = true;

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
    uint32_t i = m_topOffset, line = ss.m_topLine;
    uint8_t fw = video::getFontWidth(), fh = video::getFontHeight();
    int row = m_y;
    while (i < ss.m_documentSize) {
        uint32_t j=i;
        while (m_document[j]!=10 && j<ss.m_documentSize)
            j++;
        ++line;
        int x = m_x;
        if (ss.m_showLineNumbers) {
            g_video->drawStringf(x,row,m_palette[LNUM],"%3d ",line);
            x+=4*fw;
        }
        g_video->drawString(x,row,m_palette[TEXT],m_document + i,j-i);
        if (j-i < m_width-x)
            g_video->fill(x + (j-i)*fw,row,(m_width-(j-i))*fw,fh,hal::black);
        if (m_document[j]==10)
            ++j;
        row+=fh;
        if (row >= m_height * fh)
            break;
        i=j;
    }
    g_video->drawStringf(0,m_statusY,m_palette[STATUS],"Line: %d Col: %d  Offset %u",ss.m_cursorLine+1,ss.m_cursorColumn+1,m_cursorOffset);
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

    if (key)
        g_video->drawStringf(160,m_statusY-8,m_palette[STATUS],"%s%s%s%s     ",
            event & modifier::CTRL_BITS?"Ctrl+":"",
            event & modifier::ALT_BITS?"Alt+":"",
            event & modifier::SHIFT_BITS?"Shift+":"",
            keyboard::getKeyCap(event));
    
    if (event & modifier::ALT_BITS) {
        if (key == 'S')
            error = !quickSave();
    }
    else if (key==10 || (key >= 32 && key < 127)) {
        if (m_readOnly || ss.m_documentSize == ss.m_documentCapacity)
            error = true;
        else {
            if (m_cursorOffset != ss.m_documentSize)
                memmove(m_document + m_cursorOffset + 1,m_document + m_cursorOffset,ss.m_documentSize - m_cursorOffset);
            m_document[m_cursorOffset++] = key;
            ss.m_cursorColumn++;
            ss.m_documentSize++;
            if (key==10)
                ss.m_cursorLine++;
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
