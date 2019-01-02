/* Locale.h
Copyright (c) 2018 by Michael Zahniser

Main function for Endless Sky, a space exploration and combat RPG.

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef LOCALEINFO_H_
#define LOCALEINFO_H_

#include <functional>
#include <initializer_list>
#include <locale>
#include <string>
#include <vector>

class DataNode;

// The class gathering all locale infomations.
// This class does not support multi-language, because the gettext affects globally.
class LocaleInfo {
public:
	// Set the Locale globally.
	// These functions set only LC_MESSAGES and LC_CTYPE, bacause the gettext
	// uses LC_MESSAGES for the detemination of the language, and LC_CTYPE for
	// the determination character encoding.
	// Local locale should not be set LC_ALL, bacause something like DataWriter
	// that use stream I/O assume LC_NUMERIC is C locale.
	//
	// Use the system locale.
	static void SetLocale();
	// Use loc as the locale.
	static void SetLocale(const std::locale &loc);
	
	// Bind all textdomain.
	static void Init(const std::vector<std::string> &sources);
	
	// Get the translation text.
	// These static functions use the core textdomain.
	static std::string TranslateCore(const std::string &msgid);
	static std::string TranslateCore(const std::string &msgid, const std::string &context);
	static std::string TranslateCore(const std::string &msgid, const std::string &msgid_plural, unsigned long n);
	static std::string TranslateCore(const std::string &msgid, const std::string &msgid_plural,
		const std::string &context, unsigned long n);
	// Translate a text which is loaded from data files.
	// These static functions use a textdomain in which a translated text is
	// found for the first time. It is in registration order to search a
	// translated text.
	static std::string TranslateData(const std::string &msgid);
	static std::string TranslateData(const std::string &msgid, const std::string &context);
	static std::string TranslateData(const std::string &msgid, const std::string &msgid_plural, unsigned long n);
	static std::string TranslateData(const std::string &msgid, const std::string &msgid_plural,
		const std::string &context, unsigned long n);
	
	// Translate a node structure.
	// This function can change the number of child nodes and depth, etc.
	static DataNode TranslateNode(const DataNode &node);
	static DataNode TranslateNode(const DataNode &node, const std::string &context);
	
	// Stop translating a data text.
	static void StopTranslatingData();
	// Restart translating a data text.
	static void RestartTranslatingData();
	
	// Add/Remove a hook to notify that the core textdomain is updated.
	// When this function add a hook, it is called immediately unless it's already existed.
	// AddHookUpdatingCore returns true when the hook is already existed.
	// AddHookUpdatingCore can be called for a initialization of any static variables.
	static bool AddHookUpdatingCore(std::function<void()> *hook);
	static void RemoveHookUpdatingCore(std::function<void()> *hook);
	
	
	// Get the punctation of number.
	static char GetDecimalPoint();
	static char GetThousandsSep();
};



namespace Gettext {
	// These are helper functions and a class for xgettext.
	// The xgettext extracts translatable strings from source files,
	// using function names. If you want to extract words for a translation,
	// certain functions must have a *literal* as a parameter.
	// These translate functions usually have very short names.
	//
	// This command can make a pot file from that source file:
	// xgettext -c++ -kT_:1 -kT_:1,2c -kT:1 -kT:1,2c -knT:1,2 -knT:1,2,3c -kG:1 -kG:1,2c -cTRANSLATORS: filename(s)
	//
	// Just a mark for xgettext.
	constexpr const char* G(const char* p)
	{
		return p;
	}
	constexpr const char* G(const char *p, const char*)
	{
		return p;
	}
	
	// This class holds a translated text.
	class T_ {
	public:
		explicit T_(const std::string &msgid);
		T_(const std::string &msgid, const std::string &context);
		T_(const T_ &a);
		~T_();
		const T_ &operator=(const T_ &a);
		
		operator const std::string &() const;
		const std::string &Str() const;
		const std::string &Original() const;
	private:
		std::string context;
		std::string original;
		std::string translated;
		std::function<void()> f;
	};
	
	inline T_::operator const std::string &() const
	{
		return translated;
	}
	inline const std::string &T_::Str() const
	{
		return translated;
	}
	inline const std::string &T_::Original() const
	{
		return original;
	}
	
	// This function translates a text with the core textdomain.
	inline std::string T(const std::string &msgid)
	{
		return LocaleInfo::TranslateCore(msgid);
	}
	inline std::string T(const std::string &msgid, const std::string &context)
	{
		return LocaleInfo::TranslateCore(msgid, context);
	}
	// This function is a plural form version of T().
	inline std::string nT(const std::string &msgid, const std::string &msgid_plural, unsigned long n)
	{
		return LocaleInfo::TranslateCore(msgid, msgid_plural, n);
	}
	inline std::string nT(const std::string &msgid, const std::string &msgid_plural,
		const std::string &context, unsigned long n)
	{
		return LocaleInfo::TranslateCore(msgid, msgid_plural, context, n);
	}
};
#endif
