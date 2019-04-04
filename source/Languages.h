/* Languages.h
Copyright (c) 2019 by Michael Zahniser

Main function for Endless Sky, a space exploration and combat RPG.

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef LANGUAGES_H_
#define LANGUAGES_H_

#include <string>
#include <vector>

class DataNode;

// The class managing all language infomations.
class Languages {
public:
	// Initialize.
	// sources are the list of "source" folders.
	static void Init(const std::vector<std::string> &sources);
	
	
	// Load a language node.
	static void Load(const DataNode &node);
	
	
	// Get current language name.
	static std::string GetLanguageName();
	
	// Get current language code (ISO-639).
	// Empty means the system default language.
	static const std::string &GetLanguageCode();
	
	// Set new language code.
	static void SetLanguageCode(const std::string &languageCode);
	
	// Get known language codes.
	static const std::vector<std::string> &GetAllLanguageCodes();
	
	
	// Get <fullname>.
	static std::string GetFullname(const std::string &first, const std::string &last);
	
	// Get current <fullname> format.
	static std::string GetFullnameFormat();
	
	// Set new <fullname> format.
	// Empty means "<first> <last>".
	static void SetFullnameFormat(const std::string &fullnameFormat);
	
	// Get known fullname formats.
	static const std::vector<std::string> &GetAllFullnameFormats();
};

#endif
