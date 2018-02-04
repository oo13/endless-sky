/* WrappedText.cpp
Copyright (c) 2014 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "WrappedText.h"

#include "Font.h"
#include "Point.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <numeric>
#include <unordered_map>
#include <unordered_set>

using namespace std;

namespace {
	// The type of line break opportunity.
	enum LineBreakOpportunity {
		// It can be overrided, but equivalent BREAK_ALLOWED at the end.
		WEAK_BREAK_ALLOWED = 0,
		// ! Mandatory break at the indicated position.
		MANDATORY_BREAK,
		// # No break allowed at the indicated position.
		NO_BREAK_ALLOWED,
		// % Break allowed at the indicated position.
		BREAK_ALLOWED,
	};
	
	// This class is a merged characters' block which doesn't allow to break a
	// line and to vary the width in it.
	class MergedCharactersBlock {
	public:
		// This block is a interword-space. It's not drawn.
		// The string s contains a single character if isInterwordSpace is true.
		bool isInterwordSpace;
		
		// This block is a space. It'll be removed at the end of line,
		// otherwise it'll be drawn. In fact, OGHAM SPACE MARK has a glyph.
		bool isSpace;
		
		// The interblock space between this block and next one can be expanded
		// width according to this weight.
		// 0: Not adjustable.
		// 1: Half weight of interword spaces (in the hope that the extra width
		//    will be less than 1/4 EM SPACE).
		// 2: Interword space. (U+0020 SPACE and U+00A0 NO-BREAK SPACE)
		int spaceWeight;
		
		// Line Break Opportunity at the end of this block.
		LineBreakOpportunity lineBreakOpportunity;
		
		// This block is a end of paragraph.
		bool isParagraphEnd;
		
		// The contents of this block.
		string s;
		
		MergedCharactersBlock();
		
		// To move and clear.
		MergedCharactersBlock moveAndClear();
	};
	
	// Divide a string into some MergedCharactersBlocks.
	vector<MergedCharactersBlock> divideIntoBlocks(const string &text);
}



WrappedText::WrappedText()
	: font(nullptr), space(0), wrapWidth(1000), tabWidth(0),
	  lineHeight(0), paragraphBreak(0), alignment(JUSTIFIED), height(0)
{
}



WrappedText::WrappedText(const Font &font)
	: WrappedText()
{
	SetFont(font);
}



// Set the alignment mode.
WrappedText::Align WrappedText::Alignment() const
{
	return alignment;
}



void WrappedText::SetAlignment(Align align)
{
	alignment = align;
}



// Set the wrap width. This does not include any margins.
int WrappedText::WrapWidth() const
{
	return wrapWidth;
}



void WrappedText::SetWrapWidth(int width)
{
	wrapWidth = width;
}



// Set the font to use. This will also set sensible defaults for the tab
// width, line height, and paragraph break. You must specify the wrap width
// and the alignment separately.
void WrappedText::SetFont(const Font &font)
{
	this->font = &font;
	
	space = font.Space();
	SetTabWidth(4 * space);
	SetLineHeight(font.Height() * 120 / 100);
	SetParagraphBreak(font.Height() * 40 / 100);
}



// Set the width in pixels of a single '\t' character.
int WrappedText::TabWidth() const
{
	return tabWidth;
}



void WrappedText::SetTabWidth(int width)
{
	tabWidth = width;
}



// Set the height in pixels of one line of text within a paragraph.
int WrappedText::LineHeight() const
{
	return lineHeight;
}



void WrappedText::SetLineHeight(int height)
{
	lineHeight = height;
}



// Set the extra spacing in pixels to be added in between paragraphs.
int WrappedText::ParagraphBreak() const
{
	return paragraphBreak;
}



void WrappedText::SetParagraphBreak(int height)
{
	paragraphBreak = height;
}



// Get the word positions when wrapping the given text. The coordinates
// always begin at (0, 0).
void WrappedText::Wrap(const string &str)
{
	SetText(str.data(), str.length());
	
	Wrap();
}



void WrappedText::Wrap(const char *str)
{
	SetText(str, strlen(str));
	
	Wrap();
}



// Get the height of the wrapped text.
int WrappedText::Height() const
{
	return height;
}



// Draw the text.
void WrappedText::Draw(const Point &topLeft, const Color &color) const
{
	for(const Word &w : words)
		font->Draw(w.Str(), w.Pos() + topLeft, color);
}



WrappedText::Word::Word()
	: s(), x(0), y(0)
{
}



const string &WrappedText::Word::Str() const
{
	return s;
}



Point WrappedText::Word::Pos() const
{
	return Point(x, y);
}



void WrappedText::SetText(const char *it, size_t length)
{
	// Clear any previous word-wrapping data. It becomes invalid as soon as the
	// underlying text buffer changes.
	words.clear();
	
	// Reallocate that buffer.
	text.assign(it, length);
}



void WrappedText::Wrap()
{
	height = 0;
	if(text.empty() || !font)
		return;
	
	vector<MergedCharactersBlock> blocks(divideIntoBlocks(text));
	if(blocks.empty())
		return;
	// This is a sentinel.
	blocks.back().lineBreakOpportunity = MANDATORY_BREAK;
	blocks.back().isParagraphEnd = true;

	// This is a width of a block.
	vector<int> widthOfBlock;
	// This is the width that a block would add to a line, except the block is at the beginning of line.
	vector<int> additionalWidth;
	int defered = 0;
	for(const auto &block : blocks)
	{
		if(block.isInterwordSpace)
			// An interword space is not drawn, and its block has a single character.
			widthOfBlock.push_back(Space(Font::DecodeCodePoint(block.s, 0)));
		else
			widthOfBlock.push_back(font->Width(block.s));
		
		if(block.isInterwordSpace || block.isSpace)
		{
			// Any space blocks can removed at the end of line.
			additionalWidth.push_back(0);
			defered += widthOfBlock.back();
		}
		else
		{
			additionalWidth.push_back(widthOfBlock.back() + defered);
			defered = 0;
		}
	}
	
	// A candidate for next word.
	Word word;
	
	// Keep track of how wide the current line would be. This is just so we know
	// how the line would be overflowed.
	int lineWidth = 0;
	// This is the index in the "blocks" vector of the first block on this line.
	size_t blockBegin = 0;
	// This is the candidate for index in the "blocks" vector of the first block
	// on next line.
	size_t blockEnd = 0;
	// TODO: handle single words that are longer than the wrap width. Right now
	// they are simply drawn un-broken, and thus extend beyond the margin.
	// TODO: break words at hyphens, or even do automatic hyphenation.
	for(size_t n = 0; n < blocks.size(); ++n)
	{
		const auto &block = blocks[n];
		
		// Update the candidate for width of the line.
		lineWidth += additionalWidth[n];
		
		// Whenever we encounter the point of line-breakable, the current word needs wrapping.
		if(block.lineBreakOpportunity != NO_BREAK_ALLOWED)
		{
			// If adding this block would overflow the length of the line.
			const bool needToBreak = lineWidth > wrapWidth;
			
			if(!needToBreak)
				blockEnd = n + 1;
			if(needToBreak || block.lineBreakOpportunity == MANDATORY_BREAK)
			{
				// The end of paragraph needs not to break a line.
				const bool isEnd = !needToBreak;
				
				// A line has a single word at least.
				if(blockBegin == blockEnd)
					blockEnd = n + 1;
				
				// Keep the begin of the next line.
				const size_t nextLineBegin = blockEnd;
				// Remove any spaces at the end of line.
				while(blockBegin < blockEnd && (blocks[blockEnd-1].isInterwordSpace || blocks[blockEnd-1].isSpace))
					--blockEnd;
				
				// This is the index in the "words" vector of the first word on this line.
				const size_t lineBegin = words.size();
				// Space weight of the words.
				vector<int> spaceWeights;
				// Generating the words to draw.
				for(size_t m = blockBegin; m < blockEnd; ++m)
				{
					// An interword space is not drawn.
					if(!blocks[m].isInterwordSpace)
					{
						swap(word.s, blocks[m].s);
						words.push_back(word);
						spaceWeights.push_back(blocks[m].spaceWeight);
					}
					else if(!spaceWeights.empty())
						// Enable the space weight of this interword space.
						spaceWeights.back() = max(spaceWeights.back(), blocks[m].spaceWeight);
					
					// Proceed to the next x potision.
					word.x += widthOfBlock[m];
				}
				
				// Adjust the spacing of words.
				AdjustLine(lineBegin, word.x, isEnd, spaceWeights);
				
				// The next word will be the first on the next line.
				word.y += lineHeight;
				word.x = 0;
				lineWidth = widthOfBlock[nextLineBegin] - additionalWidth[nextLineBegin];
				if(blocks[nextLineBegin-1].isParagraphEnd)
					// Here is a paragraph break.
					word.y += paragraphBreak;
				n = nextLineBegin - 1;
				blockBegin = blockEnd = nextLineBegin;
			}
		}
	}
	
	height = word.y;
}



void WrappedText::AdjustLine(size_t lineBegin, int lineWidth, bool isEnd, const vector<int> &spaceWeight)
{
	int wordCount = words.size() - lineBegin;
	if(wordCount == 0)
		return;
	
	int extraSpace = wrapWidth - lineWidth;
	
	// Figure out how much space is left over. Depending on the alignment, we
	// will add that space to the left, to the right, to both sides, or to the
	// space in between the words. Exception: the last line of a "justified"
	// paragraph is left aligned, not justified.
	if(alignment == JUSTIFIED && !isEnd && wordCount > 1)
	{
		const int totalSpaceWeight = accumulate(spaceWeight.begin(), spaceWeight.end(), 0);
		if(totalSpaceWeight == 0)
			return;
		// The hard limit of compression. Too long line including some NO-BREAK SPACEs can be compressed.
		extraSpace = max(extraSpace, -totalSpaceWeight);
		int partialSumOfWeight = 0;
		int shift = 0;
		for(int i = 0; i < wordCount; ++i)
		{
			words[lineBegin + i].x += shift;
			if(spaceWeight[i])
			{
				partialSumOfWeight += spaceWeight[i];
				shift = (2 * extraSpace * partialSumOfWeight / totalSpaceWeight + 1) / 2;
			}
		}
	}
	else if(alignment == CENTER || alignment == RIGHT)
	{
		int shift = (alignment == CENTER) ? extraSpace / 2 : extraSpace;
		for(int i = 0; i < wordCount; ++i)
			words[lineBegin + i].x += shift;
	}
}



int WrappedText::Space(char32_t c) const
{
	return (c == ' ' || c == U'\U000000A0') ? space : (c == '\t') ? tabWidth : 0;
}



namespace {
	// A implementation of Unicode Line Breaking Algorithm. (UAX #14 Revision#39)
	// For more information, see UAX #14: Unicode Line Breaking Algorithm,
	// at http://www.unicode.org/reports/tr14/.
	//
	// This implementation uses the default set as the tailorable line breaking
	// rules at section 6.2 on UAX#14. Supposedly, It's enough for western and
	// east asian styles.
	
	// Unicode Line_Break Property.
	// and
	// LB1 Assign a line breaking class.
	// AI, SG, XX, SA, and CJ are treated as General_Category in UAX #14.
	enum LineBreakProperty : unsigned long long {
		LBP_EMPTY = 0,
		// Ordinary Alphabetic and Symbol Characters (XP)
		// including:
		//   AI: Ambiguous (Alphabetic or Ideograph)
		//   SG: Surrogate (XP) (Non-tailorable)
		//   XX: Unknown (XP)
		//   SA: Complex-Context Dependent (South East Asian) (P) (Any except Mn and Mc)
		AL = (1ULL << 0),
		// Break Opportunity Before and After (B/A/XP)
		B2 = (1ULL << 1),
		// Break After (A)
		BA = (1ULL << 2),
		// Break Before (B)
		BB = (1ULL << 3),
		// Mandatory Break (A) (Non-tailorable)
		BK = (1ULL << 4),
		// Contingent Break Opportunity (B/A)
		CB = (1ULL << 5),
		// Close Punctuation (XB)
		CL = (1ULL << 6),
		// Combining Mark (XB) (Non-tailorable)
		// including:
		//   SA: Complex-Context Dependent (South East Asian) (P) (Only Mn or Mc)
		CM = (1ULL << 7),
		// Closing Parenthesis (XB)
		CP = (1ULL << 8),
		// Carriage Return (A) (Non-tailorable)
		CR = (1ULL << 9),
		// Emoji Base (B/A)
		EB = (1ULL << 10),
		// Emoji Modifier (A)
		EM = (1ULL << 11),
		// Exclamation/Interrogation (XB)
		EX = (1ULL << 12),
		// Non-breaking ("Glue") (XB/XA) (Non-tailorable)
		GL = (1ULL << 13),
		// Hangul LV Syllable (B/A)
		H2 = (1ULL << 14),
		// Hangul LVT Syllable (B/A)
		H3 = (1ULL << 15),
		// Hebrew Letter (XB)
		HL = (1ULL << 16),
		// Hyphen (XA)
		HY = (1ULL << 17),
		// Ideographic (B/A)
		ID = (1ULL << 18),
		// Inseparable Characters (XP)
		IN = (1ULL << 19),
		// Infix Numeric Separator (XB)
		IS = (1ULL << 20),
		// Hangul L Jamo (B)
		JL = (1ULL << 21),
		// Hangul T Jamo (A)
		JT = (1ULL << 22),
		// Hangul V Jamo (XA/XB)
		JV = (1ULL << 23),
		// Line Feed (A) (Non-tailorable)
		LF = (1ULL << 24),
		// Next Line (A) (Non-tailorable)
		NL = (1ULL << 25),
		// Nonstarters (XB)
		// including:
		//   CJ: Conditional Japanese Starter
		NS = (1ULL << 26),
		// Numeric (XP)
		NU = (1ULL << 27),
		// Open Punctuation (XA)
		OP = (1ULL << 28),
		// Postfix Numeric (XB)
		PO = (1ULL << 29),
		// Prefix Numeric (XA)
		PR = (1ULL << 30),
		// Quotation (XB/XA)
		QU = (1ULL << 31),
		// Regional Indicator (B/A/XP)
		RI = (1ULL << 32),
		// Space (A) (Non-tailorable)
		SP = (1ULL << 33),
		// Symbols Allowing Break After (A)
		SY = (1ULL << 34),
		// Word Joiner (XB/XA) (Non-tailorable)
		WJ = (1ULL << 35),
		// Zero Width Space (A) (Non-tailorable)
		ZW = (1ULL << 36),
		// Zero Width Joiner (XA/XB) (Non-tailorable)
		ZWJ = (1ULL << 37),
	};
	
	// Bitwise OR.
	LineBreakProperty operator|=(LineBreakProperty &a, LineBreakProperty b)
	{
		return a = static_cast<LineBreakProperty>(a | b);
	}
	
	
	
	// 1st priority Line_Break Property Table.
	// 2nd priority table has the characters are not in this table.
	const unordered_map<char32_t, LineBreakProperty> lineBreakProperties1st
	{
		{ U'\U00000009', BA }, { U'\U0000000A', LF },
		{ U'\U0000000B', BK }, { U'\U0000000C', BK },
		{ U'\U0000000D', CR }, { U'\U00000020', SP },
		{ U'\U00000021', EX }, { U'\U00000022', QU },
		{ U'\U00000023', AL }, { U'\U00000024', PR },
		{ U'\U00000025', PO }, { U'\U00000026', AL },
		{ U'\U00000027', QU }, { U'\U00000028', OP },
		{ U'\U00000029', CP }, { U'\U0000002A', AL },
		{ U'\U0000002B', PR }, { U'\U0000002C', IS },
		{ U'\U0000002D', HY }, { U'\U0000002E', IS },
		{ U'\U0000002F', SY }, { U'\U0000003A', IS },
		{ U'\U0000003B', IS }, { U'\U0000003F', EX },
		{ U'\U0000005B', OP }, { U'\U0000005C', PR },
		{ U'\U0000005D', CP }, { U'\U0000007B', OP },
		{ U'\U0000007C', BA }, { U'\U0000007D', CL },
		{ U'\U0000007E', AL }, { U'\U00000085', NL },
		{ U'\U000000A0', GL }, { U'\U000000A1', OP },
		{ U'\U000000A2', PO }, { U'\U000000AB', QU },
		{ U'\U000000AC', AL }, { U'\U000000AD', BA },
		{ U'\U000000AE', AL }, { U'\U000000AF', AL },
		{ U'\U000000B0', PO }, { U'\U000000B1', PR },
		{ U'\U000000B2', AL }, { U'\U000000B3', AL },
		{ U'\U000000B4', BB }, { U'\U000000BB', QU },
		{ U'\U000000BF', OP }, { U'\U000002C8', BB },
		{ U'\U000002CC', BB }, { U'\U000002DF', BB },
		{ U'\U0000034F', GL }, { U'\U0000037E', IS },
		{ U'\U00000589', IS }, { U'\U0000058A', BA },
		{ U'\U0000058F', PR }, { U'\U00000590', AL },
		{ U'\U000005BE', BA }, { U'\U000005BF', CM },
		{ U'\U000005C0', AL }, { U'\U000005C1', CM },
		{ U'\U000005C2', CM }, { U'\U000005C3', AL },
		{ U'\U000005C4', CM }, { U'\U000005C5', CM },
		{ U'\U000005C6', EX }, { U'\U000005C7', CM },
		{ U'\U0000060C', IS }, { U'\U0000060D', IS },
		{ U'\U0000060E', AL }, { U'\U0000060F', AL },
		{ U'\U0000061B', EX }, { U'\U0000061C', CM },
		{ U'\U0000061D', AL }, { U'\U0000061E', EX },
		{ U'\U0000061F', EX }, { U'\U0000066A', PO },
		{ U'\U0000066B', NU }, { U'\U0000066C', NU },
		{ U'\U00000670', CM }, { U'\U000006D4', EX },
		{ U'\U000006D5', AL }, { U'\U000006DD', AL },
		{ U'\U000006DE', AL }, { U'\U000006E5', AL },
		{ U'\U000006E6', AL }, { U'\U000006E7', CM },
		{ U'\U000006E8', CM }, { U'\U000006E9', AL },
		{ U'\U000006EE', AL }, { U'\U000006EF', AL },
		{ U'\U00000711', CM }, { U'\U000007F8', IS },
		{ U'\U000007F9', EX }, { U'\U0000081A', AL },
		{ U'\U00000824', AL }, { U'\U00000828', AL },
		{ U'\U000008E2', AL }, { U'\U0000093D', AL },
		{ U'\U00000950', AL }, { U'\U00000962', CM },
		{ U'\U00000963', CM }, { U'\U00000964', BA },
		{ U'\U00000965', BA }, { U'\U000009BC', CM },
		{ U'\U000009BD', AL }, { U'\U000009C5', AL },
		{ U'\U000009C6', AL }, { U'\U000009C7', CM },
		{ U'\U000009C8', CM }, { U'\U000009C9', AL },
		{ U'\U000009CA', AL }, { U'\U000009D7', CM },
		{ U'\U000009E2', CM }, { U'\U000009E3', CM },
		{ U'\U000009E4', AL }, { U'\U000009E5', AL },
		{ U'\U000009F0', AL }, { U'\U000009F1', AL },
		{ U'\U000009F2', PO }, { U'\U000009F3', PO },
		{ U'\U000009F9', PO }, { U'\U000009FA', AL },
		{ U'\U000009FB', PR }, { U'\U00000A3C', CM },
		{ U'\U00000A3D', AL }, { U'\U00000A47', CM },
		{ U'\U00000A48', CM }, { U'\U00000A49', AL },
		{ U'\U00000A4A', AL }, { U'\U00000A51', CM },
		{ U'\U00000A70', CM }, { U'\U00000A71', CM },
		{ U'\U00000A75', CM }, { U'\U00000ABC', CM },
		{ U'\U00000ABD', AL }, { U'\U00000AC6', AL },
		{ U'\U00000ACA', AL }, { U'\U00000AE2', CM },
		{ U'\U00000AE3', CM }, { U'\U00000AE4', AL },
		{ U'\U00000AE5', AL }, { U'\U00000AF0', AL },
		{ U'\U00000AF1', PR }, { U'\U00000B00', AL },
		{ U'\U00000B3C', CM }, { U'\U00000B3D', AL },
		{ U'\U00000B45', AL }, { U'\U00000B46', AL },
		{ U'\U00000B47', CM }, { U'\U00000B48', CM },
		{ U'\U00000B49', AL }, { U'\U00000B4A', AL },
		{ U'\U00000B56', CM }, { U'\U00000B57', CM },
		{ U'\U00000B62', CM }, { U'\U00000B63', CM },
		{ U'\U00000B64', AL }, { U'\U00000B65', AL },
		{ U'\U00000B82', CM }, { U'\U00000BC9', AL },
		{ U'\U00000BD7', CM }, { U'\U00000BF9', PR },
		{ U'\U00000C45', AL }, { U'\U00000C49', AL },
		{ U'\U00000C55', CM }, { U'\U00000C56', CM },
		{ U'\U00000C62', CM }, { U'\U00000C63', CM },
		{ U'\U00000C64', AL }, { U'\U00000C65', AL },
		{ U'\U00000CBC', CM }, { U'\U00000CBD', AL },
		{ U'\U00000CC5', AL }, { U'\U00000CC9', AL },
		{ U'\U00000CD5', CM }, { U'\U00000CD6', CM },
		{ U'\U00000CE2', CM }, { U'\U00000CE3', CM },
		{ U'\U00000CE4', AL }, { U'\U00000CE5', AL },
		{ U'\U00000D3B', CM }, { U'\U00000D3C', CM },
		{ U'\U00000D3D', AL }, { U'\U00000D45', AL },
		{ U'\U00000D49', AL }, { U'\U00000D57', CM },
		{ U'\U00000D62', CM }, { U'\U00000D63', CM },
		{ U'\U00000D64', AL }, { U'\U00000D65', AL },
		{ U'\U00000D79', PO }, { U'\U00000D82', CM },
		{ U'\U00000D83', CM }, { U'\U00000DCA', CM },
		{ U'\U00000DD5', AL }, { U'\U00000DD6', CM },
		{ U'\U00000DD7', AL }, { U'\U00000DF0', AL },
		{ U'\U00000DF1', AL }, { U'\U00000DF2', CM },
		{ U'\U00000DF3', CM }, { U'\U00000E31', CM },
		{ U'\U00000E32', AL }, { U'\U00000E33', AL },
		{ U'\U00000E3F', PR }, { U'\U00000E4F', AL },
		{ U'\U00000E5A', BA }, { U'\U00000E5B', BA },
		{ U'\U00000EB1', CM }, { U'\U00000EB2', AL },
		{ U'\U00000EB3', AL }, { U'\U00000EBA', AL },
		{ U'\U00000EBB', CM }, { U'\U00000EBC', CM },
		{ U'\U00000ECE', AL }, { U'\U00000ECF', AL },
		{ U'\U00000F05', AL }, { U'\U00000F06', BB },
		{ U'\U00000F07', BB }, { U'\U00000F08', GL },
		{ U'\U00000F09', BB }, { U'\U00000F0A', BB },
		{ U'\U00000F0B', BA }, { U'\U00000F0C', GL },
		{ U'\U00000F12', GL }, { U'\U00000F13', AL },
		{ U'\U00000F14', EX }, { U'\U00000F18', CM },
		{ U'\U00000F19', CM }, { U'\U00000F34', BA },
		{ U'\U00000F35', CM }, { U'\U00000F36', AL },
		{ U'\U00000F37', CM }, { U'\U00000F38', AL },
		{ U'\U00000F39', CM }, { U'\U00000F3A', OP },
		{ U'\U00000F3B', CL }, { U'\U00000F3C', OP },
		{ U'\U00000F3D', CL }, { U'\U00000F3E', CM },
		{ U'\U00000F3F', CM }, { U'\U00000F7F', BA },
		{ U'\U00000F85', BA }, { U'\U00000F86', CM },
		{ U'\U00000F87', CM }, { U'\U00000F98', AL },
		{ U'\U00000FBD', AL }, { U'\U00000FBE', BA },
		{ U'\U00000FBF', BA }, { U'\U00000FC6', CM },
		{ U'\U00000FD0', BB }, { U'\U00000FD1', BB },
		{ U'\U00000FD2', BA }, { U'\U00000FD3', BB },
		{ U'\U00000FD9', GL }, { U'\U00000FDA', GL },
		{ U'\U0000103F', AL }, { U'\U0000104A', BA },
		{ U'\U0000104B', BA }, { U'\U00001061', AL },
		{ U'\U00001065', AL }, { U'\U00001066', AL },
		{ U'\U0000108E', AL }, { U'\U0000108F', CM },
		{ U'\U00001360', AL }, { U'\U00001361', BA },
		{ U'\U00001400', BA }, { U'\U00001680', BA },
		{ U'\U0000169B', OP }, { U'\U0000169C', CL },
		{ U'\U00001735', BA }, { U'\U00001736', BA },
		{ U'\U00001752', CM }, { U'\U00001753', CM },
		{ U'\U00001772', CM }, { U'\U00001773', CM },
		{ U'\U000017D4', BA }, { U'\U000017D5', BA },
		{ U'\U000017D6', NS }, { U'\U000017D7', AL },
		{ U'\U000017D8', BA }, { U'\U000017D9', AL },
		{ U'\U000017DA', BA }, { U'\U000017DB', PR },
		{ U'\U000017DC', AL }, { U'\U000017DD', CM },
		{ U'\U000017DE', AL }, { U'\U000017DF', AL },
		{ U'\U00001802', EX }, { U'\U00001803', EX },
		{ U'\U00001804', BA }, { U'\U00001805', BA },
		{ U'\U00001806', BB }, { U'\U00001807', AL },
		{ U'\U00001808', EX }, { U'\U00001809', EX },
		{ U'\U0000180A', AL }, { U'\U0000180E', GL },
		{ U'\U0000180F', AL }, { U'\U00001885', CM },
		{ U'\U00001886', CM }, { U'\U000018A9', CM },
		{ U'\U00001944', EX }, { U'\U00001945', EX },
		{ U'\U00001A5F', AL }, { U'\U00001A7D', AL },
		{ U'\U00001A7E', AL }, { U'\U00001A7F', CM },
		{ U'\U00001B5A', BA }, { U'\U00001B5B', BA },
		{ U'\U00001B5C', AL }, { U'\U00001BAE', AL },
		{ U'\U00001BAF', AL }, { U'\U00001C7E', BA },
		{ U'\U00001C7F', BA }, { U'\U00001CD3', AL },
		{ U'\U00001CED', CM }, { U'\U00001CF5', AL },
		{ U'\U00001CF6', AL }, { U'\U00001DFA', AL },
		{ U'\U00001FFD', BB }, { U'\U00001FFE', AL },
		{ U'\U00001FFF', AL }, { U'\U00002007', GL },
		{ U'\U0000200B', ZW }, { U'\U0000200C', CM },
		{ U'\U0000200D', ZWJ }, { U'\U0000200E', CM },
		{ U'\U0000200F', CM }, { U'\U00002010', BA },
		{ U'\U00002011', GL }, { U'\U00002012', BA },
		{ U'\U00002013', BA }, { U'\U00002014', B2 },
		{ U'\U00002018', QU }, { U'\U00002019', QU },
		{ U'\U0000201A', OP }, { U'\U0000201E', OP },
		{ U'\U0000201F', QU }, { U'\U00002027', BA },
		{ U'\U00002028', BK }, { U'\U00002029', BK },
		{ U'\U0000202F', GL }, { U'\U00002038', AL },
		{ U'\U00002039', QU }, { U'\U0000203A', QU },
		{ U'\U0000203B', AL }, { U'\U0000203C', NS },
		{ U'\U0000203D', NS }, { U'\U00002044', IS },
		{ U'\U00002045', OP }, { U'\U00002046', CL },
		{ U'\U00002056', BA }, { U'\U00002057', AL },
		{ U'\U0000205C', AL }, { U'\U00002060', WJ },
		{ U'\U0000207D', OP }, { U'\U0000207E', CL },
		{ U'\U0000208D', OP }, { U'\U0000208E', CL },
		{ U'\U000020A7', PO }, { U'\U000020B6', PO },
		{ U'\U000020BB', PO }, { U'\U000020BC', PR },
		{ U'\U000020BD', PR }, { U'\U000020BE', PO },
		{ U'\U00002103', PO }, { U'\U00002109', PO },
		{ U'\U00002116', PR }, { U'\U00002212', PR },
		{ U'\U00002213', PR }, { U'\U000022EF', IN },
		{ U'\U00002308', OP }, { U'\U00002309', CL },
		{ U'\U0000230A', OP }, { U'\U0000230B', CL },
		{ U'\U0000231A', ID }, { U'\U0000231B', ID },
		{ U'\U00002329', OP }, { U'\U0000232A', CL },
		{ U'\U00002614', ID }, { U'\U00002615', ID },
		{ U'\U00002616', AL }, { U'\U00002617', AL },
		{ U'\U00002618', ID }, { U'\U00002619', AL },
		{ U'\U0000261D', EB }, { U'\U0000261E', ID },
		{ U'\U0000261F', ID }, { U'\U00002668', ID },
		{ U'\U0000267F', ID }, { U'\U000026CD', ID },
		{ U'\U000026CE', AL }, { U'\U000026D2', AL },
		{ U'\U000026D3', ID }, { U'\U000026D4', ID },
		{ U'\U000026D8', ID }, { U'\U000026D9', ID },
		{ U'\U000026DA', AL }, { U'\U000026DB', AL },
		{ U'\U000026DC', ID }, { U'\U000026DD', AL },
		{ U'\U000026DE', AL }, { U'\U000026EA', ID },
		{ U'\U000026F6', AL }, { U'\U000026F7', ID },
		{ U'\U000026F8', ID }, { U'\U000026F9', EB },
		{ U'\U000026FA', ID }, { U'\U000026FB', AL },
		{ U'\U000026FC', AL }, { U'\U00002708', ID },
		{ U'\U00002709', ID }, { U'\U00002761', AL },
		{ U'\U00002762', EX }, { U'\U00002763', EX },
		{ U'\U00002764', ID }, { U'\U00002768', OP },
		{ U'\U00002769', CL }, { U'\U0000276A', OP },
		{ U'\U0000276B', CL }, { U'\U0000276C', OP },
		{ U'\U0000276D', CL }, { U'\U0000276E', OP },
		{ U'\U0000276F', CL }, { U'\U00002770', OP },
		{ U'\U00002771', CL }, { U'\U00002772', OP },
		{ U'\U00002773', CL }, { U'\U00002774', OP },
		{ U'\U00002775', CL }, { U'\U000027C5', OP },
		{ U'\U000027C6', CL }, { U'\U000027E6', OP },
		{ U'\U000027E7', CL }, { U'\U000027E8', OP },
		{ U'\U000027E9', CL }, { U'\U000027EA', OP },
		{ U'\U000027EB', CL }, { U'\U000027EC', OP },
		{ U'\U000027ED', CL }, { U'\U000027EE', OP },
		{ U'\U000027EF', CL }, { U'\U00002983', OP },
		{ U'\U00002984', CL }, { U'\U00002985', OP },
		{ U'\U00002986', CL }, { U'\U00002987', OP },
		{ U'\U00002988', CL }, { U'\U00002989', OP },
		{ U'\U0000298A', CL }, { U'\U0000298B', OP },
		{ U'\U0000298C', CL }, { U'\U0000298D', OP },
		{ U'\U0000298E', CL }, { U'\U0000298F', OP },
		{ U'\U00002990', CL }, { U'\U00002991', OP },
		{ U'\U00002992', CL }, { U'\U00002993', OP },
		{ U'\U00002994', CL }, { U'\U00002995', OP },
		{ U'\U00002996', CL }, { U'\U00002997', OP },
		{ U'\U00002998', CL }, { U'\U000029D8', OP },
		{ U'\U000029D9', CL }, { U'\U000029DA', OP },
		{ U'\U000029DB', CL }, { U'\U000029FC', OP },
		{ U'\U000029FD', CL }, { U'\U00002CF9', EX },
		{ U'\U00002CFD', AL }, { U'\U00002CFE', EX },
		{ U'\U00002CFF', BA }, { U'\U00002D70', BA },
		{ U'\U00002D7F', CM }, { U'\U00002E16', AL },
		{ U'\U00002E17', BA }, { U'\U00002E18', OP },
		{ U'\U00002E19', BA }, { U'\U00002E1A', AL },
		{ U'\U00002E1B', AL }, { U'\U00002E1C', QU },
		{ U'\U00002E1D', QU }, { U'\U00002E1E', AL },
		{ U'\U00002E1F', AL }, { U'\U00002E20', QU },
		{ U'\U00002E21', QU }, { U'\U00002E22', OP },
		{ U'\U00002E23', CL }, { U'\U00002E24', OP },
		{ U'\U00002E25', CL }, { U'\U00002E26', OP },
		{ U'\U00002E27', CL }, { U'\U00002E28', OP },
		{ U'\U00002E29', CL }, { U'\U00002E2E', EX },
		{ U'\U00002E2F', AL }, { U'\U00002E30', BA },
		{ U'\U00002E31', BA }, { U'\U00002E32', AL },
		{ U'\U00002E33', BA }, { U'\U00002E34', BA },
		{ U'\U00002E3A', B2 }, { U'\U00002E3B', B2 },
		{ U'\U00002E3F', AL }, { U'\U00002E40', BA },
		{ U'\U00002E41', BA }, { U'\U00002E42', OP },
		{ U'\U00002E9A', AL }, { U'\U00003000', BA },
		{ U'\U00003001', CL }, { U'\U00003002', CL },
		{ U'\U00003003', ID }, { U'\U00003004', ID },
		{ U'\U00003005', NS }, { U'\U00003006', ID },
		{ U'\U00003007', ID }, { U'\U00003008', OP },
		{ U'\U00003009', CL }, { U'\U0000300A', OP },
		{ U'\U0000300B', CL }, { U'\U0000300C', OP },
		{ U'\U0000300D', CL }, { U'\U0000300E', OP },
		{ U'\U0000300F', CL }, { U'\U00003010', OP },
		{ U'\U00003011', CL }, { U'\U00003012', ID },
		{ U'\U00003013', ID }, { U'\U00003014', OP },
		{ U'\U00003015', CL }, { U'\U00003016', OP },
		{ U'\U00003017', CL }, { U'\U00003018', OP },
		{ U'\U00003019', CL }, { U'\U0000301A', OP },
		{ U'\U0000301B', CL }, { U'\U0000301C', NS },
		{ U'\U0000301D', OP }, { U'\U0000301E', CL },
		{ U'\U0000301F', CL }, { U'\U00003035', CM },
		{ U'\U0000303B', NS }, { U'\U0000303C', NS },
		{ U'\U00003040', AL }, { U'\U00003041', NS },
		{ U'\U00003042', ID }, { U'\U00003043', NS },
		{ U'\U00003044', ID }, { U'\U00003045', NS },
		{ U'\U00003046', ID }, { U'\U00003047', NS },
		{ U'\U00003048', ID }, { U'\U00003049', NS },
		{ U'\U00003063', NS }, { U'\U00003083', NS },
		{ U'\U00003084', ID }, { U'\U00003085', NS },
		{ U'\U00003086', ID }, { U'\U00003087', NS },
		{ U'\U0000308E', NS }, { U'\U00003095', NS },
		{ U'\U00003096', NS }, { U'\U00003097', AL },
		{ U'\U00003098', AL }, { U'\U00003099', CM },
		{ U'\U0000309A', CM }, { U'\U0000309F', ID },
		{ U'\U000030A0', NS }, { U'\U000030A1', NS },
		{ U'\U000030A2', ID }, { U'\U000030A3', NS },
		{ U'\U000030A4', ID }, { U'\U000030A5', NS },
		{ U'\U000030A6', ID }, { U'\U000030A7', NS },
		{ U'\U000030A8', ID }, { U'\U000030A9', NS },
		{ U'\U000030C3', NS }, { U'\U000030E3', NS },
		{ U'\U000030E4', ID }, { U'\U000030E5', NS },
		{ U'\U000030E6', ID }, { U'\U000030E7', NS },
		{ U'\U000030EE', NS }, { U'\U000030F5', NS },
		{ U'\U000030F6', NS }, { U'\U000030FF', ID },
		{ U'\U0000312F', AL }, { U'\U00003130', AL },
		{ U'\U0000318F', AL }, { U'\U0000321F', AL },
		{ U'\U000032FF', AL }, { U'\U0000A015', NS },
		{ U'\U0000A4FE', BA }, { U'\U0000A4FF', BA },
		{ U'\U0000A60D', BA }, { U'\U0000A60E', EX },
		{ U'\U0000A60F', BA }, { U'\U0000A673', AL },
		{ U'\U0000A69E', CM }, { U'\U0000A69F', CM },
		{ U'\U0000A6F0', CM }, { U'\U0000A6F1', CM },
		{ U'\U0000A6F2', AL }, { U'\U0000A802', CM },
		{ U'\U0000A806', CM }, { U'\U0000A80B', CM },
		{ U'\U0000A838', PO }, { U'\U0000A874', BB },
		{ U'\U0000A875', BB }, { U'\U0000A876', EX },
		{ U'\U0000A877', EX }, { U'\U0000A880', CM },
		{ U'\U0000A881', CM }, { U'\U0000A8CE', BA },
		{ U'\U0000A8CF', BA }, { U'\U0000A8FC', BB },
		{ U'\U0000A92E', BA }, { U'\U0000A92F', BA },
		{ U'\U0000A9E5', CM }, { U'\U0000AA43', CM },
		{ U'\U0000AA4C', CM }, { U'\U0000AA4D', CM },
		{ U'\U0000AA4E', AL }, { U'\U0000AA4F', AL },
		{ U'\U0000AAB0', CM }, { U'\U0000AAB1', AL },
		{ U'\U0000AAB5', AL }, { U'\U0000AAB6', AL },
		{ U'\U0000AAB7', CM }, { U'\U0000AAB8', CM },
		{ U'\U0000AABE', CM }, { U'\U0000AABF', CM },
		{ U'\U0000AAC0', AL }, { U'\U0000AAC1', CM },
		{ U'\U0000AAF0', BA }, { U'\U0000AAF1', BA },
		{ U'\U0000AAF5', CM }, { U'\U0000AAF6', CM },
		{ U'\U0000ABEB', BA }, { U'\U0000ABEC', CM },
		{ U'\U0000ABED', CM }, { U'\U0000ABEE', AL },
		{ U'\U0000ABEF', AL }, { U'\U0000AC00', H2 },
		{ U'\U0000AC1C', H2 }, { U'\U0000AC38', H2 },
		{ U'\U0000AC54', H2 }, { U'\U0000AC70', H2 },
		{ U'\U0000AC8C', H2 }, { U'\U0000ACA8', H2 },
		{ U'\U0000ACC4', H2 }, { U'\U0000ACE0', H2 },
		{ U'\U0000ACFC', H2 }, { U'\U0000AD18', H2 },
		{ U'\U0000AD34', H2 }, { U'\U0000AD50', H2 },
		{ U'\U0000AD6C', H2 }, { U'\U0000AD88', H2 },
		{ U'\U0000ADA4', H2 }, { U'\U0000ADC0', H2 },
		{ U'\U0000ADDC', H2 }, { U'\U0000ADF8', H2 },
		{ U'\U0000AE14', H2 }, { U'\U0000AE30', H2 },
		{ U'\U0000AE4C', H2 }, { U'\U0000AE68', H2 },
		{ U'\U0000AE84', H2 }, { U'\U0000AEA0', H2 },
		{ U'\U0000AEBC', H2 }, { U'\U0000AED8', H2 },
		{ U'\U0000AEF4', H2 }, { U'\U0000AF10', H2 },
		{ U'\U0000AF2C', H2 }, { U'\U0000AF48', H2 },
		{ U'\U0000AF64', H2 }, { U'\U0000AF80', H2 },
		{ U'\U0000AF9C', H2 }, { U'\U0000AFB8', H2 },
		{ U'\U0000AFD4', H2 }, { U'\U0000AFF0', H2 },
		{ U'\U0000B00C', H2 }, { U'\U0000B028', H2 },
		{ U'\U0000B044', H2 }, { U'\U0000B060', H2 },
		{ U'\U0000B07C', H2 }, { U'\U0000B098', H2 },
		{ U'\U0000B0B4', H2 }, { U'\U0000B0D0', H2 },
		{ U'\U0000B0EC', H2 }, { U'\U0000B108', H2 },
		{ U'\U0000B124', H2 }, { U'\U0000B140', H2 },
		{ U'\U0000B15C', H2 }, { U'\U0000B178', H2 },
		{ U'\U0000B194', H2 }, { U'\U0000B1B0', H2 },
		{ U'\U0000B1CC', H2 }, { U'\U0000B1E8', H2 },
		{ U'\U0000B204', H2 }, { U'\U0000B220', H2 },
		{ U'\U0000B23C', H2 }, { U'\U0000B258', H2 },
		{ U'\U0000B274', H2 }, { U'\U0000B290', H2 },
		{ U'\U0000B2AC', H2 }, { U'\U0000B2C8', H2 },
		{ U'\U0000B2E4', H2 }, { U'\U0000B300', H2 },
		{ U'\U0000B31C', H2 }, { U'\U0000B338', H2 },
		{ U'\U0000B354', H2 }, { U'\U0000B370', H2 },
		{ U'\U0000B38C', H2 }, { U'\U0000B3A8', H2 },
		{ U'\U0000B3C4', H2 }, { U'\U0000B3E0', H2 },
		{ U'\U0000B3FC', H2 }, { U'\U0000B418', H2 },
		{ U'\U0000B434', H2 }, { U'\U0000B450', H2 },
		{ U'\U0000B46C', H2 }, { U'\U0000B488', H2 },
		{ U'\U0000B4A4', H2 }, { U'\U0000B4C0', H2 },
		{ U'\U0000B4DC', H2 }, { U'\U0000B4F8', H2 },
		{ U'\U0000B514', H2 }, { U'\U0000B530', H2 },
		{ U'\U0000B54C', H2 }, { U'\U0000B568', H2 },
		{ U'\U0000B584', H2 }, { U'\U0000B5A0', H2 },
		{ U'\U0000B5BC', H2 }, { U'\U0000B5D8', H2 },
		{ U'\U0000B5F4', H2 }, { U'\U0000B610', H2 },
		{ U'\U0000B62C', H2 }, { U'\U0000B648', H2 },
		{ U'\U0000B664', H2 }, { U'\U0000B680', H2 },
		{ U'\U0000B69C', H2 }, { U'\U0000B6B8', H2 },
		{ U'\U0000B6D4', H2 }, { U'\U0000B6F0', H2 },
		{ U'\U0000B70C', H2 }, { U'\U0000B728', H2 },
		{ U'\U0000B744', H2 }, { U'\U0000B760', H2 },
		{ U'\U0000B77C', H2 }, { U'\U0000B798', H2 },
		{ U'\U0000B7B4', H2 }, { U'\U0000B7D0', H2 },
		{ U'\U0000B7EC', H2 }, { U'\U0000B808', H2 },
		{ U'\U0000B824', H2 }, { U'\U0000B840', H2 },
		{ U'\U0000B85C', H2 }, { U'\U0000B878', H2 },
		{ U'\U0000B894', H2 }, { U'\U0000B8B0', H2 },
		{ U'\U0000B8CC', H2 }, { U'\U0000B8E8', H2 },
		{ U'\U0000B904', H2 }, { U'\U0000B920', H2 },
		{ U'\U0000B93C', H2 }, { U'\U0000B958', H2 },
		{ U'\U0000B974', H2 }, { U'\U0000B990', H2 },
		{ U'\U0000B9AC', H2 }, { U'\U0000B9C8', H2 },
		{ U'\U0000B9E4', H2 }, { U'\U0000BA00', H2 },
		{ U'\U0000BA1C', H2 }, { U'\U0000BA38', H2 },
		{ U'\U0000BA54', H2 }, { U'\U0000BA70', H2 },
		{ U'\U0000BA8C', H2 }, { U'\U0000BAA8', H2 },
		{ U'\U0000BAC4', H2 }, { U'\U0000BAE0', H2 },
		{ U'\U0000BAFC', H2 }, { U'\U0000BB18', H2 },
		{ U'\U0000BB34', H2 }, { U'\U0000BB50', H2 },
		{ U'\U0000BB6C', H2 }, { U'\U0000BB88', H2 },
		{ U'\U0000BBA4', H2 }, { U'\U0000BBC0', H2 },
		{ U'\U0000BBDC', H2 }, { U'\U0000BBF8', H2 },
		{ U'\U0000BC14', H2 }, { U'\U0000BC30', H2 },
		{ U'\U0000BC4C', H2 }, { U'\U0000BC68', H2 },
		{ U'\U0000BC84', H2 }, { U'\U0000BCA0', H2 },
		{ U'\U0000BCBC', H2 }, { U'\U0000BCD8', H2 },
		{ U'\U0000BCF4', H2 }, { U'\U0000BD10', H2 },
		{ U'\U0000BD2C', H2 }, { U'\U0000BD48', H2 },
		{ U'\U0000BD64', H2 }, { U'\U0000BD80', H2 },
		{ U'\U0000BD9C', H2 }, { U'\U0000BDB8', H2 },
		{ U'\U0000BDD4', H2 }, { U'\U0000BDF0', H2 },
		{ U'\U0000BE0C', H2 }, { U'\U0000BE28', H2 },
		{ U'\U0000BE44', H2 }, { U'\U0000BE60', H2 },
		{ U'\U0000BE7C', H2 }, { U'\U0000BE98', H2 },
		{ U'\U0000BEB4', H2 }, { U'\U0000BED0', H2 },
		{ U'\U0000BEEC', H2 }, { U'\U0000BF08', H2 },
		{ U'\U0000BF24', H2 }, { U'\U0000BF40', H2 },
		{ U'\U0000BF5C', H2 }, { U'\U0000BF78', H2 },
		{ U'\U0000BF94', H2 }, { U'\U0000BFB0', H2 },
		{ U'\U0000BFCC', H2 }, { U'\U0000BFE8', H2 },
		{ U'\U0000C004', H2 }, { U'\U0000C020', H2 },
		{ U'\U0000C03C', H2 }, { U'\U0000C058', H2 },
		{ U'\U0000C074', H2 }, { U'\U0000C090', H2 },
		{ U'\U0000C0AC', H2 }, { U'\U0000C0C8', H2 },
		{ U'\U0000C0E4', H2 }, { U'\U0000C100', H2 },
		{ U'\U0000C11C', H2 }, { U'\U0000C138', H2 },
		{ U'\U0000C154', H2 }, { U'\U0000C170', H2 },
		{ U'\U0000C18C', H2 }, { U'\U0000C1A8', H2 },
		{ U'\U0000C1C4', H2 }, { U'\U0000C1E0', H2 },
		{ U'\U0000C1FC', H2 }, { U'\U0000C218', H2 },
		{ U'\U0000C234', H2 }, { U'\U0000C250', H2 },
		{ U'\U0000C26C', H2 }, { U'\U0000C288', H2 },
		{ U'\U0000C2A4', H2 }, { U'\U0000C2C0', H2 },
		{ U'\U0000C2DC', H2 }, { U'\U0000C2F8', H2 },
		{ U'\U0000C314', H2 }, { U'\U0000C330', H2 },
		{ U'\U0000C34C', H2 }, { U'\U0000C368', H2 },
		{ U'\U0000C384', H2 }, { U'\U0000C3A0', H2 },
		{ U'\U0000C3BC', H2 }, { U'\U0000C3D8', H2 },
		{ U'\U0000C3F4', H2 }, { U'\U0000C410', H2 },
		{ U'\U0000C42C', H2 }, { U'\U0000C448', H2 },
		{ U'\U0000C464', H2 }, { U'\U0000C480', H2 },
		{ U'\U0000C49C', H2 }, { U'\U0000C4B8', H2 },
		{ U'\U0000C4D4', H2 }, { U'\U0000C4F0', H2 },
		{ U'\U0000C50C', H2 }, { U'\U0000C528', H2 },
		{ U'\U0000C544', H2 }, { U'\U0000C560', H2 },
		{ U'\U0000C57C', H2 }, { U'\U0000C598', H2 },
		{ U'\U0000C5B4', H2 }, { U'\U0000C5D0', H2 },
		{ U'\U0000C5EC', H2 }, { U'\U0000C608', H2 },
		{ U'\U0000C624', H2 }, { U'\U0000C640', H2 },
		{ U'\U0000C65C', H2 }, { U'\U0000C678', H2 },
		{ U'\U0000C694', H2 }, { U'\U0000C6B0', H2 },
		{ U'\U0000C6CC', H2 }, { U'\U0000C6E8', H2 },
		{ U'\U0000C704', H2 }, { U'\U0000C720', H2 },
		{ U'\U0000C73C', H2 }, { U'\U0000C758', H2 },
		{ U'\U0000C774', H2 }, { U'\U0000C790', H2 },
		{ U'\U0000C7AC', H2 }, { U'\U0000C7C8', H2 },
		{ U'\U0000C7E4', H2 }, { U'\U0000C800', H2 },
		{ U'\U0000C81C', H2 }, { U'\U0000C838', H2 },
		{ U'\U0000C854', H2 }, { U'\U0000C870', H2 },
		{ U'\U0000C88C', H2 }, { U'\U0000C8A8', H2 },
		{ U'\U0000C8C4', H2 }, { U'\U0000C8E0', H2 },
		{ U'\U0000C8FC', H2 }, { U'\U0000C918', H2 },
		{ U'\U0000C934', H2 }, { U'\U0000C950', H2 },
		{ U'\U0000C96C', H2 }, { U'\U0000C988', H2 },
		{ U'\U0000C9A4', H2 }, { U'\U0000C9C0', H2 },
		{ U'\U0000C9DC', H2 }, { U'\U0000C9F8', H2 },
		{ U'\U0000CA14', H2 }, { U'\U0000CA30', H2 },
		{ U'\U0000CA4C', H2 }, { U'\U0000CA68', H2 },
		{ U'\U0000CA84', H2 }, { U'\U0000CAA0', H2 },
		{ U'\U0000CABC', H2 }, { U'\U0000CAD8', H2 },
		{ U'\U0000CAF4', H2 }, { U'\U0000CB10', H2 },
		{ U'\U0000CB2C', H2 }, { U'\U0000CB48', H2 },
		{ U'\U0000CB64', H2 }, { U'\U0000CB80', H2 },
		{ U'\U0000CB9C', H2 }, { U'\U0000CBB8', H2 },
		{ U'\U0000CBD4', H2 }, { U'\U0000CBF0', H2 },
		{ U'\U0000CC0C', H2 }, { U'\U0000CC28', H2 },
		{ U'\U0000CC44', H2 }, { U'\U0000CC60', H2 },
		{ U'\U0000CC7C', H2 }, { U'\U0000CC98', H2 },
		{ U'\U0000CCB4', H2 }, { U'\U0000CCD0', H2 },
		{ U'\U0000CCEC', H2 }, { U'\U0000CD08', H2 },
		{ U'\U0000CD24', H2 }, { U'\U0000CD40', H2 },
		{ U'\U0000CD5C', H2 }, { U'\U0000CD78', H2 },
		{ U'\U0000CD94', H2 }, { U'\U0000CDB0', H2 },
		{ U'\U0000CDCC', H2 }, { U'\U0000CDE8', H2 },
		{ U'\U0000CE04', H2 }, { U'\U0000CE20', H2 },
		{ U'\U0000CE3C', H2 }, { U'\U0000CE58', H2 },
		{ U'\U0000CE74', H2 }, { U'\U0000CE90', H2 },
		{ U'\U0000CEAC', H2 }, { U'\U0000CEC8', H2 },
		{ U'\U0000CEE4', H2 }, { U'\U0000CF00', H2 },
		{ U'\U0000CF1C', H2 }, { U'\U0000CF38', H2 },
		{ U'\U0000CF54', H2 }, { U'\U0000CF70', H2 },
		{ U'\U0000CF8C', H2 }, { U'\U0000CFA8', H2 },
		{ U'\U0000CFC4', H2 }, { U'\U0000CFE0', H2 },
		{ U'\U0000CFFC', H2 }, { U'\U0000D018', H2 },
		{ U'\U0000D034', H2 }, { U'\U0000D050', H2 },
		{ U'\U0000D06C', H2 }, { U'\U0000D088', H2 },
		{ U'\U0000D0A4', H2 }, { U'\U0000D0C0', H2 },
		{ U'\U0000D0DC', H2 }, { U'\U0000D0F8', H2 },
		{ U'\U0000D114', H2 }, { U'\U0000D130', H2 },
		{ U'\U0000D14C', H2 }, { U'\U0000D168', H2 },
		{ U'\U0000D184', H2 }, { U'\U0000D1A0', H2 },
		{ U'\U0000D1BC', H2 }, { U'\U0000D1D8', H2 },
		{ U'\U0000D1F4', H2 }, { U'\U0000D210', H2 },
		{ U'\U0000D22C', H2 }, { U'\U0000D248', H2 },
		{ U'\U0000D264', H2 }, { U'\U0000D280', H2 },
		{ U'\U0000D29C', H2 }, { U'\U0000D2B8', H2 },
		{ U'\U0000D2D4', H2 }, { U'\U0000D2F0', H2 },
		{ U'\U0000D30C', H2 }, { U'\U0000D328', H2 },
		{ U'\U0000D344', H2 }, { U'\U0000D360', H2 },
		{ U'\U0000D37C', H2 }, { U'\U0000D398', H2 },
		{ U'\U0000D3B4', H2 }, { U'\U0000D3D0', H2 },
		{ U'\U0000D3EC', H2 }, { U'\U0000D408', H2 },
		{ U'\U0000D424', H2 }, { U'\U0000D440', H2 },
		{ U'\U0000D45C', H2 }, { U'\U0000D478', H2 },
		{ U'\U0000D494', H2 }, { U'\U0000D4B0', H2 },
		{ U'\U0000D4CC', H2 }, { U'\U0000D4E8', H2 },
		{ U'\U0000D504', H2 }, { U'\U0000D520', H2 },
		{ U'\U0000D53C', H2 }, { U'\U0000D558', H2 },
		{ U'\U0000D574', H2 }, { U'\U0000D590', H2 },
		{ U'\U0000D5AC', H2 }, { U'\U0000D5C8', H2 },
		{ U'\U0000D5E4', H2 }, { U'\U0000D600', H2 },
		{ U'\U0000D61C', H2 }, { U'\U0000D638', H2 },
		{ U'\U0000D654', H2 }, { U'\U0000D670', H2 },
		{ U'\U0000D68C', H2 }, { U'\U0000D6A8', H2 },
		{ U'\U0000D6C4', H2 }, { U'\U0000D6E0', H2 },
		{ U'\U0000D6FC', H2 }, { U'\U0000D718', H2 },
		{ U'\U0000D734', H2 }, { U'\U0000D750', H2 },
		{ U'\U0000D76C', H2 }, { U'\U0000D788', H2 },
		{ U'\U0000FB1D', HL }, { U'\U0000FB1E', CM },
		{ U'\U0000FB29', AL }, { U'\U0000FB37', AL },
		{ U'\U0000FB3D', AL }, { U'\U0000FB3E', HL },
		{ U'\U0000FB3F', AL }, { U'\U0000FB40', HL },
		{ U'\U0000FB41', HL }, { U'\U0000FB42', AL },
		{ U'\U0000FB43', HL }, { U'\U0000FB44', HL },
		{ U'\U0000FB45', AL }, { U'\U0000FD3E', CL },
		{ U'\U0000FD3F', OP }, { U'\U0000FDFC', PO },
		{ U'\U0000FE10', IS }, { U'\U0000FE11', CL },
		{ U'\U0000FE12', CL }, { U'\U0000FE13', IS },
		{ U'\U0000FE14', IS }, { U'\U0000FE15', EX },
		{ U'\U0000FE16', EX }, { U'\U0000FE17', OP },
		{ U'\U0000FE18', CL }, { U'\U0000FE19', IN },
		{ U'\U0000FE35', OP }, { U'\U0000FE36', CL },
		{ U'\U0000FE37', OP }, { U'\U0000FE38', CL },
		{ U'\U0000FE39', OP }, { U'\U0000FE3A', CL },
		{ U'\U0000FE3B', OP }, { U'\U0000FE3C', CL },
		{ U'\U0000FE3D', OP }, { U'\U0000FE3E', CL },
		{ U'\U0000FE3F', OP }, { U'\U0000FE40', CL },
		{ U'\U0000FE41', OP }, { U'\U0000FE42', CL },
		{ U'\U0000FE43', OP }, { U'\U0000FE44', CL },
		{ U'\U0000FE45', ID }, { U'\U0000FE46', ID },
		{ U'\U0000FE47', OP }, { U'\U0000FE48', CL },
		{ U'\U0000FE50', CL }, { U'\U0000FE51', ID },
		{ U'\U0000FE52', CL }, { U'\U0000FE53', AL },
		{ U'\U0000FE54', NS }, { U'\U0000FE55', NS },
		{ U'\U0000FE56', EX }, { U'\U0000FE57', EX },
		{ U'\U0000FE58', ID }, { U'\U0000FE59', OP },
		{ U'\U0000FE5A', CL }, { U'\U0000FE5B', OP },
		{ U'\U0000FE5C', CL }, { U'\U0000FE5D', OP },
		{ U'\U0000FE5E', CL }, { U'\U0000FE67', AL },
		{ U'\U0000FE68', ID }, { U'\U0000FE69', PR },
		{ U'\U0000FE6A', PO }, { U'\U0000FE6B', ID },
		{ U'\U0000FEFF', WJ }, { U'\U0000FF00', AL },
		{ U'\U0000FF01', EX }, { U'\U0000FF02', ID },
		{ U'\U0000FF03', ID }, { U'\U0000FF04', PR },
		{ U'\U0000FF05', PO }, { U'\U0000FF06', ID },
		{ U'\U0000FF07', ID }, { U'\U0000FF08', OP },
		{ U'\U0000FF09', CL }, { U'\U0000FF0A', ID },
		{ U'\U0000FF0B', ID }, { U'\U0000FF0C', CL },
		{ U'\U0000FF0D', ID }, { U'\U0000FF0E', CL },
		{ U'\U0000FF1A', NS }, { U'\U0000FF1B', NS },
		{ U'\U0000FF1F', EX }, { U'\U0000FF3B', OP },
		{ U'\U0000FF3C', ID }, { U'\U0000FF3D', CL },
		{ U'\U0000FF5B', OP }, { U'\U0000FF5C', ID },
		{ U'\U0000FF5D', CL }, { U'\U0000FF5E', ID },
		{ U'\U0000FF5F', OP }, { U'\U0000FF60', CL },
		{ U'\U0000FF61', CL }, { U'\U0000FF62', OP },
		{ U'\U0000FF63', CL }, { U'\U0000FF64', CL },
		{ U'\U0000FF65', NS }, { U'\U0000FF66', ID },
		{ U'\U0000FF9E', NS }, { U'\U0000FF9F', NS },
		{ U'\U0000FFC8', AL }, { U'\U0000FFC9', AL },
		{ U'\U0000FFD0', AL }, { U'\U0000FFD1', AL },
		{ U'\U0000FFD8', AL }, { U'\U0000FFD9', AL },
		{ U'\U0000FFE0', PO }, { U'\U0000FFE1', PR },
		{ U'\U0000FFE5', PR }, { U'\U0000FFE6', PR },
		{ U'\U0000FFFC', CB }, { U'\U000101FD', CM },
		{ U'\U000102E0', CM }, { U'\U0001039F', BA },
		{ U'\U000103D0', BA }, { U'\U00010857', BA },
		{ U'\U0001091F', BA }, { U'\U00010A04', AL },
		{ U'\U00010A05', CM }, { U'\U00010A06', CM },
		{ U'\U00010A3F', CM }, { U'\U00010AE5', CM },
		{ U'\U00010AE6', CM }, { U'\U00010AF6', IN },
		{ U'\U00011047', BA }, { U'\U00011048', BA },
		{ U'\U00011135', AL }, { U'\U00011173', CM },
		{ U'\U00011174', AL }, { U'\U00011175', BB },
		{ U'\U000111C5', BA }, { U'\U000111C6', BA },
		{ U'\U000111C7', AL }, { U'\U000111C8', BA },
		{ U'\U000111C9', AL }, { U'\U000111DA', AL },
		{ U'\U000111DB', BB }, { U'\U000111DC', AL },
		{ U'\U00011238', BA }, { U'\U00011239', BA },
		{ U'\U0001123A', AL }, { U'\U0001123B', BA },
		{ U'\U0001123C', BA }, { U'\U0001123D', AL },
		{ U'\U0001123E', CM }, { U'\U000112A9', BA },
		{ U'\U0001133C', CM }, { U'\U0001133D', AL },
		{ U'\U00011345', AL }, { U'\U00011346', AL },
		{ U'\U00011347', CM }, { U'\U00011348', CM },
		{ U'\U00011349', AL }, { U'\U0001134A', AL },
		{ U'\U00011357', CM }, { U'\U00011362', CM },
		{ U'\U00011363', CM }, { U'\U00011364', AL },
		{ U'\U00011365', AL }, { U'\U0001144F', AL },
		{ U'\U0001145A', AL }, { U'\U0001145B', BA },
		{ U'\U000115B6', AL }, { U'\U000115B7', AL },
		{ U'\U000115C1', BB }, { U'\U000115C2', BA },
		{ U'\U000115C3', BA }, { U'\U000115C4', EX },
		{ U'\U000115C5', EX }, { U'\U000115DC', CM },
		{ U'\U000115DD', CM }, { U'\U00011641', BA },
		{ U'\U00011642', BA }, { U'\U0001173A', AL },
		{ U'\U0001173B', AL }, { U'\U00011A3A', AL },
		{ U'\U00011A3F', BB }, { U'\U00011A40', AL },
		{ U'\U00011A45', BB }, { U'\U00011A46', AL },
		{ U'\U00011A47', CM }, { U'\U00011A9D', AL },
		{ U'\U00011AA1', BA }, { U'\U00011AA2', BA },
		{ U'\U00011C37', AL }, { U'\U00011C40', AL },
		{ U'\U00011C70', BB }, { U'\U00011C71', EX },
		{ U'\U00011CA8', AL }, { U'\U00011D3A', CM },
		{ U'\U00011D3B', AL }, { U'\U00011D3C', CM },
		{ U'\U00011D3D', CM }, { U'\U00011D3E', AL },
		{ U'\U00011D46', AL }, { U'\U00011D47', CM },
		{ U'\U00013282', CL }, { U'\U00013286', OP },
		{ U'\U00013287', CL }, { U'\U00013288', OP },
		{ U'\U00013289', CL }, { U'\U00013379', OP },
		{ U'\U0001337A', CL }, { U'\U0001337B', CL },
		{ U'\U000145CE', OP }, { U'\U000145CF', CL },
		{ U'\U00016A6E', BA }, { U'\U00016A6F', BA },
		{ U'\U00016AF5', BA }, { U'\U00016B44', BA },
		{ U'\U00016FE0', NS }, { U'\U00016FE1', NS },
		{ U'\U0001BC9D', CM }, { U'\U0001BC9E', CM },
		{ U'\U0001BC9F', BA }, { U'\U0001D183', AL },
		{ U'\U0001D184', AL }, { U'\U0001DA75', CM },
		{ U'\U0001DA84', CM }, { U'\U0001DA85', AL },
		{ U'\U0001DA86', AL }, { U'\U0001DAA0', AL },
		{ U'\U0001E007', AL }, { U'\U0001E019', AL },
		{ U'\U0001E01A', AL }, { U'\U0001E022', AL },
		{ U'\U0001E023', CM }, { U'\U0001E024', CM },
		{ U'\U0001E025', AL }, { U'\U0001E95E', OP },
		{ U'\U0001E95F', OP }, { U'\U0001F12F', ID },
		{ U'\U0001F385', EB }, { U'\U0001F39C', AL },
		{ U'\U0001F39D', AL }, { U'\U0001F3B5', AL },
		{ U'\U0001F3B6', AL }, { U'\U0001F3BC', AL },
		{ U'\U0001F3C5', ID }, { U'\U0001F3C6', ID },
		{ U'\U0001F3C7', EB }, { U'\U0001F3C8', ID },
		{ U'\U0001F3C9', ID }, { U'\U0001F442', EB },
		{ U'\U0001F443', EB }, { U'\U0001F444', ID },
		{ U'\U0001F445', ID }, { U'\U0001F46E', EB },
		{ U'\U0001F46F', ID }, { U'\U0001F47C', EB },
		{ U'\U0001F484', ID }, { U'\U0001F4A0', AL },
		{ U'\U0001F4A1', ID }, { U'\U0001F4A2', AL },
		{ U'\U0001F4A3', ID }, { U'\U0001F4A4', AL },
		{ U'\U0001F4AA', EB }, { U'\U0001F4AF', AL },
		{ U'\U0001F4B0', ID }, { U'\U0001F4B1', AL },
		{ U'\U0001F4B2', AL }, { U'\U0001F574', EB },
		{ U'\U0001F575', EB }, { U'\U0001F57A', EB },
		{ U'\U0001F590', EB }, { U'\U0001F595', EB },
		{ U'\U0001F596', EB }, { U'\U0001F6A3', EB },
		{ U'\U0001F6C0', EB }, { U'\U0001F6CC', EB },
		{ U'\U0001F91D', ID }, { U'\U0001F91E', EB },
		{ U'\U0001F91F', EB }, { U'\U0001F926', EB },
		{ U'\U0001F93D', EB }, { U'\U0001F93E', EB },
		{ U'\U0001FFFE', AL }, { U'\U0001FFFF', AL },
		{ U'\U0002FFFE', AL }, { U'\U0002FFFF', AL },
		{ U'\U000E0001', CM },
	};
	
	
	
	// The type of element in 2nd priority table.
	class LineBreakPropertyRange {
	public:
		char32_t maxCodePoint;
		LineBreakProperty property;
	};
	
	// Less operator.
	bool operator<(const LineBreakPropertyRange &a, const LineBreakPropertyRange &b)
	{
		return a.maxCodePoint < b.maxCodePoint;
	}
	
	// 2nd priority Line_Break Property table.
	// This is a sorted array because of using lower_bound.
	const LineBreakPropertyRange lineBreakProperties2nd[] =
	{
		{ U'\U0000002F', CM }, { U'\U0000003B', NU },
		{ U'\U0000007E', AL }, { U'\U000000A2', CM },
		{ U'\U000000A5', PR }, { U'\U000002FF', AL },
		{ U'\U0000035B', CM }, { U'\U00000362', GL },
		{ U'\U0000036F', CM }, { U'\U00000482', AL },
		{ U'\U00000489', CM }, { U'\U00000590', AL },
		{ U'\U000005C7', CM }, { U'\U000005CF', AL },
		{ U'\U000005EA', HL }, { U'\U000005EF', AL },
		{ U'\U000005F2', HL }, { U'\U00000608', AL },
		{ U'\U0000060F', PO }, { U'\U0000061F', CM },
		{ U'\U0000064A', AL }, { U'\U0000065F', CM },
		{ U'\U0000066C', NU }, { U'\U000006D5', AL },
		{ U'\U000006EF', CM }, { U'\U000006F9', NU },
		{ U'\U0000072F', AL }, { U'\U0000074A', CM },
		{ U'\U000007A5', AL }, { U'\U000007B0', CM },
		{ U'\U000007BF', AL }, { U'\U000007C9', NU },
		{ U'\U000007EA', AL }, { U'\U000007F3', CM },
		{ U'\U00000815', AL }, { U'\U0000082D', CM },
		{ U'\U00000858', AL }, { U'\U0000085B', CM },
		{ U'\U000008D3', AL }, { U'\U00000903', CM },
		{ U'\U00000939', AL }, { U'\U00000957', CM },
		{ U'\U00000965', AL }, { U'\U0000096F', NU },
		{ U'\U00000980', AL }, { U'\U00000983', CM },
		{ U'\U000009BD', AL }, { U'\U000009CD', CM },
		{ U'\U000009E5', AL }, { U'\U000009F3', NU },
		{ U'\U00000A00', AL }, { U'\U00000A03', CM },
		{ U'\U00000A3D', AL }, { U'\U00000A42', CM },
		{ U'\U00000A4A', AL }, { U'\U00000A4D', CM },
		{ U'\U00000A65', AL }, { U'\U00000A71', NU },
		{ U'\U00000A80', AL }, { U'\U00000A83', CM },
		{ U'\U00000ABD', AL }, { U'\U00000ACD', CM },
		{ U'\U00000AE5', AL }, { U'\U00000AF1', NU },
		{ U'\U00000AF9', AL }, { U'\U00000B03', CM },
		{ U'\U00000B3D', AL }, { U'\U00000B4D', CM },
		{ U'\U00000B65', AL }, { U'\U00000B6F', NU },
		{ U'\U00000BBD', AL }, { U'\U00000BC2', CM },
		{ U'\U00000BC5', AL }, { U'\U00000BCD', CM },
		{ U'\U00000BE5', AL }, { U'\U00000BEF', NU },
		{ U'\U00000BFF', AL }, { U'\U00000C03', CM },
		{ U'\U00000C3D', AL }, { U'\U00000C4D', CM },
		{ U'\U00000C65', AL }, { U'\U00000C6F', NU },
		{ U'\U00000C80', AL }, { U'\U00000C83', CM },
		{ U'\U00000CBD', AL }, { U'\U00000CCD', CM },
		{ U'\U00000CE5', AL }, { U'\U00000CEF', NU },
		{ U'\U00000CFF', AL }, { U'\U00000D03', CM },
		{ U'\U00000D3D', AL }, { U'\U00000D4D', CM },
		{ U'\U00000D65', AL }, { U'\U00000D6F', NU },
		{ U'\U00000DCE', AL }, { U'\U00000DDF', CM },
		{ U'\U00000DE5', AL }, { U'\U00000DF3', NU },
		{ U'\U00000E33', AL }, { U'\U00000E3A', CM },
		{ U'\U00000E46', AL }, { U'\U00000E4F', CM },
		{ U'\U00000E5B', NU }, { U'\U00000EB3', AL },
		{ U'\U00000EBC', CM }, { U'\U00000EC7', AL },
		{ U'\U00000ECF', CM }, { U'\U00000ED9', NU },
		{ U'\U00000F00', AL }, { U'\U00000F0C', BB },
		{ U'\U00000F14', EX }, { U'\U00000F1F', AL },
		{ U'\U00000F29', NU }, { U'\U00000F70', AL },
		{ U'\U00000F87', CM }, { U'\U00000F8C', AL },
		{ U'\U00000FBF', CM }, { U'\U0000102A', AL },
		{ U'\U0000103F', CM }, { U'\U0000104B', NU },
		{ U'\U00001055', AL }, { U'\U00001059', CM },
		{ U'\U0000105D', AL }, { U'\U0000106D', CM },
		{ U'\U00001070', AL }, { U'\U00001074', CM },
		{ U'\U00001081', AL }, { U'\U0000108F', CM },
		{ U'\U00001099', NU }, { U'\U0000109D', CM },
		{ U'\U000010FF', AL }, { U'\U0000115F', JL },
		{ U'\U000011A7', JV }, { U'\U000011FF', JT },
		{ U'\U0000135C', AL }, { U'\U00001361', CM },
		{ U'\U000016EA', AL }, { U'\U000016ED', BA },
		{ U'\U00001711', AL }, { U'\U00001714', CM },
		{ U'\U00001731', AL }, { U'\U00001736', CM },
		{ U'\U000017B3', AL }, { U'\U000017DF', CM },
		{ U'\U000017E9', NU }, { U'\U0000180A', AL },
		{ U'\U0000180F', CM }, { U'\U00001819', NU },
		{ U'\U0000191F', AL }, { U'\U0000192B', CM },
		{ U'\U0000192F', AL }, { U'\U0000193B', CM },
		{ U'\U00001945', AL }, { U'\U0000194F', NU },
		{ U'\U000019CF', AL }, { U'\U000019D9', NU },
		{ U'\U00001A16', AL }, { U'\U00001A1B', CM },
		{ U'\U00001A54', AL }, { U'\U00001A7F', CM },
		{ U'\U00001A89', NU }, { U'\U00001A8F', AL },
		{ U'\U00001A99', NU }, { U'\U00001AAF', AL },
		{ U'\U00001ABE', CM }, { U'\U00001AFF', AL },
		{ U'\U00001B04', CM }, { U'\U00001B33', AL },
		{ U'\U00001B44', CM }, { U'\U00001B4F', AL },
		{ U'\U00001B5C', NU }, { U'\U00001B60', BA },
		{ U'\U00001B6A', AL }, { U'\U00001B73', CM },
		{ U'\U00001B7F', AL }, { U'\U00001B82', CM },
		{ U'\U00001BA0', AL }, { U'\U00001BAF', CM },
		{ U'\U00001BB9', NU }, { U'\U00001BE5', AL },
		{ U'\U00001BF3', CM }, { U'\U00001C23', AL },
		{ U'\U00001C37', CM }, { U'\U00001C3A', AL },
		{ U'\U00001C3F', BA }, { U'\U00001C49', NU },
		{ U'\U00001C4F', AL }, { U'\U00001C59', NU },
		{ U'\U00001CCF', AL }, { U'\U00001CE8', CM },
		{ U'\U00001CF1', AL }, { U'\U00001CF9', CM },
		{ U'\U00001DBF', AL }, { U'\U00001DFF', CM },
		{ U'\U00001FFF', AL }, { U'\U00002014', BA },
		{ U'\U0000201A', AL }, { U'\U0000201F', QU },
		{ U'\U00002023', AL }, { U'\U00002029', IN },
		{ U'\U0000202F', CM }, { U'\U0000203D', PO },
		{ U'\U00002046', AL }, { U'\U00002049', NS },
		{ U'\U00002057', AL }, { U'\U00002060', BA },
		{ U'\U00002065', AL }, { U'\U0000206F', CM },
		{ U'\U0000209F', AL }, { U'\U000020CF', PR },
		{ U'\U000020F0', CM }, { U'\U000023EF', AL },
		{ U'\U000023F3', ID }, { U'\U000025FF', AL },
		{ U'\U00002603', ID }, { U'\U00002619', AL },
		{ U'\U0000261F', ID }, { U'\U00002638', AL },
		{ U'\U0000263B', ID }, { U'\U000026BC', AL },
		{ U'\U000026C8', ID }, { U'\U000026CE', AL },
		{ U'\U000026D4', ID }, { U'\U000026DE', AL },
		{ U'\U000026E1', ID }, { U'\U000026F0', AL },
		{ U'\U00002704', ID }, { U'\U00002709', AL },
		{ U'\U0000270D', EB }, { U'\U0000275A', AL },
		{ U'\U00002764', QU }, { U'\U00002CEE', AL },
		{ U'\U00002CF1', CM }, { U'\U00002CF9', AL },
		{ U'\U00002CFF', BA }, { U'\U00002DDF', AL },
		{ U'\U00002DFF', CM }, { U'\U00002E0D', QU },
		{ U'\U00002E34', BA }, { U'\U00002E3B', AL },
		{ U'\U00002E49', BA }, { U'\U00002E7F', AL },
		{ U'\U00002EF3', ID }, { U'\U00002EFF', AL },
		{ U'\U00002FD5', ID }, { U'\U00002FEF', AL },
		{ U'\U00002FFB', ID }, { U'\U0000301F', AL },
		{ U'\U00003029', ID }, { U'\U0000302F', CM },
		{ U'\U0000309A', ID }, { U'\U000030A9', NS },
		{ U'\U000030FA', ID }, { U'\U000030FF', NS },
		{ U'\U00003104', AL }, { U'\U000031BA', ID },
		{ U'\U000031BF', AL }, { U'\U000031E3', ID },
		{ U'\U000031EF', AL }, { U'\U000031FF', NS },
		{ U'\U00003247', ID }, { U'\U0000324F', AL },
		{ U'\U00004DBF', ID }, { U'\U00004DFF', AL },
		{ U'\U0000A48C', ID }, { U'\U0000A48F', AL },
		{ U'\U0000A4C6', ID }, { U'\U0000A61F', AL },
		{ U'\U0000A629', NU }, { U'\U0000A66E', AL },
		{ U'\U0000A67D', CM }, { U'\U0000A6F2', AL },
		{ U'\U0000A6F7', BA }, { U'\U0000A822', AL },
		{ U'\U0000A827', CM }, { U'\U0000A8B3', AL },
		{ U'\U0000A8C5', CM }, { U'\U0000A8CF', AL },
		{ U'\U0000A8D9', NU }, { U'\U0000A8DF', AL },
		{ U'\U0000A8F1', CM }, { U'\U0000A8FF', AL },
		{ U'\U0000A909', NU }, { U'\U0000A925', AL },
		{ U'\U0000A92F', CM }, { U'\U0000A946', AL },
		{ U'\U0000A953', CM }, { U'\U0000A95F', AL },
		{ U'\U0000A97C', JL }, { U'\U0000A97F', AL },
		{ U'\U0000A983', CM }, { U'\U0000A9B2', AL },
		{ U'\U0000A9C0', CM }, { U'\U0000A9C6', AL },
		{ U'\U0000A9C9', BA }, { U'\U0000A9CF', AL },
		{ U'\U0000A9D9', NU }, { U'\U0000A9EF', AL },
		{ U'\U0000A9F9', NU }, { U'\U0000AA28', AL },
		{ U'\U0000AA36', CM }, { U'\U0000AA4F', AL },
		{ U'\U0000AA59', NU }, { U'\U0000AA5C', AL },
		{ U'\U0000AA5F', BA }, { U'\U0000AA7A', AL },
		{ U'\U0000AA7D', CM }, { U'\U0000AAB1', AL },
		{ U'\U0000AAB8', CM }, { U'\U0000AAEA', AL },
		{ U'\U0000AAF1', CM }, { U'\U0000ABE2', AL },
		{ U'\U0000ABEF', CM }, { U'\U0000ABF9', NU },
		{ U'\U0000AC00', AL }, { U'\U0000D7A3', H3 },
		{ U'\U0000D7AF', AL }, { U'\U0000D7C6', JV },
		{ U'\U0000D7CA', AL }, { U'\U0000D7FB', JT },
		{ U'\U0000F8FF', AL }, { U'\U0000FAFF', ID },
		{ U'\U0000FB1E', AL }, { U'\U0000FB4F', HL },
		{ U'\U0000FDFF', AL }, { U'\U0000FE19', CM },
		{ U'\U0000FE1F', AL }, { U'\U0000FE2F', CM },
		{ U'\U0000FE6B', ID }, { U'\U0000FF0E', AL },
		{ U'\U0000FF66', ID }, { U'\U0000FF70', NS },
		{ U'\U0000FFBE', ID }, { U'\U0000FFC1', AL },
		{ U'\U0000FFDC', ID }, { U'\U0000FFE1', AL },
		{ U'\U0000FFE6', ID }, { U'\U0000FFF8', AL },
		{ U'\U0000FFFC', CM }, { U'\U000100FF', AL },
		{ U'\U00010102', BA }, { U'\U00010375', AL },
		{ U'\U0001037A', CM }, { U'\U0001049F', AL },
		{ U'\U000104A9', NU }, { U'\U00010A00', AL },
		{ U'\U00010A06', CM }, { U'\U00010A0B', AL },
		{ U'\U00010A0F', CM }, { U'\U00010A37', AL },
		{ U'\U00010A3A', CM }, { U'\U00010A4F', AL },
		{ U'\U00010A57', BA }, { U'\U00010AEF', AL },
		{ U'\U00010AF6', BA }, { U'\U00010B38', AL },
		{ U'\U00010B3F', BA }, { U'\U00010FFF', AL },
		{ U'\U00011002', CM }, { U'\U00011037', AL },
		{ U'\U00011048', CM }, { U'\U00011065', AL },
		{ U'\U0001106F', NU }, { U'\U0001107E', AL },
		{ U'\U00011082', CM }, { U'\U000110AF', AL },
		{ U'\U000110BA', CM }, { U'\U000110BD', AL },
		{ U'\U000110C1', BA }, { U'\U000110EF', AL },
		{ U'\U000110F9', NU }, { U'\U000110FF', AL },
		{ U'\U00011102', CM }, { U'\U00011126', AL },
		{ U'\U00011135', CM }, { U'\U0001113F', NU },
		{ U'\U00011143', BA }, { U'\U0001117F', AL },
		{ U'\U00011182', CM }, { U'\U000111B2', AL },
		{ U'\U000111C0', CM }, { U'\U000111C9', AL },
		{ U'\U000111CC', CM }, { U'\U000111CF', AL },
		{ U'\U000111DC', NU }, { U'\U000111DF', BA },
		{ U'\U0001122B', AL }, { U'\U0001123E', CM },
		{ U'\U000112DE', AL }, { U'\U000112EA', CM },
		{ U'\U000112EF', AL }, { U'\U000112F9', NU },
		{ U'\U000112FF', AL }, { U'\U00011303', CM },
		{ U'\U0001133D', AL }, { U'\U0001134D', CM },
		{ U'\U00011365', AL }, { U'\U0001136C', CM },
		{ U'\U0001136F', AL }, { U'\U00011374', CM },
		{ U'\U00011434', AL }, { U'\U00011446', CM },
		{ U'\U0001144A', AL }, { U'\U0001144F', BA },
		{ U'\U0001145B', NU }, { U'\U000114AF', AL },
		{ U'\U000114C3', CM }, { U'\U000114CF', AL },
		{ U'\U000114D9', NU }, { U'\U000115AE', AL },
		{ U'\U000115C5', CM }, { U'\U000115C8', AL },
		{ U'\U000115D7', BA }, { U'\U0001162F', AL },
		{ U'\U00011642', CM }, { U'\U0001164F', AL },
		{ U'\U00011659', NU }, { U'\U0001165F', AL },
		{ U'\U0001166C', BB }, { U'\U000116AA', AL },
		{ U'\U000116B7', CM }, { U'\U000116BF', AL },
		{ U'\U000116C9', NU }, { U'\U0001171C', AL },
		{ U'\U0001172B', CM }, { U'\U0001172F', AL },
		{ U'\U0001173B', NU }, { U'\U0001173E', BA },
		{ U'\U000118DF', AL }, { U'\U000118E9', NU },
		{ U'\U00011A00', AL }, { U'\U00011A0A', CM },
		{ U'\U00011A32', AL }, { U'\U00011A40', CM },
		{ U'\U00011A47', BA }, { U'\U00011A50', AL },
		{ U'\U00011A5B', CM }, { U'\U00011A89', AL },
		{ U'\U00011A99', CM }, { U'\U00011A9D', BA },
		{ U'\U00011AA2', BB }, { U'\U00011C2E', AL },
		{ U'\U00011C40', CM }, { U'\U00011C45', BA },
		{ U'\U00011C4F', AL }, { U'\U00011C59', NU },
		{ U'\U00011C91', AL }, { U'\U00011CB6', CM },
		{ U'\U00011D30', AL }, { U'\U00011D36', CM },
		{ U'\U00011D3E', AL }, { U'\U00011D47', CM },
		{ U'\U00011D4F', AL }, { U'\U00011D59', NU },
		{ U'\U0001246F', AL }, { U'\U00012474', BA },
		{ U'\U00013257', AL }, { U'\U0001325A', OP },
		{ U'\U0001325D', CL }, { U'\U00016A5F', AL },
		{ U'\U00016A69', NU }, { U'\U00016AEF', AL },
		{ U'\U00016AF5', CM }, { U'\U00016B2F', AL },
		{ U'\U00016B36', CM }, { U'\U00016B39', BA },
		{ U'\U00016B4F', AL }, { U'\U00016B59', NU },
		{ U'\U00016F50', AL }, { U'\U00016F7E', CM },
		{ U'\U00016F8E', AL }, { U'\U00016F92', CM },
		{ U'\U00016FFF', AL }, { U'\U000187EC', ID },
		{ U'\U000187FF', AL }, { U'\U00018AF2', ID },
		{ U'\U0001AFFF', AL }, { U'\U0001B11E', ID },
		{ U'\U0001B16F', AL }, { U'\U0001B2FB', ID },
		{ U'\U0001BC9F', AL }, { U'\U0001BCA3', CM },
		{ U'\U0001D164', AL }, { U'\U0001D169', CM },
		{ U'\U0001D16C', AL }, { U'\U0001D18B', CM },
		{ U'\U0001D1A9', AL }, { U'\U0001D1AD', CM },
		{ U'\U0001D241', AL }, { U'\U0001D244', CM },
		{ U'\U0001D7CD', AL }, { U'\U0001D7FF', NU },
		{ U'\U0001D9FF', AL }, { U'\U0001DA36', CM },
		{ U'\U0001DA3A', AL }, { U'\U0001DA6C', CM },
		{ U'\U0001DA86', AL }, { U'\U0001DA8A', BA },
		{ U'\U0001DA9A', AL }, { U'\U0001DAAF', CM },
		{ U'\U0001DFFF', AL }, { U'\U0001E02A', CM },
		{ U'\U0001E8CF', AL }, { U'\U0001E8D6', CM },
		{ U'\U0001E943', AL }, { U'\U0001E94A', CM },
		{ U'\U0001E94F', AL }, { U'\U0001E959', NU },
		{ U'\U0001EFFF', AL }, { U'\U0001F0FF', ID },
		{ U'\U0001F10C', AL }, { U'\U0001F10F', ID },
		{ U'\U0001F16B', AL }, { U'\U0001F16F', ID },
		{ U'\U0001F1AC', AL }, { U'\U0001F1E5', ID },
		{ U'\U0001F1FF', RI }, { U'\U0001F3C1', ID },
		{ U'\U0001F3CC', EB }, { U'\U0001F3FA', ID },
		{ U'\U0001F3FF', EM }, { U'\U0001F445', ID },
		{ U'\U0001F450', EB }, { U'\U0001F465', ID },
		{ U'\U0001F469', EB }, { U'\U0001F46F', ID },
		{ U'\U0001F478', EB }, { U'\U0001F480', ID },
		{ U'\U0001F487', EB }, { U'\U0001F4FF', ID },
		{ U'\U0001F506', AL }, { U'\U0001F516', ID },
		{ U'\U0001F524', AL }, { U'\U0001F531', ID },
		{ U'\U0001F549', AL }, { U'\U0001F5D3', ID },
		{ U'\U0001F5DB', AL }, { U'\U0001F5F3', ID },
		{ U'\U0001F5F9', AL }, { U'\U0001F644', ID },
		{ U'\U0001F647', EB }, { U'\U0001F64A', ID },
		{ U'\U0001F64F', EB }, { U'\U0001F675', AL },
		{ U'\U0001F678', QU }, { U'\U0001F67B', NS },
		{ U'\U0001F67F', AL }, { U'\U0001F6B3', ID },
		{ U'\U0001F6B6', EB }, { U'\U0001F6FF', ID },
		{ U'\U0001F773', AL }, { U'\U0001F77F', ID },
		{ U'\U0001F7D4', AL }, { U'\U0001F7FF', ID },
		{ U'\U0001F80B', AL }, { U'\U0001F80F', ID },
		{ U'\U0001F847', AL }, { U'\U0001F84F', ID },
		{ U'\U0001F859', AL }, { U'\U0001F85F', ID },
		{ U'\U0001F887', AL }, { U'\U0001F88F', ID },
		{ U'\U0001F8AD', AL }, { U'\U0001F8FF', ID },
		{ U'\U0001F90B', AL }, { U'\U0001F917', ID },
		{ U'\U0001F91F', EB }, { U'\U0001F92F', ID },
		{ U'\U0001F939', EB }, { U'\U0001F9D0', ID },
		{ U'\U0001F9DD', EB }, { U'\U0003FFFD', ID },
		{ U'\U000E001F', AL }, { U'\U000E007F', CM },
		{ U'\U000E00FF', AL }, { U'\U000E01EF', CM },
		{ U'\U0010FFFD', AL },
	};
	
	
	
	// get the line break property of a character.
	LineBreakProperty getLineBreakProperty(const char32_t c)
	{
		const auto it = lineBreakProperties1st.find(c);
		if(it != lineBreakProperties1st.end())
			return it->second;
		else
		{
			const LineBreakPropertyRange target{ c, AL };
			const auto it2 = lower_bound(begin(lineBreakProperties2nd), end(lineBreakProperties2nd), target);
			if(it2 != end(lineBreakProperties2nd))
				return it2->property;
			else
				return AL;
		}
	}
	
	
	
	// Set LineBreakOpportunity if its potision has not been determined yet.
	void setLineBreakOpportunity(LineBreakOpportunity &var, const LineBreakOpportunity value)
	{
		if(var == WEAK_BREAK_ALLOWED)
			var = value;
	}
	
	
	
	// function needToScan*(total)
	// param[in] total: bitwise OR of all Line_Break properties in a text.
	// result: need to call scan*()
	//
	// function scan*(prop, opportunity)
	// param[inout] prop: vector of Line_Break properties in a text.
	// param[out] opportunity: vector of line break opportunities in a text.
	// result: bitwise OR of Line_Break properties that this function may add.
	
	// LB2 Never break at the start of text.
	// sot #
	// Nothing to do it.
	//
	// LB3 Always break at the end of text.
	// ! eot
	bool needToScanLB3(LineBreakProperty)
	{
		return true;
	}
	
	LineBreakProperty scanLB3(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		setLineBreakOpportunity(opportunity[prop.size()-1], MANDATORY_BREAK);
		return LBP_EMPTY;
	}
	
	
	
	// LB4 Always break after hard line breaks.
	// BK !
	bool needToScanLB4(LineBreakProperty total)
	{
		return total & BK;
	}
	
	LineBreakProperty scanLB4(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		for(size_t i = 0; i < prop.size(); ++i)
			if(prop[i] == BK)
				setLineBreakOpportunity(opportunity[i], MANDATORY_BREAK);
		return LBP_EMPTY;
	}
	
	
	
	// LB5 Treat CR followed by LF, as well as CR, LF, and NL as hard line breaks.
	// CR # LF
	// CR !
	// LF !
	// NL !
	//
	// LB6 Do not break before hard line breaks.
	// # ( BK | CR | LF | NL )
	bool needToScanLB5_6(LineBreakProperty total)
	{
		return total & (BK | CR | LF | NL);
	}
	
	LineBreakProperty scanLB5_6(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		const size_t sz = prop.size();
		for(size_t i = 0; i < sz; ++i)
		{
			if(prop[i] == CR)
			{
				if(i < sz-1 && prop[i+1] == LF)
					setLineBreakOpportunity(opportunity[i], NO_BREAK_ALLOWED);
				else
					setLineBreakOpportunity(opportunity[i], MANDATORY_BREAK);
			}
			else if(prop[i] & (LF |NL))
				setLineBreakOpportunity(opportunity[i], MANDATORY_BREAK);
			
			if(i > 0)
				if(prop[i] & (BK | CR | LF | NL))
					setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
		}
		return LBP_EMPTY;
	}
	
	
	
	// LB7 Do not break before spaces or zero width space.
	// # ( SP | ZW )
	bool needToScanLB7(LineBreakProperty total)
	{
		return total & (SP | ZW);
	}
	
	LineBreakProperty scanLB7(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		for(size_t i = 1; i < prop.size(); ++i)
			if(prop[i] & (SP | ZW))
				setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
		return LBP_EMPTY;
	}
	
	
	
	// LB8 Break before any character following a zero-width space, even if one
	//     or more spaces intervene.
	// ZW SP* %
	bool needToScanLB8(LineBreakProperty total)
	{
		return total & ZW;
	}
	
	LineBreakProperty scanLB8(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		bool isAfterZW = false;
		for(size_t i = 0; i < prop.size(); ++i)
		{
			if(prop[i] != SP && isAfterZW)
			{
				setLineBreakOpportunity(opportunity[i-1], BREAK_ALLOWED);
				isAfterZW = false;
			}
			if(prop[i] == ZW)
				isAfterZW = true;
		}
		return LBP_EMPTY;
	}
	
	
	
	// LB8a Do not break between a zero width joiner and an ideograph, emoji
	//      base or emoji modifier.
	// ZWJ # (ID | EB | EM)
	bool needToScanLB8a(LineBreakProperty total)
	{
		return (total & ZWJ) && (total & (ID | EB | EM));
	}
	
	LineBreakProperty scanLB8a(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		for(size_t i = 0; i < prop.size() - 1; ++i)
			if(prop[i] == ZWJ)
				if(prop[i+1] & (ID | EB | EM))
					setLineBreakOpportunity(opportunity[i], NO_BREAK_ALLOWED);
		return LBP_EMPTY;
	}
	
	
	
	// LB9 Do not break a combining character sequence; treat it as if it has
	//     the line breaking class of the base character in all of the following
	//     rules. Treat ZWJ as if it were CM.
	// Treat X (CM | ZWJ)* as if it were X.
	// where X is any line break class except BK, CR, LF, NL, SP, or ZW.
	//
	// LB10 Treat any remaining combining mark or ZWJ as AL.
	// Treat any remaining CM or ZWJ as it if were AL.
	bool needToScanLB9_10(LineBreakProperty total)
	{
		return total & (CM | ZWJ);
	}
	
	LineBreakProperty scanLB9_10(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		LineBreakProperty r = LBP_EMPTY;
		LineBreakProperty x = BK;
		for(size_t i = 0; i < prop.size(); ++i)
			if(prop[i] & (CM | ZWJ))
			{
				if(x & (BK | CR | LF | NL | SP | ZW))
				{
					prop[i] = AL;
					r = AL;
				}
				else
				{
					prop[i] = x;
					setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
				}
			}
			else
				x = prop[i];
		return r;
	}
	
	
	
	// LB11 Do not break before or after Word joiner and related characters.
	// # WJ
	// WJ #
	bool needToScanLB11(LineBreakProperty total)
	{
		return total & WJ;
	}
	
	LineBreakProperty scanLB11(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		for(size_t i = 0; i < prop.size(); ++i)
			if(prop[i] == WJ)
			{
				setLineBreakOpportunity(opportunity[i], NO_BREAK_ALLOWED);
				if(i > 0)
					setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
			}
		return LBP_EMPTY;
	}
	
	
	
	// LB12 Do not break after NBSP and related characters.
	// GL #
	//
	// LB12a Do not break before NBSP and related characters, except after
	//       spaces and hyphens.
	// [^SP BA HY] # GL
	bool needToScanLB12_12a(LineBreakProperty total)
	{
		return total & GL;
	}
	
	LineBreakProperty scanLB12_12a(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		for(size_t i = 0; i < prop.size(); ++i)
			if(prop[i] == GL)
			{
				setLineBreakOpportunity(opportunity[i], NO_BREAK_ALLOWED);
				if(i > 0 && !(prop[i-1] & (SP | BA | HY)))
					setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
			}
		return LBP_EMPTY;
	}
	
	
	
	// LB13 Do not break before ']' or '!' or ';' or '/', even after spaces.
	// # CL
	// # CP
	// # EX
	// # IS
	// # SY
	bool needToScanLB13(LineBreakProperty total)
	{
		return total & (CL | CP | EX | IS | SY);
	}
	
	LineBreakProperty scanLB13(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		for(size_t i = 1; i < prop.size(); ++i)
			if(prop[i] & (CL | CP | EX | IS | SY))
				setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
		return LBP_EMPTY;
	}
	
	
	
	// LB14 Do not break after '[', even after spaces.
	// OP SP* #
	//
	// LB15 Do not break within '"[', even with intervening spaces.
	// QU SP* # OP
	bool needToScanLB14_15(LineBreakProperty total)
	{
		return total & OP;
	}
	
	LineBreakProperty scanLB14_15(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	
	{
		bool isAfterOP = false;
		bool isAfterQU = false;
		for(size_t i = 0; i < prop.size(); ++i)
		{
			if(prop[i] != SP)
			{
				if (isAfterOP || (isAfterQU && prop[i] == OP))
					setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
				isAfterOP = prop[i] == OP;
				isAfterQU = prop[i] == QU;
			}
		}
		return LBP_EMPTY;
	}
	
	
	
	// LB16 Do not break between closing punctuation and a nonstarter (lb=NS),
	//      even with intervening spaces.
	// (CL | CP) SP* # NS
	bool needToScanLB16(LineBreakProperty total)
	{
		return (total & (CL | CP)) && (total & NS);
	}
	
	LineBreakProperty scanLB16(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		bool isAfterCLCP = false;
		for(size_t i = 0; i < prop.size(); ++i)
		{
			if(isAfterCLCP && prop[i] == NS)
				setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
			
			if(prop[i] & (CL | CP))
				isAfterCLCP = true;
			else if(prop[i] != SP)
				isAfterCLCP = false;
		}
		return LBP_EMPTY;
	}
	
	
	
	// LB17 Do not break within (TWO/THREE-)EM DASH + (TWO/THREE-)EM DASH, even
	//      with intervening spaces.
	// B2 SP* # B2
	bool needToScanLB17(LineBreakProperty total)
	{
		return total & B2;
	}
	
	LineBreakProperty scanLB17(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		bool isAfterB2 = false;
		for(size_t i = 0; i < prop.size(); ++i)
		{
			if(prop[i] == B2)
			{
				if(isAfterB2)
					setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
				isAfterB2 = true;
			}
			else if(prop[i] != SP)
				isAfterB2 = false;
		}
		return LBP_EMPTY;
	}
	
	
	
	// LB18 Break after spaces.
	// SP %
	bool needToScanLB18(LineBreakProperty total)
	{
		return total & SP;
	}
	
	LineBreakProperty scanLB18(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		for(size_t i = 0; i < prop.size(); ++i)
			if(prop[i] == SP)
				setLineBreakOpportunity(opportunity[i], BREAK_ALLOWED);
		return LBP_EMPTY;
	}
	
	
	
	// LB19 Do not break before or after quotation marks, such as '"'.
	// # QU
	// QU #
	bool needToScanLB19(LineBreakProperty total)
	{
		return total & QU;
	}
	
	LineBreakProperty scanLB19(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		for(size_t i = 0; i < prop.size(); ++i)
			if(prop[i] == QU)
			{
				setLineBreakOpportunity(opportunity[i], NO_BREAK_ALLOWED);
				if(i > 0)
					setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
			}
		return LBP_EMPTY;
	}
	
	
	
	// LB20 Break before and after unresolved CB.
	// % CB
	// CB %
	bool needToScanLB20(LineBreakProperty total)
	{
		return total & CB;
	}
	
	LineBreakProperty scanLB20(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		for(size_t i = 0; i < prop.size(); ++i)
			if(prop[i] == CB)
			{
				setLineBreakOpportunity(opportunity[i], BREAK_ALLOWED);
				if(i > 0)
					setLineBreakOpportunity(opportunity[i-1], BREAK_ALLOWED);
			}
		return LBP_EMPTY;
	}
	
	
	
	// LB21 Do not break before hyphen-minus, other hyphens, fixed-width spaces,
	//      small kana, and other non-starters, or after acute accents.
	// # BA
	// # HY
	// # NS
	// BB #
	bool needToScanLB21(LineBreakProperty total)
	{
		return total & (BA | HY | NS | BB);
	}
	
	LineBreakProperty scanLB21(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		for(size_t i = 0; i < prop.size(); ++i)
		{
			if(i > 0 && (prop[i] & (BA | HY | NS)))
				setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
			if(prop[i] == BB)
				setLineBreakOpportunity(opportunity[i], NO_BREAK_ALLOWED);
		}
		return LBP_EMPTY;
	}
	
	
	
	// LB21a Don't break after Hebrew + Hyphen.
	// HL (HY | BA) #
	bool needToScanLB21a(LineBreakProperty total)
	{
		return (total & HL) && (total & (HY | BA));
	}
	
	LineBreakProperty scanLB21a(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		for(size_t i = 1; i < prop.size(); ++i)
			if(prop[i-1] == HL && (prop[i] & (HY | BA)))
				setLineBreakOpportunity(opportunity[i], NO_BREAK_ALLOWED);
		return LBP_EMPTY;
	}
	
	
	
	// LB21b Don't break between Solidus and Hebrew letters.
	// SY # HL
	bool needToScanLB21b(LineBreakProperty total)
	{
		return (total & SY) && (total & HL);
	}
	
	LineBreakProperty scanLB21b(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		for(size_t i = 1; i < prop.size(); ++i)
			if(prop[i-1] == SY && prop[i] == HL)
				setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
		return LBP_EMPTY;
	}
	
	
	
	// LB22 Do not break between two ellipses, or between letters, numbers or
	//      exclamations and ellipsis.
	// (AL | HL) # IN
	// EX # IN
	// (ID | EB | EM) # IN
	// IN # IN
	// NU # IN
	bool needToScanLB22(LineBreakProperty total)
	{
		return total & IN;
	}
	
	LineBreakProperty scanLB22(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		for(size_t i = 1; i < prop.size(); ++i)
			if(prop[i-1] & (AL | HL | EX | ID | EB | EM | IN | NU))
				if(prop[i] == IN)
					setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
		return LBP_EMPTY;
	}
	
	
	
	// LB23 Do not break between digits and letters.
	// (AL | HL) # NU
	// NU # (AL | HL)
	bool needToScanLB23(LineBreakProperty total)
	{
		return (total & (AL | HL)) && (total & NU);
	}
	
	LineBreakProperty scanLB23(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		for(size_t i = 1; i < prop.size(); ++i)
			if(prop[i-1] & (AL | HL))
			{
				if(prop[i] == NU)
					setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
			}
			else if(prop[i-1] == NU)
				if(prop[i] & (AL | HL))
					setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
		return LBP_EMPTY;
	}
	
	
	
	// LB23a Do not break between numeric prefixes and ideographs, or between
	//       ideographs and numeric postfixes.
	// PR # (ID | EB | EM)
	// (ID | EB | EM) # PO
	bool needToScanLB23a(LineBreakProperty total)
	{
		return (total & (PR | PO)) && (total & (ID | EB | EM));
	}
	
	LineBreakProperty scanLB23a(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		for(size_t i = 1; i < prop.size(); ++i)
			if(prop[i-1] == PR)
			{
				if(prop[i] & (ID | EB | EM))
					setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
			}
			else if(prop[i-1] & (ID | EB | EM))
				if(prop[i] == PO)
					setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
		return LBP_EMPTY;
	}
	
	
	
	// LB24 Do not break between numeric prefix/postfix and letters, or between
	//      letters and prefix/postfix.
	// (PR | PO) # (AL | HL)
	// (AL | HL) # (PR | PO)
	bool needToScanLB24(LineBreakProperty total)
	{
		return (total & (PR | PO)) && (total & (AL | HL));
	}
	
	LineBreakProperty scanLB24(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		for(size_t i = 1; i < prop.size(); ++i)
			if(prop[i-1] & (PR | PO))
			{
				if(prop[i] & (AL | HL))
					setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
			}
			else if(prop[i-1] & (AL | HL))
				if(prop[i] & (PR | PO))
					setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
		return LBP_EMPTY;
	}
	
	
	
	// LB25 Do not break between the following pairs of classes relevant to numbers:
	// CL # PO
	// CP # PO
	// CL # PR
	// CP # PR
	// NU # PO
	// NU # PR
	// PO # OP
	// PO # NU
	// PR # OP
	// PR # NU
	// HY # NU
	// IS # NU
	// NU # NU
	// SY # NU
	bool needToScanLB25(LineBreakProperty total)
	{
		return (total & (CL | CP | NU | PO | PR | HY | IS | SY)) && (total & (PO | PR | OP | NU));
	}
	
	LineBreakProperty scanLB25(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		for(size_t i = 1; i < prop.size(); ++i)
			if((prop[i-1] & (CL | CP | NU)) && (prop[i] & (PO | PR)))
				setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
			else if((prop[i-1] & (PO | PR)) && (prop[i] & (OP | NU)))
				setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
			else if((prop[i-1] & (HY | IS | NU | SY)) && (prop[i] == NU))
				setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
		return LBP_EMPTY;
	}
	
	
	
	// LB26 Do not break a Korean syllable.
	// JL # (JL | JV | H2 | H3)
	// (JV | H2) # (JV | JT)
	// (JT | H3) # JT
	bool needToScanLB26(LineBreakProperty total)
	{
		return total & (JL | JV | H2 | JT | H3);
	}
	
	LineBreakProperty scanLB26(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		for(size_t i = 1; i < prop.size(); ++i)
			if(prop[i-1] == JL)
			{
				if(prop[i] & (JL | JV | H2 | H3))
					setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
			}
			else if(prop[i-1] & (JV | H2))
			{
				if(prop[i] & (JV | JT))
					setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
			}
			else if(prop[i-1] & (JT | H3))
				if(prop[i] == JT)
					setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
		return LBP_EMPTY;
	}
	
	
	
	// LB27 Treat a Korean Syllable Block the same as ID.
	// (JL | JV | JT | H2 | H3) # IN
	// (JL | JV | JT | H2 | H3) # PO
	// PR # (JL | JV | JT | H2 | H3)
	bool needToScanLB27(LineBreakProperty total)
	{
		return (total & (JL | JV | H2 | JT | H3)) && (total & (IN | PO | PR));
	}
	
	LineBreakProperty scanLB27(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		for(size_t i = 1; i < prop.size(); ++i)
			if(prop[i-1] & (JL | JV | JT | H2 | H3))
			{
				if(prop[i] & (IN | PO))
					setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
			}
			else if(prop[i-1] == PR)
				if(prop[i] & (JL | JV | JT | H2 | H3))
					setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
		return LBP_EMPTY;
	}
	
	
	
	// LB28 Do not break between alphabetics ("at").
	// (AL | HL) # (AL | HL)
	bool needToScanLB28(LineBreakProperty total)
	{
		return total & (AL | HL);
	}
	
	LineBreakProperty scanLB28(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		for(size_t i = 1; i < prop.size(); ++i)
			if((prop[i-1] & (AL | HL)) && (prop[i] & (AL | HL)))
				setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
		return LBP_EMPTY;
	}
	
	
	
	// LB29 Do not break between numeric punctuation and alphabetics ("e.g.").
	// IS # (AL | HL)
	bool needToScanLB29(LineBreakProperty total)
	{
		return (total & IS) && (total & (AL | HL));
	}
	
	LineBreakProperty scanLB29(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		for(size_t i = 1; i < prop.size(); ++i)
			if(prop[i-1] == IS && (prop[i] & (AL | HL)))
				setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
		return LBP_EMPTY;
	}
	
	
	
	// LB30 Do not break between letters, numbers, or ordinary symbols and
	//      opening or closing parentheses.
	// (AL | HL | NU) # OP
	// CP # (AL | HL | NU)
	bool needToScanLB30(LineBreakProperty total)
	{
		return (total & (AL | HL | NU)) && (total & (OP | CP));
	}
	
	LineBreakProperty scanLB30(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		for(size_t i = 1; i < prop.size(); ++i)
			if(prop[i-1] & (AL | HL | NU))
			{
				if(prop[i] == OP)
					setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
			}
			else if(prop[i-1] == CP)
				if(prop[i] & (AL | HL | NU))
					setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
		return LBP_EMPTY;
	}
	
	
	
	// LB30a Break between two regional indicator symbols if and only if there
	//       are an even number of regional indicators preceding the position of
	//       the break.
	// sot (RI RI)* RI # RI
	// [^RI] (RI RI)* RI # RI
	bool needToScanLB30a(LineBreakProperty total)
	{
		return total & RI;
	}
	
	LineBreakProperty scanLB30a(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		enum { INITIAL, FOUND1ST, SEARCH2ND } state = INITIAL;
		for(size_t i = 0; i < prop.size(); ++i)
			if(prop[i] == RI)
			{
				// If opportunity[i] is not WEAK_BREAK_ALLOWED, treat as if it's
				// a single RI character. (LB9)
				const bool isCharacter = opportunity[i] == WEAK_BREAK_ALLOWED;
				if(state == INITIAL)
				{
					if(isCharacter)
						state = FOUND1ST;
				}
				else if(state == FOUND1ST)
				{
					setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
					if(isCharacter)
						state = INITIAL;
					else
						state = SEARCH2ND;
				}
				else
					if(isCharacter)
						state = INITIAL;
			}
			else
				state = INITIAL;
		return LBP_EMPTY;
	}
	
	
	
	// LB30b Do not break between an emoji base and an emoji modifier.
	// EB # EM
	bool needToScanLB30b(LineBreakProperty total)
	{
		return (total & EB) && (total & EM);
	}
	
	LineBreakProperty scanLB30b(vector<LineBreakProperty> &prop, vector<LineBreakOpportunity> &opportunity)
	{
		for(size_t i = 1; i < prop.size(); ++i)
			if(prop[i-1] == EB && prop[i] == EM)
				setLineBreakOpportunity(opportunity[i-1], NO_BREAK_ALLOWED);
		return LBP_EMPTY;
	}
	
	
	
	// Table of Line break procedures.
	class TableOfLineBreakProcedures {
	public:
		bool (*needToScan)(LineBreakProperty);
		LineBreakProperty (*scan)(vector<LineBreakProperty>&, vector<LineBreakOpportunity>&);
	};
	
	TableOfLineBreakProcedures tableOfLineBreakProcedures[] =
	{
		{ needToScanLB3, scanLB3 },
		{ needToScanLB4, scanLB4 },
		{ needToScanLB5_6, scanLB5_6 },
		{ needToScanLB7, scanLB7 },
		{ needToScanLB8, scanLB8 },
		{ needToScanLB8a, scanLB8a },
		{ needToScanLB9_10, scanLB9_10 },
		{ needToScanLB11, scanLB11 },
		{ needToScanLB12_12a, scanLB12_12a },
		{ needToScanLB13, scanLB13 },
		{ needToScanLB14_15, scanLB14_15 },
		{ needToScanLB16, scanLB16 },
		{ needToScanLB17, scanLB17 },
		{ needToScanLB18, scanLB18 },
		{ needToScanLB19, scanLB19 },
		{ needToScanLB20, scanLB20 },
		{ needToScanLB21, scanLB21 },
		{ needToScanLB21a, scanLB21a },
		{ needToScanLB21b, scanLB21b },
		{ needToScanLB22, scanLB22 },
		{ needToScanLB23, scanLB23 },
		{ needToScanLB23a, scanLB23a },
		{ needToScanLB24, scanLB24 },
		{ needToScanLB25, scanLB25 },
		{ needToScanLB26, scanLB26 },
		{ needToScanLB27, scanLB27 },
		{ needToScanLB28, scanLB28 },
		{ needToScanLB29, scanLB29 },
		{ needToScanLB30, scanLB30 },
		{ needToScanLB30a, scanLB30a },
		{ needToScanLB30b, scanLB30b },
	};
	
	// The end of a implementation of Unicode Line Breaking Algorithm.
	
	
	
	// To divide into some merged blocks and to prepare line-fitting.
	//
	// The subject to expansion is:
	// (1) U+0020 SPACE
	// (2) U+00A0 NO-BREAK SPACE
	// (3) after/before ID characters where is BREAK_ALLOWED.
	//
	// For western languages, (1) and (2) perform line-fitting.
	//
	// For japanese (and probably chinese), (3) is worked. This is not a
	// standard way, but very simple and almost no difference from a proper way,
	// such as https://www.w3.org/TR/jlreq/#line_adjustment, because
	// line-fitting process normally makes only a slight expansion for
	// japanese text.
	
	// Interword-spaces.
	// These are not drawn.
	unordered_set<char32_t> interwordSpaceCharacters
	{
		U'\U00000009', U'\U0000000A', U'\U0000000B', U'\U0000000C',
		U'\U0000000D', U'\U00000020', U'\U000000A0',
	};
	
	// Spaces.
	// These shall be removed at the end of line, otherwise drawn.
	// OGHAM SPACE MARK has a glyph.
	unordered_set<char32_t> spaceCharacters
	{
		U'\U00001680', U'\U00002000', U'\U00002001', U'\U00002002',
		U'\U00002003', U'\U00002004', U'\U00002005', U'\U00002006',
		U'\U00002007', U'\U00002008', U'\U00002009', U'\U0000200A',
		U'\U0000202F', U'\U0000205F', U'\U00003000',
	};
	
	
	
	// Per character information.
	class CharacterInformation {
	public:
		// The character.
		char32_t c;
		// The position in the UTF-8 text.
		size_t pos;
		size_t next;
		// This is a interword-space.
		bool isInterwordSpace;
		// This is a space.
		bool isSpace;
		// Line_Break Property.
		LineBreakProperty lineBreakProperty;
		// Line Break Opportunity.
		LineBreakOpportunity lineBreakOpportunity;
	};
	
	// get per character information.
	vector<CharacterInformation> getCharacterInformation(const string &s)
	{
		vector<CharacterInformation> info;
		vector<LineBreakProperty> props;
		char32_t pos = 0;
		LineBreakProperty total = LBP_EMPTY;
		while(pos < s.length())
		{
			const char32_t c = Font::DecodeCodePoint(s, pos);
			const size_t next = Font::NextCodePoint(s, pos);
			const bool isInterwordSpace = interwordSpaceCharacters.count(c);
			const bool isSpace = spaceCharacters.count(c);
			const LineBreakProperty p0 = getLineBreakProperty(c);
			props.push_back(p0);
			total |= p0;
			info.push_back({ c, pos, next, isInterwordSpace, isSpace, p0, WEAK_BREAK_ALLOWED });
			pos = next;
		}
		vector<LineBreakOpportunity> opportunities(props.size(), WEAK_BREAK_ALLOWED);
		for(const auto &it : tableOfLineBreakProcedures)
			if(it.needToScan(total))
				total |= it.scan(props, opportunities);
		
		size_t i = 0;
		for(const auto &opportunity : opportunities)
		{
			info[i].lineBreakOpportunity = opportunity == WEAK_BREAK_ALLOWED ? BREAK_ALLOWED : opportunity;
			++i;
		}
		return info;
	}
	
	
	
	MergedCharactersBlock::MergedCharactersBlock()
		: isInterwordSpace(false), isSpace(false), spaceWeight(0),
			lineBreakOpportunity(WEAK_BREAK_ALLOWED), isParagraphEnd(false), s()
	{
	}
	
	
	
	MergedCharactersBlock MergedCharactersBlock::moveAndClear()
	{
		MergedCharactersBlock r;
		r.isInterwordSpace = this->isInterwordSpace;
		r.isSpace = this->isSpace;
		r.spaceWeight = this->spaceWeight;
		r.lineBreakOpportunity = this->lineBreakOpportunity;
		r.isParagraphEnd = this->isParagraphEnd;
		r.s.swap(this->s);
		this->isInterwordSpace = false;
		this->isSpace = false;
		this->spaceWeight = 0;
		this->lineBreakOpportunity = WEAK_BREAK_ALLOWED;
		this->isParagraphEnd = false;
		return r;
	}
	
	
	
	vector<MergedCharactersBlock> divideIntoBlocks(const string &text)
	{
		vector<CharacterInformation> info = getCharacterInformation(text);
		vector<MergedCharactersBlock> blocks;
		MergedCharactersBlock block;
		for(const auto &c : info)
		{
			if(block.s.length() > 0)
				// Check if c can not merge into the current block.
				if(block.isInterwordSpace || block.isInterwordSpace != c.isInterwordSpace
					|| block.isSpace != c.isSpace || block.lineBreakOpportunity != NO_BREAK_ALLOWED
					|| block.spaceWeight > 0)
					blocks.push_back(block.moveAndClear());
			
			block.spaceWeight = 0;
			if(c.c==U'\U00000020' || c.c==U'\U000000A0')
				block.spaceWeight = 2;
			else if(c.lineBreakProperty == ID)
			{
				// Space weight = 1 where is the point after/before ID && BREAK_ALLOWED.
				block.spaceWeight = c.lineBreakOpportunity == BREAK_ALLOWED ? 1 : 0;
				if(!blocks.empty() && blocks.back().lineBreakOpportunity == BREAK_ALLOWED && blocks.back().spaceWeight == 0)
					blocks.back().spaceWeight = 1;
			}
			block.isInterwordSpace = c.isInterwordSpace;
			block.isSpace = c.isSpace;
			block.lineBreakOpportunity = c.lineBreakOpportunity;
			block.isParagraphEnd = c.lineBreakOpportunity == MANDATORY_BREAK;
			block.s.append(text.begin() + c.pos, text.begin() + c.next);
		}
		blocks.push_back(block.moveAndClear());
		return blocks;
	}
}
