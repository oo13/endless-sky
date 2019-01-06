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
#include <utility>
#include <vector>

class DataNode;

// The class managing all language infomations.
class Languages {
public:
	static void Load(const DataNode &node);
	
	// Get the name of the selected language.
	static std::string PreferenceName();
	// Get the selected language code.
	static const std::string &LanguageCode();
	
	// Set all catalog files under the specified directories.
	static void SetCatalogFiles(const std::vector<std::string> &sources);
	
	// Select a language.
	// Select the system default language if languageCode is systemDefaultLanguageCode.
	static void SelectLanguage(const std::string &languageCode);
	
	// Set a initial language from preferences.
	static void SetInitialPreferenceLanguage(const std::string &languageCode);
	
	// Toggle the language.
	static void ToggleLanguage();
};

#endif
