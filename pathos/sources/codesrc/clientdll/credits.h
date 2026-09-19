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

#ifndef CREDITS_H
#define CREDITS_H

struct font_set_t;

/*
====================
CCredits

====================
*/
class CCredits
{
public:
	// Default path to credits file
	static const Char CREDITS_FILE_PATH[];
	// Credits font schema name
	static const Char CREDITS_TEXTSCHEMA_NAME[];
	// Default color for text
	static const color32_t CREDITS_TEXT_COLOR;

public:
	struct creditline_t
	{
		creditline_t():
			width(0),
			height(0)
		{
		}

		CString text;
		Uint32 width;
		Uint32 height;
	};

public:
	CCredits( void );
	~CCredits( void );

public:
	bool Init( void );
	void Shutdown( void );

	bool InitGL( void );
	void ClearGL( void );

	bool InitGame( void );
	void ClearGame( void );

	void Think( void );
	bool Draw( void );

public:
	void StartCredits( Float scrollSpeed );
	bool LoadCreditsFile( void );
	void RecomputeDimensions( void );

private:
	bool m_isActive;
	Float m_scrollSpeed;
	Double m_currentY;
	Double m_lastTime;
	Double m_totalHeight;

	Uint32 m_screenWidth;
	Uint32 m_screenHeight;

	CArray<creditline_t> m_creditsLines;
	const font_set_t* m_pFontSet;
};

extern CCredits gCredits;
#endif //CREDITS_H