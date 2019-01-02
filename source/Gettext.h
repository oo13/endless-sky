/* Gettext.h
Copyright (c) 2019 by Michael Zahniser

Main function for Endless Sky, a space exploration and combat RPG.

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef GETTEXT_H_
#define GETTEXT_H_

#include "DataNode.h"

#include <functional>
#include <string>
#include <vector>

namespace Gettext {
	// These are helper functions and a class for xgettext.
	// The xgettext extracts translatable strings from source files,
	// using function names. If you want to extract words for a translation,
	// certain functions must have a *literal* as a parameter.
	// These translate functions usually have very short names.
	//
	// This command can make a pot file from that source file:
	// xgettext -c++ -kT_:1 -kT_:1,2c -kT:1 -kT:1,2c -knT:1,2 -knT:1,2,3c -kG:1 -kG:1,2c -cTRANSLATORS: filename(s)
	
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
	// The translated text is dynamically changed when updating the catalog.
	// If IsTranslating() is true at constructing except copy, the instance will not translate the text forever.
	class T_ {
	public:
		T_();
		explicit T_(const std::string &msgid);
		T_(const std::string &msgid, const std::string &context);
		T_(const T_ &a);
		// DO NOT/DO translate msg regardless of translating mode.
		enum ForceT { DONT, FORCE };
		T_(const std::string &msg, ForceT forceType);
		~T_();
		const T_ &operator=(const T_ &a);
		
		operator const std::string &() const;
		const std::string &Str() const;
		const std::string &Original() const;
		
		void Clear();
	private:
		std::string context;
		std::string original;
		std::string translated;
		std::function<void()> f;
		
		void AddHook(bool force = false);
		void RemoveHook();
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
	// Same as T_(msg, DONT), but xgettext does not extract this string.
	inline T_ Tx(const std::string &msg)
	{
		return T_(msg, T_::DONT);
	}
	
	
	// This function translates a text.
	std::string T(const std::string &msgid);
	std::string T(const std::string &msgid, const std::string &context);
	std::string nT(const std::string &msgid, const std::string &msgid_plural, unsigned long n);
	std::string nT(const std::string &msgid, const std::string &msgid_plural,
		const std::string &context, unsigned long n);
	
	// Translate a node structure.
	// This function can change the number of child nodes and depth, etc.
	DataNode TranslateNode(const DataNode &node);
	DataNode TranslateNode(const DataNode &node, const std::string &context);
	
	// Make a text form vector<T_>.
	std::string Concat(const std::vector<T_> &a);
	// Same as Concat(a).empty(), but probably faster.
	bool IsEmptyText(const std::vector<T_> &a);
	
	// Update message catalog.
	void UpdateCatalog(const std::vector<std::string> &catalogFiles);
	
	// Stop translating a text.
	void StopTranslating();
	// Restart translating a text.
	void RestartTranslating();
	// Get translating mode.
	bool IsTranslating();
	
	// Add/Remove a hook to notify when updating message catalog.
	// When this function add a hook, it is called immediately unless it's already existed.
	// AddHookUpdating returns true when the hook is already existed.
	// You can call AddHookUpdating when initializing any static variables.
	// This hook is called after updating all instances of class T_.
	bool AddHookUpdating(std::function<void()> *hook);
	void RemoveHookUpdating(std::function<void()> *hook);
};

#endif
