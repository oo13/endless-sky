/* Languages.cpp
Copyright (c) 2019 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "Languages.h"

#include "DataNode.h"
#include "Format.h"
#include "Gettext.h"
#include "Files.h"
#include "FontSet.h"
#include "WrappedText.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <locale>
#include <map>
#include <stdexcept>

using namespace std;
using namespace Gettext;

namespace {
	class Language {
	public:
		void Load(const DataNode &node);
		
		// Get/Set the name of the language.
		const string &Name() const;
		void SetName(const string &langName);
		
		// Get the rendering priority of fonts.
		const vector<string> &FontPriority(int size) const;
		// Get the reference for layouting a text.
		const string &FontReference(int size) const;
		
		// Get/Add the message catalog files.
		const vector<string> &CatalogFiles() const;
		void AddCatalogFile(const string &catalogFile);
		
		// Get the fullname formats.
		const vector<string> &GetFullnameFormats() const;
		
		
	private:
		string name;
		mutable map<int, vector<string>> fontPriority;
		mutable map<int, string> fontReference;
		vector<string> catalogFiles;
		vector<string> fullnameFormats;
	};
	
	
	
	const int ANY_FONT_SIZE = -1;
	
	// Specical language code for system default.
	const string systemDefaultLanguageCode(G("system default"));
	const string englishCode("en");
	map<string, Language> languages{ { englishCode, Language() } };
	map<string, Language>::iterator selected = languages.begin();
	
	vector<string> toggleList{ systemDefaultLanguageCode, englishCode };
	size_t currentIndex = 0;
	
	// Fullname format.
	const string defaultFullnameFormat("<first> <last>");
	string currentFullnameFormat(defaultFullnameFormat);
	size_t currentFullnameIndex = 0;
	
	
	
	void Language::Load(const DataNode &node)
	{
		if(node.Token(0) != "language")
		{
			node.PrintTrace("Not a language node:");
			return;
		}
		if(node.Size() != 2)
		{
			node.PrintTrace("Must have two language size:");
			return;
		}
		name = node.Token(1);
		
		for(const DataNode &child : node)
		{
			if(child.Token(0) == "name" && child.Size() >= 2)
				name = child.Token(1);
			else if(child.Token(0) == "font priority")
			{
				int size = ANY_FONT_SIZE;
				if(child.Size() >= 2)
				{
					size = child.Value(1);
					if(size <= 0)
					{
						child.PrintTrace("Invalid font size:");
						continue;
					}
				}
				for(const DataNode &grand : child)
					fontPriority[size].push_back(grand.Token(0));
			}
			else if(child.Token(0) == "font reference")
			{
				int size = ANY_FONT_SIZE;
				if(child.Size() >= 2)
				{
					size = child.Value(1);
					if(size <= 0)
					{
						child.PrintTrace("Invalid font size:");
						continue;
					}
				}
				for(const DataNode &grand : child)
					fontReference[size] = grand.Token(0);
			}
			else if(child.Token(0) == "fullname")
				for(const DataNode &grand : child)
					fullnameFormats.push_back(grand.Token(0));
			else
				child.PrintTrace("Skipping unrecognized attribute:");
		}
	}
	
	
	
	const string &Language::Name() const
	{
		return name;
	}
	
	
	
	void Language::SetName(const string &langName)
	{
		name = langName;
	}
	
	
	
	const vector<string> &Language::FontPriority(int size) const
	{
		const auto it = fontPriority.find(size);
		if(it != fontPriority.end())
			return it->second;
		
		const auto it2 = fontPriority.find(ANY_FONT_SIZE);
		if(it2 != fontPriority.end())
			return it2->second;
		else
			return fontPriority[ANY_FONT_SIZE];
	}
	
	
	
	const string &Language::FontReference(int size) const
	{
		const auto it = fontReference.find(size);
		if(it != fontReference.end())
			return it->second;
		
		const auto it2 = fontReference.find(ANY_FONT_SIZE);
		if(it2 != fontReference.end())
			return it2->second;
		else
		{
			const auto &prio = FontPriority(size);
			if(prio.empty())
				return fontReference[ANY_FONT_SIZE];
			else
			{
				fontReference[size] = prio.front();
				return prio.front();
			}
		}
	}
	
	
	
	const vector<string> &Language::CatalogFiles() const
	{
		return catalogFiles;
	}
	
	
	
	void Language::AddCatalogFile(const string &catalogFile)
	{
		catalogFiles.push_back(catalogFile);
	}
	
	
	
	const vector<string> &Language::GetFullnameFormats() const
	{
		return fullnameFormats;
	}
	
	
	
	// Get the system default language code (ISO-639-1).
	string GetSystemDefaultLanguage()
	{
		try
		{
			string lang = locale("").name();
			size_t r = lang.find('_');
			if(r == string::npos)
				r = lang.find('-');
			if(r == string::npos)
				r = lang.length();
			// A code in ISO-639-1 has 2 letters.
			if(r != 2)
				return "";
			lang.resize(r);
			return lang;
		}
		catch (runtime_error &e)
		{
			return "";
		}
	}
	
	
	
	Language &getLanguage(const string &code)
	{
		const size_t oldSize = languages.size();
		Language &lang = languages[code];
		if(oldSize < languages.size())
		{
			toggleList.push_back(code);
			lang.SetName(code);
		}
		return lang;
	}
	
	
	
	void SetFullnameFormatFromIndex(size_t n)
	{
		const vector<string> formats = selected->second.GetFullnameFormats();
		if(formats.empty())
			currentFullnameFormat = defaultFullnameFormat;
		else
		{
			currentFullnameIndex = n;
			if(n >= formats.size())
				currentFullnameIndex = 0;
			currentFullnameFormat = formats[currentFullnameIndex];
		}
	}
}



void Languages::Load(const DataNode &node)
{
	if(node.Token(0) != "language")
	{
		node.PrintTrace("Not a language node:");
		return;
	}
	if(node.Size() != 2)
	{
		node.PrintTrace("Must have two language size:");
		return;
		
	}
	getLanguage(node.Token(1)).Load(node);
}



string Languages::PreferenceName()
{
	if(currentIndex == 0)
		return T(systemDefaultLanguageCode);
	else
		return selected->second.Name();
}



const string &Languages::LanguageCode()
{
	return toggleList[currentIndex];
}



void Languages::SetCatalogFiles(const vector<string> &sources)
{
	for(const string &source : sources)
	{
		const vector<string> langDirs = Files::ListDirectories(source + "locales/");
		for(const auto &langDir : langDirs)
		{
			string lang(langDir);
			lang.pop_back();
			lang = Files::Name(lang);
			for(const auto &filename : Files::List(langDir))
				if(Files::Extension(filename) == ".po")
					getLanguage(lang).AddCatalogFile(filename);
		}
	}
}



void Languages::SelectLanguage(const string &languageCode)
{
	string code = languageCode;
	if(languageCode == systemDefaultLanguageCode)
		code = GetSystemDefaultLanguage();
	selected = languages.find(code);
	if(selected == languages.end())
		selected = languages.emplace(code, Language()).first;
	
	auto prioFunc = [&](int s) -> const vector<string>& { return selected->second.FontPriority(s); };
	auto refFunc = [&](int s) -> const string& { return selected->second.FontReference(s); };
	FontSet::SetFontPriority(prioFunc, refFunc);
	Gettext::UpdateCatalog(selected->second.CatalogFiles());
}



void Languages::SetInitialPreferenceLanguage(const string &languageCode)
{
	if(languageCode == systemDefaultLanguageCode)
		currentIndex = 0;
	else
	{
		auto it = find(toggleList.begin(), toggleList.end(), languageCode);
		if(it == toggleList.end())
		{
			getLanguage(languageCode);
			it = toggleList.end();
			--it;
		}
		currentIndex = distance(toggleList.begin(), it);
	}
	SelectLanguage(toggleList[currentIndex]);
}



void Languages::ToggleLanguage()
{
	++currentIndex;
	if(currentIndex >= toggleList.size())
		currentIndex = 0;
	SelectLanguage(toggleList[currentIndex]);
	SetFullnameFormatFromIndex(0);
}



string Languages::Fullname(const string &first, const string &last)
{
	map<string, string> subs;
	subs["<first>"] = first;
	subs["<last>"] = last;
	return Format::Replace(currentFullnameFormat, subs);
}



string Languages::FullnameFormat()
{
	return currentFullnameFormat;
}



void Languages::SetFullnameFormat(const std::string &fullnameFormat)
{
	const vector<string> formats = selected->second.GetFullnameFormats();
	auto it = find(formats.begin(), formats.end(), fullnameFormat);
	if(it == formats.end())
		currentFullnameIndex = 0;
	else
		currentFullnameIndex = distance(formats.begin(), it);
	SetFullnameFormatFromIndex(currentFullnameIndex);
}



void Languages::ToggleFullnameFormat()
{
	++currentFullnameIndex;
	SetFullnameFormatFromIndex(currentFullnameIndex);
}
