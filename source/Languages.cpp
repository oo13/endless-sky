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

#include <algorithm>
#include <functional>
#include <locale>
#include <map>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;
using namespace Gettext;

namespace {
	class Language {
	public:
		explicit Language(const string &code);
		
		void Load(const DataNode &node);
		
		// Get/Set the name of the language.
		const string &GetName() const;
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
		
		static const int ANY_FONT_SIZE;
		static const vector<string> defaultFullnameFormats;
	};
	
	const int Language::ANY_FONT_SIZE = -1;
	const vector<string> Language::defaultFullnameFormats{"<first> <last>"};
	
	
	
	// System default language code.
	string systemDefaultLanguageCode;
	
	// List of language codes.
	vector<string> languageCodes{"", "en"};
	
	// map of a language. (language code -> an instance of Language)
	map<string, Language> languages;
	
	// Current language code.
	string currentLanguageCode;
	
	// Current fullname format.
	string currentFullnameFormat;
	
	
	
	// Get the system default language code (ISO-639).
	string GetSystemDefaultLanguageCode()
	{
#ifdef _WIN32
		char buf[9];
		GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, buf, sizeof(buf));
		return buf;
#else
		try
		{
			string lang = locale("").name();
			size_t r = lang.find('_');
			if(r == string::npos)
				r = lang.find('-');
			if(r == string::npos)
				r = lang.length();
			lang.resize(r);
			return lang;
		}
		catch (runtime_error &e)
		{
			return "";
		}
#endif
	}
	
	
	
	// Get an instance of language from the map of languages.
	Language &GetLanguage(const string &languageCode)
	{
		string code = languageCode.empty() ? systemDefaultLanguageCode : languageCode;
		auto it = languages.find(code);
		if(it != languages.end())
			return it->second;
		else
		{
			auto it2 = find(languageCodes.begin(), languageCodes.end(), code);
			if(it2 == languageCodes.end())
				languageCodes.push_back(code);
			return languages.emplace(code, code).first->second;
		}
	}
	
	
	
	Language::Language(const string &code)
		: name(code), fontPriority(), fontReference(), catalogFiles(), fullnameFormats()
	{}
	
	
	
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
		SetName(node.Token(1));
		
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
				{
					const string &fullname = grand.Token(0);
					auto it = find(fullnameFormats.begin(), fullnameFormats.end(), fullname);
					if(it == fullnameFormats.end())
						fullnameFormats.push_back(grand.Token(0));
					else
						grand.PrintTrace("Duplicate <fullname> format:");
				}
			else
				child.PrintTrace("Skipping unrecognized attribute:");
		}
	}
	
	
	
	const string &Language::GetName() const
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
		if(fullnameFormats.empty())
			return defaultFullnameFormats;
		else
			return fullnameFormats;
	}
}



void Languages::Init(const std::vector<std::string> &sources)
{
	systemDefaultLanguageCode = GetSystemDefaultLanguageCode();
	
	for(const string &source : sources)
	{
		const vector<string> langDirs = Files::ListDirectories(source + "locales/");
		for(const auto &langDir : langDirs)
		{
			string langCode(langDir);
			langCode.pop_back();
			langCode = Files::Name(langCode);
			auto &language = GetLanguage(langCode);
			for(const auto &filename : Files::List(langDir))
				if(Files::Extension(filename) == ".po")
					language.AddCatalogFile(filename);
		}
	}
	
	SetLanguageCode("");
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
	GetLanguage(node.Token(1)).Load(node);
}



string Languages::GetLanguageName()
{
	if(currentLanguageCode.empty())
		return T("system default");
	else
		return GetLanguage(currentLanguageCode).GetName();
}



const string &Languages::GetLanguageCode()
{
	return currentLanguageCode;
}



void Languages::SetLanguageCode(const std::string &languageCode)
{
	currentLanguageCode = languageCode;
	auto &lang = GetLanguage(currentLanguageCode);
	auto prioFunc = [&](int s) -> const vector<string>& { return lang.FontPriority(s); };
	auto refFunc = [&](int s) -> const string& { return lang.FontReference(s); };
	FontSet::SetFontPriority(prioFunc, refFunc);
	Gettext::UpdateCatalog(lang.CatalogFiles());
}



const vector<string> &Languages::GetAllLanguageCodes()
{
	return languageCodes;
}



string Languages::GetFullname(const string &first, const string &last)
{
	return Format::Replace(currentFullnameFormat, { {"<first>", first}, {"<last>", last} });
}



string Languages::GetFullnameFormat()
{
	return currentFullnameFormat;
}



void Languages::SetFullnameFormat(const std::string &fullnameFormat)
{
	currentFullnameFormat = fullnameFormat;
}



const vector<std::string> &Languages::GetAllFullnameFormats()
{
	return GetLanguage(currentLanguageCode).GetFullnameFormats();
}
