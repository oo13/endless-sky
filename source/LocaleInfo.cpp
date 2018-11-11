/* Locale.cpp
Copyright (c) 2018 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "LocaleInfo.h"

#include <algorithm>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "DataFile.h"
#include "DataNode.h"
#include "DataWriter.h"
#include "Files.h"
#include "Format.h"

using namespace std;

namespace {
	// Context separator. It's compatible with GNU gettext tools.
	const char contextSeparator = '\x04';
	
	// The core textdomain name.
	const string coreTextdomainName("endless-sky-core");
	
	// already bound the core textdomain.
	bool boundCoreTextdomain = false;
	
	// Bound data textdomain in priority order.
	vector<string> boundDataTextdomains;
	
	// The locale.
	std::locale localLocale("C");
	// The language name.
	std::string langName("C");
	
	// Stop translating a data text.
	bool stopTranslatingData = false;
	
	// The punctation of number.
	char decimalPoint = '.';
	char thousandsSep = ',';
	
	
	
	// Replace the character encoding name to UTF-8.
	string ReplaceEncodingToUTF8(const locale& loc)
	{
		const string localeName(loc.name());
		const string typePrefix("LC_TYPE=");
		size_t n = localeName.find(typePrefix);
		if(n == string::npos)
			n = 0;
		else
			n += typePrefix.length();
		size_t m = localeName.find_first_of('.', n);
		if(m != string::npos)
			return string(localeName.begin() + n, localeName.begin() + m) + ".UTF-8";
		return "C.UTF-8";
	}
	
	
	
	// Get the language name of LC_MESSAGES.
	string GetLanguageOfMessages(const locale& loc)
	{
		const string localeName(loc.name());
		const string messagesPrefix("LC_MESSAGES=");
		size_t n = localeName.find(messagesPrefix);
		if(n == string::npos)
			n = 0;
		else
			n += messagesPrefix.length();
		size_t m = localeName.find_first_of('_', n);
		if(m != string::npos)
			return string(localeName.begin() + n, localeName.begin() + m);
		
		return "C";
	}
	
	// Return the set of hook functions.
	// This is ensure that the return value has been initialized when it uses
	// for a initialization of any static variables.
	set<std::function<void()>*> &GetCoreHooks()
	{
		// Thread safe in C++11.
		static set<std::function<void()>*> setOfUpdatingCoresHook;
		return setOfUpdatingCoresHook;
	}
	
	// Bind all message catalogs in localizedBaseDir with textdomain.
	// localizedBaseDir must have '/' at the end.
	void BindTextdomain(const std::string &localizedBaseDir)
	{
		// find all textdomains.
		string dir(localizedBaseDir + langName + "/LC_MESSAGES/");
		bool updateCoreTextdomain = false;
		vector<string> list(Files::List(dir));
		sort(list.begin(), list.end());
		for(const auto &path : list)
			if(Files::Extension(path) == ".mo")
			{
				string textdomainName(Files::Name(path));
				textdomainName.erase(textdomainName.length()-3, 3);
				bool needToBind = false;
				if(textdomainName == coreTextdomainName)
				{
					if(!boundCoreTextdomain)
					{
						needToBind = true;
						boundCoreTextdomain = true;
						updateCoreTextdomain = true;
					}
				}
				else if(find(boundDataTextdomains.begin(), boundDataTextdomains.end(), textdomainName)
					== boundDataTextdomains.end())
				{
					needToBind = true;
					boundDataTextdomains.push_back(textdomainName);
				}
				if(needToBind)
					bindtextdomain(textdomainName.c_str(), localizedBaseDir.c_str());
			}
		if(updateCoreTextdomain)
			// Updating the core textdomain.
			for(const auto hook : GetCoreHooks())
				(*hook)();
	}
}



void LocaleInfo::SetLocale()
{
	SetLocale(locale(""));
}



void LocaleInfo::SetLocale(const locale &loc)
{
	// Set the Locale globally for gettext.
	// The gettext uses only LC_MESSAGES and LC_CTYPE. LC_MESSAGES detemines the
	// language, and LC_CTYPE detemines character encoding.
	// Local locale should not be set LC_ALL, bacause something like DataWriter
	// that use stream I/O assume LC_NUMERIC is C locale.
	localLocale = locale(locale(), loc, locale::messages);
	// The character encoding of LC_CTYPE should be UTF-8.
	const string utf8LocaleNames[] = { ReplaceEncodingToUTF8(locale("")), "C.UTF-8", "en_US.UTF-8" };
	for(auto name : utf8LocaleNames)
	{
		bool found = true;
		try
		{
			localLocale = locale(localLocale, locale(name), locale::ctype);
		}
		catch(runtime_error &e)
		{
			found = false;
		}
		if(found)
			break;
	}
	locale::global(localLocale);
	
	langName = GetLanguageOfMessages(localLocale);
	
	// Default textdomain always sets the core textdomain.
	::textdomain(coreTextdomainName.c_str());
	
	const auto& np = use_facet<numpunct<char>>(localLocale);
	decimalPoint = np.decimal_point();
	thousandsSep = np.thousands_sep();
}



void LocaleInfo::Init(const vector<string> &sources)
{
	for(const string &source : sources)
		BindTextdomain(source + "locales/");
}



string LocaleInfo::TranslateCore(const string &msgid)
{
	if(msgid.empty())
		return string();
	return string(gettext(msgid.c_str()));
}



string LocaleInfo::TranslateCore(const string &msgid, const string &context)
{
	if(msgid.empty())
		return string();
	if(context.empty())
		return TranslateCore(msgid);
	string id(context + contextSeparator + msgid);
	const char *p = id.c_str();
	const char *t = gettext(p);
	if(p == t)
		// This is equivalent to remove context information.
		return msgid;
	else
		return string(t);
}



string LocaleInfo::TranslateCore(const string &msgid, const string &msgid_plural, unsigned long n)
{
	if(msgid.empty())
		return string();
	return string(ngettext(msgid.c_str(), msgid_plural.c_str(), n));
}



string LocaleInfo::TranslateCore(const string &msgid, const string &msgid_plural,
	const string &context, unsigned long n)
{
	if(msgid.empty())
		return string();
	if(context.empty())
		return TranslateCore(msgid, msgid_plural, n);
	string id1(context + contextSeparator + msgid);
	string id2(context + contextSeparator + msgid_plural);
	const char *p1 = id1.c_str();
	const char *p2 = id2.c_str();
	const char *t = ngettext(p1, p2, n);
	if(p1 == t)
		// This is equivalent to remove context information.
		return msgid;
	else if(p2 == t)
		return msgid_plural;
	else
		return string(t);
}



string LocaleInfo::TranslateData(const string &msgid)
{
	if(stopTranslatingData)
		return msgid;
	if(msgid.empty())
		return string();
	const char *p = msgid.c_str();
	for(const auto &s : boundDataTextdomains)
	{
		const char *t = dgettext(s.c_str(), p);
		if(t != p)
			return string(t);
	}
	return msgid;
}



string LocaleInfo::TranslateData(const string &msgid, const string &context)
{
	if(stopTranslatingData)
		return msgid;
	if(msgid.empty())
		return string();
	if(context.empty())
		return TranslateData(msgid);
	string id(context + contextSeparator + msgid);
	const char *p = id.c_str();
	for(const auto &s : boundDataTextdomains)
	{
		const char *t = dgettext(s.c_str(), p);
		if(t != p)
			return string(t);
	}
	return msgid;
}



string LocaleInfo::TranslateData(const string &msgid, const string &msgid_plural, unsigned long n)
{
	if(stopTranslatingData)
		return msgid;
	if(msgid.empty())
		return string();
	const char *p1 = msgid.c_str();
	const char *p2 = msgid_plural.c_str();
	const char *t = nullptr;
	for(const auto &s : boundDataTextdomains)
	{
		t = dngettext(s.c_str(), p1, p2, n);
		if(t != p1 || t != p2)
			return string(t);
	}
	if(t == p1)
		return msgid;
	else
		return msgid_plural;
}



string LocaleInfo::TranslateData(const string &msgid, const string &msgid_plural,
	const string &context, unsigned long n)
{
	if(stopTranslatingData)
		return msgid;
	if(msgid.empty())
		return string();
	if(context.empty())
		return TranslateData(msgid, msgid_plural, n);
	string id1(context + contextSeparator + msgid);
	string id2(context + contextSeparator + msgid_plural);
	const char *p1 = id1.c_str();
	const char *p2 = id2.c_str();
	const char *t = nullptr;
	for(const auto &s : boundDataTextdomains)
	{
		t = dngettext(s.c_str(), p1, p2, n);
		if(t != p1 || t != p2)
			return string(t);
	}
	if(t == p1)
		return msgid;
	else
		return msgid_plural;
}



DataNode LocaleInfo::TranslateNode(const DataNode &node)
{
	return LocaleInfo::TranslateNode(node, "");
}



DataNode LocaleInfo::TranslateNode(const DataNode &node, const string &context)
{
	if(stopTranslatingData)
		return node;
	DataWriter originalNode("");
	originalNode.Write(node);
	const string originalText = originalNode.GetString();
	const string translatedText = TranslateData(originalText, context);
	istringstream translatedStream(translatedText);
	DataFile translatedNodes(translatedStream);
	return *translatedNodes.begin();
}



void LocaleInfo::StopTranslatingData()
{
	stopTranslatingData = true;
}



void LocaleInfo::RestartTranslatingData()
{
	stopTranslatingData = false;
}



bool LocaleInfo::AddHookUpdatingCore(std::function<void()> *hook)
{
	auto r = GetCoreHooks().insert(hook);
	if(r.second)
		(*hook)();
	return r.second;
}



void LocaleInfo::RemoveHookUpdatingCore(std::function<void()> *hook)
{
	GetCoreHooks().erase(hook);
}



char LocaleInfo::GetDecimalPoint()
{
	return decimalPoint;
}



char LocaleInfo::GetThousandsSep()
{
	return thousandsSep;
}



namespace Gettext {
	T_::T_(const std::string &msgid)
		: context(""), original(msgid), translated(),
		f([&](){ translated = LocaleInfo::TranslateCore(original); })
	{
		LocaleInfo::AddHookUpdatingCore(&f);
	}
	T_::T_(const std::string &msgid, const std::string &ctx)
		: context(ctx), original(msgid), translated(),
		f([&](){ translated = LocaleInfo::TranslateCore(original, context); })
	{
		LocaleInfo::AddHookUpdatingCore(&f);
	}
	T_::T_(const T_ &a)
		: context(a.context), original(a.original), translated(),
		f([&](){ translated = LocaleInfo::TranslateCore(original, context); })
	{
		LocaleInfo::AddHookUpdatingCore(&f);
	}
	T_::~T_()
	{
		LocaleInfo::RemoveHookUpdatingCore(&f);
	}
	const T_ &T_::operator=(const T_ &a)
	{
		LocaleInfo::RemoveHookUpdatingCore(&f);
		context = a.context;
		original = a.original;
		f = [&](){ translated = LocaleInfo::TranslateCore(original, context); };
		LocaleInfo::AddHookUpdatingCore(&f);
		return *this;
	}
}
