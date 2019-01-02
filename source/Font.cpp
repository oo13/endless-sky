/* Font.cpp
Copyright (c) 2014 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "Font.h"

#include "AtlasGlyphs.h"
#include "DataNode.h"
#include "GameData.h"
#include "Gettext.h"
#include "Files.h"
#include "FreeTypeGlyphs.h"
#include "Point.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <utility>

using namespace std;
using namespace Gettext;

namespace {
	bool showUnderlines = false;
	
	// Determines the number of bytes used by the unicode code point in utf8.
	int CodePointBytes(const char *str)
	{
		// end - 00000000
		if(!str || !*str)
			return 0;
		
		// 1 byte - 0xxxxxxx
		if((*str & 0x80) == 0)
			return 1;
		
		// invalid - 10?????? or 11?????? invalid
		if((*str & 0x40) == 0 || (*(str + 1) & 0xc0) != 0x80)
			return -1;
		
		// 2 bytes - 110xxxxx 10xxxxxx
		if((*str & 0x20) == 0)
			return 2;
		
		// invalid - 111????? 10?????? invalid
		if((*(str + 2) & 0xc0) != 0x80)
			return -1;
		
		// 3 bytes - 1110xxxx 10xxxxxx 10xxxxxx
		if((*str & 0x10) == 0)
			return 3;
		
		// invalid - 1111???? 10?????? 10?????? invalid
		if((*(str + 3) & 0xc0) != 0x80)
			return -1;
		
		// 4 bytes - 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
		if((*str & 0x8) == 0)
			return 4;
		
		// not unicode - 11111??? 10?????? 10?????? 10??????
		return -1;
	}
	
	// The ellipsis character.
	const T_ ellipsisCharacter = T_("...");
	const string nonEllipsis = "";
}



Font::Font()
	: referenceFont(nullptr)
{
	widthCache.SetUpdateInterval(3600);
	drawCache.SetUpdateInterval(3600);
}



bool Font::Load(const DataNode &node, int size)
{
	size_t oldCount = sources.size();
	
	if(node.Token(0) != "font")
	{
		node.PrintTrace("Not a font node:");
		return false;
	}
	
	// Get glyph source.
	for(const DataNode &child : node)
	{
		const string &key = child.Token(0);
		if(key != "atlas" && key != "freetype")
			continue;
		
		if(child.Size() <= 1 || child.Token(1).empty())
		{
			child.PrintTrace("Path is missing:");
			return false;
		}
		
		vector<string> paths;
		paths.emplace_back(child.Token(1));
		paths.emplace_back(Files::Resources() + child.Token(1));
		for(const string &source : GameData::Sources())
			paths.emplace_back(source + child.Token(1));
		while(!paths.empty() && !Files::Exists(paths.back()))
			paths.pop_back();
		if(paths.empty())
		{
			child.PrintTrace("Path not found:");
			return false;
		}
		
		size_t count = sources.size();
		if(key == "atlas")
		{
			auto glyphs = make_shared<AtlasGlyphs>();
			if(glyphs->Load(paths.back()))
				sources.emplace_back(glyphs);
		}
		else if(key == "freetype")
		{
			auto glyphs = make_shared<FreeTypeGlyphs>();
			if(glyphs->Load(paths.back(), size))
				sources.emplace_back(glyphs);
		}
		if(count == sources.size())
		{
			child.PrintTrace("Load failed:");
			return false;
		}
		
		IGlyphs *newFont = sources.back().get();
		if(count == 0)
			referenceFont = newFont;
		fontName[Files::Name(paths.back())] = count;
		preferedFont.push_back(newFont);
	}
	if(sources.size() == oldCount)
	{
		node.PrintTrace("Must have at least one glyph source (atlas or freetype):");
		return false;
	}
	
	// Unsupported children are ignored instead of producing an error.
	return true;
}



void Font::SetUpShader()
{
	for(auto &source : sources)
		source->SetUpShader();
}



void Font::Draw(const string &str, const Point &point, const Color &color) const
{
	DrawAliased(str, round(point.X()), round(point.Y()), color);
}


void Font::DrawAliased(const string &str, double x, double y, const Color &color) const
{
	if(preferedFont.empty() || str.empty())
		return;
	
	y += referenceFont->Baseline();
	if(preferedFont.size() == 1)
	{
		const string buf = ReplaceCharacters(str);
		preferedFont[0]->Draw(buf, x, y, color);
	}
	else
	{
		const auto cached = drawCache.Use(str);
		if(cached.second)
		{
			for(const auto &section : *cached.first)
			{
				preferedFont[section.priorityNumber]->Draw(section.text, x, y, color);
				x += section.width;
			}
		}
		else
		{
			vector<DrawnData> cacheData;
			const string buf = ReplaceCharacters(str);
			if(preferedFont[0]->FindUnsupported(buf) == buf.length())
			{
				preferedFont[0]->Draw(buf, x, y, color);
				cacheData.emplace_back(buf, 0, 0.0);
			}
			else
			{
				size_t pos = 0;
				vector<pair<size_t,size_t>> sections = Prepare(buf);
				for(const auto &section : sections)
				{
					string tmp(buf, pos, section.second - pos);
					preferedFont[section.first]->Draw(tmp, x, y, color);
					const double w = preferedFont[section.first]->Width(tmp);
					x += w;
					pos = section.second;
					cacheData.emplace_back(tmp, section.first, w);
				}
			}
			drawCache.Set(str, move(cacheData));
		}
	}
}



int Font::Width(const string &str, double *highPrecisionWidth) const
{
	if(preferedFont.empty())
		return 0;
	
	const auto cached = widthCache.Use(str);
	if(cached.second)
	{
		if(highPrecisionWidth)
			*highPrecisionWidth = *cached.first;
		return ceil(*cached.first);
	}
	
	const string buf = ReplaceCharacters(str);
	double w = 0;
	if(preferedFont.size() == 1 || preferedFont[0]->FindUnsupported(buf) == buf.length())
		w = preferedFont[0]->Width(buf);
	else
	{
		size_t pos = 0;
		vector<pair<size_t,size_t>> sections = Prepare(buf);
		for(const auto &section : sections)
		{
			string tmp(buf, pos, section.second - pos);
			w += preferedFont[section.first]->Width(tmp);
			pos = section.second;
		}
	}
	widthCache.Set(str, double(w));
	if(highPrecisionWidth)
		*highPrecisionWidth = w;
	return ceil(w);
}



string Font::Truncate(const string &str, int width, bool ellipsis) const
{
	int prevWidth = Width(str);
	if(prevWidth <= width)
		return str;
	
	if(ellipsis)
		width -= Width(ellipsisCharacter);
	
	// Find the last index that fits the width. [good,bad[
	size_t len = str.length();
	size_t prev = len;
	size_t good = 0;
	size_t bad = len;
	size_t tries = len + 1;
	while(NextCodePoint(str, good) < bad && --tries)
	{
		// Interpolate next index from the width of the previous index.
		size_t next = CodePointStart(str, (prev * width) / prevWidth);
		if(next <= good)
			next = NextCodePoint(str, good);
		else if(next >= bad)
			next = CodePointStart(str, bad - 1);
		
		int nextWidth = Width(str.substr(0, next));
		if(nextWidth <= width)
			good = next;
		else
			bad = next;
		prev = next;
		prevWidth = nextWidth;
	}
	return str.substr(0, good) + (ellipsis ? ellipsisCharacter : nonEllipsis);
}



string Font::TruncateFront(const string &str, int width, bool ellipsis) const
{
	int prevWidth = Width(str);
	if(prevWidth <= width)
		return str;
	
	if(ellipsis)
		width -= Width(ellipsisCharacter);
	
	// Find the first index that fits the width. ]bad,good]
	size_t len = str.length();
	size_t prev = 0;
	size_t bad = 0;
	size_t good = len;
	int tries = len + 1;
	while(NextCodePoint(str, bad) < good && --tries)
	{
		// Interpolate next index from the width of the previous index.
		size_t next = CodePointStart(str, len - ((len - prev) * width) / prevWidth);
		if(next <= bad)
			next = NextCodePoint(str, bad);
		else if(next >= good)
			next = CodePointStart(str, good - 1);
		
		int nextWidth = Width(str.substr(next));
		if(nextWidth <= width)
			good = next;
		else
			bad = next;
		prev = next;
		prevWidth = nextWidth;
	}
	return (ellipsis ? ellipsisCharacter : nonEllipsis) + str.substr(good);
}



string Font::TruncateMiddle(const string &str, int width, bool ellipsis) const
{
	if(Width(str) <= width)
		return str;
	
	if(ellipsis)
		width -= Width(ellipsisCharacter);
	
	string right = TruncateFront(str, width / 2, false);
	width -= Width(right);
	string left = Truncate(str, width, false);
	return left + (ellipsis ? ellipsisCharacter : nonEllipsis) + right;
}



int Font::Height() const
{
	if(!referenceFont)
		return 0;
	
	return ceil(referenceFont->LineHeight());
}



int Font::Space() const
{
	if(!referenceFont)
		return 0;
	
	return ceil(referenceFont->Space());
}



void Font::SetFontPriority(const vector<string> &priorityList, const string &referenece)
{
	if(sources.empty())
		return;
	
	vector<size_t> copied;
	size_t dest = 0;
	for(const auto &name : priorityList)
	{
		const auto it = fontName.find(name);
		if(it != fontName.end())
		{
			preferedFont[dest] = sources[it->second].get();
			++dest;
			copied.push_back(it->second);
		}
		else
			Files::LogError("Unknown font name: " + name);
	}
	sort(copied.begin(), copied.end());
	size_t skipIndex = 0;
	for(size_t src = 0; src < sources.size(); ++src)
	{
		if(skipIndex < copied.size() && copied[skipIndex] == src)
			skipIndex++;
		else
		{
			preferedFont[dest] = sources[src].get();
			++dest;
		}
	}
	
	referenceFont = preferedFont[0];
	if(!referenece.empty())
	{
		const auto ref = fontName.find(referenece);
		if(ref == fontName.end())
			Files::LogError("Unknown font name: " + referenece);
		else
			referenceFont = sources[ref->second].get();
	}
	
	ClearCache();
}



void Font::ShowUnderlines(bool show)
{
	showUnderlines = show;
}



bool Font::ShowUnderlines()
{
	return showUnderlines;
}



// Replaces straight quotation marks with curly ones.
string Font::ReplaceCharacters(const string &str)
{
	string buf;
	buf.reserve(str.length());
	bool isAfterWhitespace = true;
	for(size_t pos = 0; pos < str.length(); ++pos)
	{
		// U+2018 LEFT_SINGLE_QUOTATION_MARK
		// U+2019 RIGHT_SINGLE_QUOTATION_MARK
		// U+201C LEFT_DOUBLE_QUOTATION_MARK
		// U+201D RIGHT_DOUBLE_QUOTATION_MARK
		if(str[pos] == '\'')
			buf.append(isAfterWhitespace ? "\xE2\x80\x98" : "\xE2\x80\x99");
		else if(str[pos] == '"')
			buf.append(isAfterWhitespace ? "\xE2\x80\x9C" : "\xE2\x80\x9D");
		else
			buf.append(1, str[pos]);
		isAfterWhitespace = (str[pos] == ' ');
	}
	return buf;
}



// Convert between utf8 and utf32.
// Invalid code points are converted to 0xFFFFFFFF in utf32 and 0xFF in utf8.
u32string Font::Convert(const string &str)
{
	u32string buf;
	for(size_t pos = 0; pos < str.length(); pos = NextCodePoint(str, pos))
		buf.append(1, DecodeCodePoint(str, pos));
	return buf;
}



string Font::Convert(const u32string &str)
{
	string buf;
	for(char32_t c : str)
	{
		if(c < 0x80)
			buf.append(1, c);
		else if(c < 0x800)
		{
			buf.append(1, 0xC0 | (c >> 6));
			buf.append(1, 0x80 | (c & 0x3f));
		}
		else if(c < 0x10000)
		{
			buf.append(1, 0xE0 | (c >> 12));
			buf.append(1, 0x80 | ((c >> 6) & 0x3f));
			buf.append(1, 0x80 | (c & 0x3f));
		}
		else if(c < 0x110000)
		{
			buf.append(1, 0xF0 | (c >> 18));
			buf.append(1, 0x80 | ((c >> 12) & 0x3f));
			buf.append(1, 0x80 | ((c >> 6) & 0x3f));
			buf.append(1, 0x80 | (c & 0x3f));
		}
		else
			buf.append(1, 0xFF); // not unicode
	}
	return buf;
}



// Skip to the next unicode code point after pos in utf8.
// Return the string length when there are no more code points.
size_t Font::NextCodePoint(const string &str, size_t pos)
{
	if(pos >= str.length())
		return str.length();
	
	for(++pos; pos < str.length(); ++pos)
		if((str[pos] & 0x80) == 0 || (str[pos] & 0xc0) == 0xc0)
			break;
	return pos;
}



// Returns the start of the unicode code point at pos in utf8.
size_t Font::CodePointStart(const string &str, size_t pos)
{
	// 0xxxxxxx and 11?????? start a code point
	while(pos > 0 && (str[pos] & 0x80) != 0x00 && (str[pos] & 0xc0) != 0xc0)
		--pos;
	return pos;
}



// Decodes a unicode code point in utf8.
// Invalid codepoints are converted to 0xFFFFFFFF.
char32_t Font::DecodeCodePoint(const string &str, size_t pos)
{
	if(pos >= str.length())
		return 0;
	
	// invalid (-1) or end (0)
	int bytes = CodePointBytes(str.c_str() + pos);
	if(bytes < 1)
		return bytes;
	
	// 1 byte
	if(bytes == 1)
		return (str[pos] & 0x7f);
	
	// 2-4 bytes
	char32_t c = (str[pos] & ((1 << (7 - bytes)) - 1));
	for(int i = 1; i < bytes; ++i)
		c = (c << 6) + (str[pos + i] & 0x3f);
	return c;
}



// Prepare a string for processing by multiple sources, producing source-end pairs.
vector<pair<size_t,size_t>> Font::Prepare(const std::string &str) const
{
	// XXX This is an experimental attempt at using different glyph sources to get full unicode coverage:
	//  - assumes code points supported by multiple sources are treated the same way
	//  - prefers the first source that supports [start,end[
	//  - uses source 0 for unsupported data
	vector<pair<size_t,size_t>> sections;
	for(size_t start = 0; start < str.length(); )
	{
		bool isUnsupported = true;
		
		// Supported data.
		for(size_t i = 0; i < preferedFont.size(); ++i)
		{
			size_t end = preferedFont[i]->FindUnsupported(str, start);
			if(end == start)
				continue;
			const size_t next = i == 0 ? end : NextCodePoint(str, start);
			if(!sections.empty() && sections.back().first == i)
				sections.back().second = next;
			else
				sections.emplace_back(i, next);
			isUnsupported = false;
			start = next;
			break;
		}
		
		// Unsupported data.
		if(isUnsupported)
		{
			size_t end = NextCodePoint(str, start);
			if(!sections.empty() && sections.back().first == 0)
				sections.back().second = end;
			else
				sections.emplace_back(0, end);
			start = end;
		}
	}
	return sections;
}



Font::IGlyphs::~IGlyphs()
{
}



void Font::ClearCache() const
{
	widthCache.Clear();
	drawCache.Clear();
	for(auto &source : sources)
		source->ClearCache();
}
