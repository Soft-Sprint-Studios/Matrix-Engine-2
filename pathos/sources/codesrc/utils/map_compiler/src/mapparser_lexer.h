/*
 * MIT License
 *
 * Copyright (c) 2025-2026 Soft Sprint Studios
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef MAPPARSER_LEXER_H
#define MAPPARSER_LEXER_H

#include "datatypes.h"
#include <cctype>
#include <cstddef>

class CMapLexer
{
public:
    CMapLexer(const Char* buffer, size_t length) :
        m_pBuffer(buffer),
        m_length(length),
        m_offset(0),
        m_line(1)
    {
    }

    bool NextToken(Char* outToken, size_t tokenBufferSize, bool crossLines = true)
    {
        outToken[0] = '\0';
        SkipWhitespace(crossLines);

        if (m_offset >= m_length)
        {
            return false;
        }

        Char c = m_pBuffer[m_offset];

        if (c == '"')
        {
            m_offset++;
            size_t i = 0;
            while (m_offset < m_length && m_pBuffer[m_offset] != '"' && m_pBuffer[m_offset] != '\n')
            {
                if (i < tokenBufferSize - 1)
                {
                    outToken[i++] = m_pBuffer[m_offset];
                }
                m_offset++;
            }
            outToken[i] = '\0';
            if (m_offset < m_length && m_pBuffer[m_offset] == '"')
            {
                m_offset++;
            }
            return true;
        }

        if (c == '(' || c == ')' || c == '{' || c == '}' || c == '[' || c == ']')
        {
            outToken[0] = c;
            outToken[1] = '\0';
            m_offset++;
            return true;
        }

        size_t i = 0;
        while (m_offset < m_length)
        {
            Char ch = m_pBuffer[m_offset];
            if (isspace((unsigned char)ch) || ch == '(' || ch == ')' || ch == '{' || ch == '}' || ch == '[' || ch == ']' || ch == '"')
            {
                break;
            }
            if (i < tokenBufferSize - 1)
            {
                outToken[i++] = ch;
            }
            m_offset++;
        }
        outToken[i] = '\0';
        return i > 0;
    }

    Int32 GetLine() const
    {
        return m_line;
    }

private:
    void SkipWhitespace(bool crossLines)
    {
        while (m_offset < m_length)
        {
            Char c = m_pBuffer[m_offset];

            if (c == '\n')
            {
                m_line++;
                if (!crossLines)
                {
                    return;
                }
                m_offset++;
                continue;
            }

            if (isspace((unsigned char)c))
            {
                m_offset++;
                continue;
            }

            if (c == '/' && m_offset + 1 < m_length && m_pBuffer[m_offset + 1] == '/')
            {
                while (m_offset < m_length && m_pBuffer[m_offset] != '\n')
                {
                    m_offset++;
                }
                continue;
            }

            break;
        }
    }

    const Char* m_pBuffer;
    size_t m_length;
    size_t m_offset;
    Int32 m_line;
};

#endif // MAPPARSER_LEXER_H