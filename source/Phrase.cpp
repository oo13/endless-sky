/* Phrase.cpp
Copyright (c) 2014 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "Phrase.h"

#include "DataNode.h"
#include "GameData.h"
#include "Gettext.h"
#include "Random.h"

#include <regex>
#include <set>

using namespace std;
using namespace Gettext;

namespace {
	// List of all instances need to translate.
	set<Phrase*> &GetListOfTranslating()
	{
		static set<Phrase*> *listOfTranslating(new set<Phrase*>);
		return *listOfTranslating;
	}
	
	// The Hook of translation.
	function<void()> updateCatalog([](){
		for(auto it : GetListOfTranslating())
			it->ParseAllNodes();
	});
	// Set the hook.
	bool hooked = AddHookUpdating(&updateCatalog);
}



Phrase::~Phrase() noexcept
{
	GetListOfTranslating().erase(this);
}



void Phrase::Load(const DataNode &node)
{
	// Set the name of this phrase, so we know it has been loaded.
	name = node.Size() >= 2 ? node.Token(1) : "Unnamed Phrase";
	
	originalNode.push_back(node);
	ParseNode(node);
	if(IsTranslating())
		GetListOfTranslating().insert(this);
}



void Phrase::ParseAllNodes()
{
	parts.clear();
	for(const auto &node : originalNode)
		ParseNode(node);
}



void Phrase::ParseNode(const DataNode &node)
{
	// Translate all nodes.
	const DataNode tnode = TranslateNode(node);
	
	parts.emplace_back();
	for(const DataNode &child : tnode)
	{
		parts.back().emplace_back();
		Part &part = parts.back().back();
		
		if(child.Token(0) == "word")
		{
			for(const DataNode &grand : child)
				part.words.push_back(grand.Token(0));
		}
		else if(child.Token(0) == "phrase")
		{
			for(const DataNode &grand : child)
			{
				const Phrase *subphrase = GameData::Phrases().Get(grand.Token(0));
				if(subphrase->ReferencesPhrase(this))
					child.PrintTrace("Found recursive phrase reference:");
				else
					part.phrases.push_back(subphrase);
			}
		}
		else if(child.Token(0) == "replace")
		{
			for(const DataNode &grand : child)
			{
				try
				{
					const regex e(grand.Token(0));
					const string fmt = grand.Size() >= 2 ? grand.Token(1) : "";
					regex_constants::match_flag_type flags = regex_constants::format_first_only;
					if(grand.Size() >= 3 && grand.Token(2).find("g") != string::npos)
						flags = regex_constants::match_default;
					auto f = [e, fmt, flags](const string &s) -> string { return regex_replace(s, e, fmt, flags); };
					part.replaceRules.push_back(f);
				}
				catch (regex_error &e)
				{
					grand.PrintTrace("Regex error:");
				}
			}
		}
		else
			child.PrintTrace("Skipping unrecognized attribute:");
		
		// If no words or phrases or replace rules were given, discard this part of the phrase.
		if(part.words.empty() && part.phrases.empty() && part.replaceRules.empty())
			parts.back().pop_back();
	}
}



const string &Phrase::Name() const
{
	return name;
}



string Phrase::Get() const
{
	string result;
	if(parts.empty())
		return result;
	
	for(const Part &part : parts[Random::Int(parts.size())])
	{
		if(!part.phrases.empty())
			result += part.phrases[Random::Int(part.phrases.size())]->Get();
		else if(!part.words.empty())
			result += part.words[Random::Int(part.words.size())];
		else if(!part.replaceRules.empty())
			for(auto f : part.replaceRules)
				result = f(result);
	}
	
	return result;
}



bool Phrase::ReferencesPhrase(const Phrase *phrase) const
{
	if(phrase == this)
		return true;
	
	for(const vector<Part> &alternative : parts)
		for(const Part &part : alternative)
			for(const Phrase *subphrase : part.phrases)
				if(subphrase->ReferencesPhrase(phrase))
					return true;
	
	return false;
}
