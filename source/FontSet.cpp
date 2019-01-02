/* FontSet.cpp
Copyright (c) 2014 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "FontSet.h"

#include "DataNode.h"
#include "Font.h"

#include <cmath>
#include <utility>
#include <map>

using namespace std;

namespace {
	map<int, Font> fonts;
}



void FontSet::Load(const DataNode &node)
{
	if(node.Token(0) != "font")
	{
		node.PrintTrace("Not a font node:");
		return;
	}
	if(node.Size() != 2)
	{
		node.PrintTrace("Must have one font size:");
		return;
		
	}
	int size = round(node.Value(1));
	if(size <= 0)
	{
		node.PrintTrace("Invalid font size:");
	}
	
	fonts[size].Load(node, size);
}



void FontSet::SetUpShaders()
{
	for(auto &it : fonts)
		it.second.SetUpShader();
}



const Font &FontSet::Get(int size)
{
	return fonts[size];
}



void FontSet::SetFontPriority(std::function<const std::vector<std::string>&(int)> priorityFunc,
	std::function<const std::string&(int)> referenceFunc)
{
	for(auto &it : fonts)
		it.second.SetFontPriority(priorityFunc(it.first), referenceFunc(it.first));
}
