#include "editor.h"
#include "hal/keyboard.h"
#include "hal/video.h"
#include "hal/storage.h"
#include "hal/timer.h"

#include <string.h>
#include <stdio.h>

using namespace hal;

namespace ide {

editor::editor(storage *s) {
    m_storage = s;
    m_xPix = m_yPix = 0;
    m_widthChars = video::getScreenWidth() / video::getFontWidth();
    m_heightChars = video::getScreenHeight() / video::getFontHeight() - 1;
    m_statusYPix = video::getScreenHeight() - video::getFontHeight();
    g_video->setColor(m_palette[TEXT],hal::green,hal::black);
    g_video->setColor(m_palette[LNUM],hal::blue,hal::black);
    g_video->setColor(m_palette[STATUS],hal::black,hal::white);
    g_video->setColor(m_palette[CURSOR],hal::black,hal::green);

    newFile();
}

void editor::newFile() {
    memset(&blockPadding,0,sizeof(blockPadding));
    ss.m_magic = savestate::magicValue;
    ss.m_topLine = 0;
    ss.m_cursorLine = 0;
    ss.m_cursorColumn = 0;
    ss.m_desiredCursorColumn = 0;
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
    m_widthChars = video::getScreenWidth() / video::getFontWidth();
    m_heightChars = video::getScreenHeight() / video::getFontHeight() - 1;
    m_statusYPix = video::getScreenHeight() - video::getFontHeight();

    if (ss.m_asHex)
        drawHex();
    else
        drawText();
}

void editor::drawText() {
    uint32_t i = m_topOffset, line = ss.m_topLine;
    uint8_t fw = video::getFontWidth(), fh = video::getFontHeight();
    int rowChars = 0;
    while (i < ss.m_documentSize) {
        uint32_t j=i;
        while (m_document[j]!=10 && j<ss.m_documentSize)
            j++;
        ++line;
        int xPix = m_xPix, widthChars = m_widthChars;
        if (ss.m_showLineNumbers) {
            g_video->drawStringf(xPix,m_yPix+rowChars*fh,m_palette[LNUM],"%3d ",line);
            xPix+=4*fw;
            widthChars-=4;
        }
        int drawLen = j-i<widthChars?j-i:widthChars;
        g_video->drawString(xPix,m_yPix+rowChars*fh,m_palette[TEXT],m_document + i,drawLen);
        int right = (xPix + drawLen * fw);
        if (right < hal::video::getScreenWidth())
            g_video->fill(xPix + right,m_yPix+rowChars*fh,hal::video::getScreenWidth() - right,fh,m_palette[TEXT]);
        if (m_document[j]==10)
            ++j;
        ++rowChars;
        if (rowChars >= m_heightChars)
            break;
        i=j;
    }
    postDraw(rowChars);
}

void editor::postDraw(int rowChars) {
    int fw = hal::video::getFontWidth();
    int fh = hal::video::getFontHeight();
    int fillHeight = m_statusYPix - (m_yPix + rowChars * fh);
    if (fillHeight)
        g_video->fill(m_xPix,m_yPix + rowChars * fh,m_widthChars * fw,fillHeight,m_palette[TEXT]);
    
    char temp[32];
    sprintf(temp,"Line:%d Col:%d %s Bat:%3d%%",ss.m_cursorLine+1,ss.m_cursorColumn+1,keyboard::getCapsLock()?"CAPS":"    ",
        g_keyboard->getBattery());
    size_t sl = strlen(temp);
    g_video->drawString(m_xPix,m_statusYPix,m_palette[STATUS],temp,sl);
    g_video->fill(m_xPix + sl*fw,m_statusYPix,(m_widthChars-sl)*fw,fh,m_palette[STATUS]);
}

void editor::drawHex() {
    uint8_t perRow = (m_widthChars - 6) / 4; // XXXX nn nn ... cccc
    int rowChars = 0;
    if (perRow >= 64)
        perRow = 64;
    else if (perRow >= 32)
        perRow = 32;
    else if (perRow >= 16)
        perRow = 16;
    else
        perRow = 8;
    uint32_t i = m_topOffset;
    while (i < ss.m_documentSize && rowChars < m_heightChars) {
        char hexBuf[256];
        sprintf(hexBuf,"%04x ",i);
        for (uint32_t o=0; o<perRow; o++) {
            if (i+o < ss.m_documentSize)
                sprintf(hexBuf + 5 + 3*o,"%02x ",m_document[i+o]);
            else
                strcpy(hexBuf + 5 + 3*o,"   ");
        }
        for (uint32_t o=0; o<perRow; o++)
            hexBuf[5 + perRow*3 + o] = i+o<ss.m_documentSize? m_document[i+o] : 32;
        g_video->drawString(m_xPix,m_yPix + rowChars * hal::video::getFontHeight(),m_palette[TEXT],hexBuf,perRow*4 + 5);
        int right = m_xPix + (perRow*4 + 5) * hal::video::getFontWidth();
        if (right < hal::video::getScreenWidth())
            g_video->fill(right,m_yPix + rowChars * hal::video::getFontHeight(),hal::video::getScreenWidth() - right,
                hal::video::getFontHeight(),m_palette[TEXT]);
        i += perRow;
        ++rowChars;
    }
    postDraw(rowChars);
}

void editor::drawCursor() {
    uint8_t fw = video::getFontWidth(), fh = video::getFontHeight();
    char c = m_cursorOffset==ss.m_documentSize? 32 : m_document[m_cursorOffset];
    if (c == 10)
        c = 32;
    g_video->drawString(
        m_xPix + (ss.m_showLineNumbers? 4*fw : 0) + ss.m_cursorColumn * fw,
        m_yPix + (ss.m_cursorLine - ss.m_topLine) * fh,m_palette[getUsTime32() & 0x40000? TEXT:CURSOR],
        &c,1);
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

void editor::updateCursorFromOffset() {
    ss.m_cursorLine = 0;
    ss.m_cursorColumn = 0;
    for (uint32_t i=0; i<m_cursorOffset;i++) {
        if (m_document[i]==10) {
            ss.m_cursorLine++;
            ss.m_cursorColumn = 0;
        }
        else
            ss.m_cursorColumn++;
    }
}

void editor::updateCursorFromVerticalMove() {
    uint32_t line = 0, column = 0;
    for (m_cursorOffset=0; m_cursorOffset<ss.m_documentSize; m_cursorOffset++) {
        if (line==ss.m_cursorLine && column==ss.m_cursorColumn) {
            // try to reach the desired column again
            while (ss.m_cursorColumn < ss.m_desiredCursorColumn && m_cursorOffset<ss.m_documentSize && m_document[m_cursorOffset]!=10)
                ++ss.m_cursorColumn,++m_cursorOffset;
            return;
        }
        if (m_document[m_cursorOffset]==10) {
            // if the line we're on isn't as long as the one we left, remember
            // the desired column (otherwise the test below would happen first)
            if (line == ss.m_cursorLine) {
                ss.m_desiredCursorColumn = ss.m_cursorColumn;
                ss.m_cursorColumn = column;
                return;
            }
            line++;
            column = 0;
        }
        else
            column++;
    }
    // we might end up here if we tried to move down past end of document
    ss.m_desiredCursorColumn = ss.m_cursorColumn;
    ss.m_cursorColumn = column;
    ss.m_cursorLine = line;
}

void editor::updateVisibleRegion() {
    bool updated = false;
    if (ss.m_cursorLine < ss.m_topLine)
        updated = true, ss.m_topLine = ss.m_cursorLine;
    else if (ss.m_cursorLine >= ss.m_topLine + m_heightChars)
        updated = true, ss.m_topLine = ss.m_cursorLine - m_heightChars + 1;
    if (updated)
        updateCursor();
}

void editor::update(uint16_t event) {
    if (!(event & modifier::PRESSED_BIT)) // todo: could update status here
        return;
    uint8_t key = uint8_t(event);
    bool error = false;

    /* if (key)
        g_video->drawStringf(160,m_statusYPix-8,m_palette[STATUS],"%s%s%s%s     ",
            event & modifier::CTRL_BITS?"Ctrl+":"",
            event & modifier::ALT_BITS?"Alt+":"",
            event & modifier::SHIFT_BITS?"Shift+":"",
            keyboard::getKeyCap(event)); */
    
    // anything other than up or down resets the desired column to the actual column
    if (key != key::UP && key != key::DOWN)
            ss.m_desiredCursorColumn = ss.m_cursorColumn;

    if (event & modifier::ALT_BITS) {
        if (key == 'S')
            error = !quickSave();
        else if (key == 'N')
            newFile();
        else if (key == 'H')
            ss.m_asHex = !ss.m_asHex;
        else if (key == 'L')
            ss.m_showLineNumbers = !ss.m_showLineNumbers;
        
    }
    else if (key==10 || (key >= 32 && key < 127)) {
        if (m_readOnly || ss.m_documentSize == ss.m_documentCapacity)
            error = true;
        else {
            // This is by far the common case so do it manually instead of calling updateCursor.
            memmove(m_document + m_cursorOffset + 1,m_document + m_cursorOffset,ss.m_documentSize - m_cursorOffset);
            m_document[m_cursorOffset++] = key;
            ss.m_cursorColumn++;
            ss.m_documentSize++;
            if (key==10) {
                ss.m_cursorColumn = 0;
                ss.m_cursorLine++;
            }
        }
    }
    else if (key == 8) {
        if (!m_readOnly && m_cursorOffset) {
            --m_cursorOffset;
            --ss.m_documentSize;
            memmove(m_document + m_cursorOffset,m_document + m_cursorOffset + 1,ss.m_documentSize - m_cursorOffset);
            updateCursorFromOffset();
            updateVisibleRegion();
        }
        else
            error = true;
    }
    else if (key == key::DEL) {
        if (!m_readOnly && m_cursorOffset != ss.m_documentSize) {
            --ss.m_documentSize;
            memmove(m_document + m_cursorOffset,m_document + m_cursorOffset + 1,ss.m_documentSize - m_cursorOffset);
        }
        else
            error = true;
    }
    else if (key == key::LEFT) {
        if (ss.m_cursorColumn) {
            --m_cursorOffset;
            --ss.m_cursorColumn;
        }
        else if (m_cursorOffset) {
            --m_cursorOffset;
            updateCursorFromOffset();
        }
        else
            error = true;
        updateVisibleRegion();
    }
    else if (key == key::RIGHT) {
        if (m_cursorOffset == ss.m_documentSize)
            error = true;
        else {
            if (m_document[m_cursorOffset]==10) {
                ss.m_cursorColumn = 0;
                ss.m_cursorLine++;
            }
            else
                ss.m_cursorColumn++;
            m_cursorOffset++;
        }
        updateVisibleRegion();
    }
    else if (key == key::UP) {
        if (ss.m_cursorLine) {
            --ss.m_cursorLine;
            updateCursorFromVerticalMove();
            updateVisibleRegion();
        }
        else
            error = true;
    }
    else if (key == key::DOWN) {
        ++ss.m_cursorLine;
        updateCursorFromVerticalMove();
        updateVisibleRegion();
    }
    if (error) {
        g_video->fill(m_xPix,m_yPix,m_widthChars * video::getFontWidth(),m_heightChars * video::getFontHeight(),m_palette[CURSOR]);
        draw();
    }
}

}
