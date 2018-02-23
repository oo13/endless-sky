/* Gettext.cpp
Copyright (c) 2019 by OOTA, Masato

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "Gettext.h"

#include "../DataFile.h"
#include "../DataWriter.h"
#include "../Files.h"
#include "../spiritless_po/spiritless_po.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <utility>

using namespace std;
using namespace Gettext;

namespace {
	spiritless_po::Catalog catalog;
	
	// C++ ensures that initialized set to 0 (false) before someone calls GetHooks.
	bool initialized;
	bool stopTranslating = false;
	
	// Return the set of hook functions.
	// C++ ensures that the return value has been initialized when it uses for
	// initialization of any static variables.
	set<function<void()>*> &GetHooks()
	{
		// Don't destruct this object because it must be alive until all static
		// variables are destructed.
		static set<function<void()>*> *setOfUpdatingHook(new set<function<void()>*>);
		return *setOfUpdatingHook;
	}
	
	// List of instances which need to translate.
	list<T_*> &GetListOfT_()
	{
		static list<T_*> *listOfT_(new list<T_*>);
		return *listOfT_;
	}
	
	// Invalid iterator of list<T_*>.
	list<T_*>::iterator InvalidT_()
	{
		return GetListOfT_().end();
	}
};

namespace Gettext {
	T_::T_()
		: iterToThis(InvalidT_())
	{}
	
	
	
	T_::T_(const string &msgid)
		: original(msgid), translated(msgid), iterToThis(InvalidT_())
	{
		AddToList();
	}
	
	
	
	T_::T_(const string &msgid, const string &ctx)
		: context(ctx), original(msgid), translated(msgid), iterToThis(InvalidT_())
	{
		AddToList();
	}
	
	
	
	T_::T_(const T_ &a)
		: context(a.context), original(a.original), translated(a.translated), iterToThis(InvalidT_())
	{
		if(a.iterToThis != InvalidT_())
			AddToList(true);
	}
	
	
	
	T_::T_(T_ &&a) noexcept
		: context(std::move(a.context)), original(std::move(a.original)), translated(std::move(a.translated)),
		iterToThis(std::move(a.iterToThis))
	{
		const auto invalid = InvalidT_();
		if(iterToThis != invalid)
			*iterToThis = this;
		a.iterToThis = invalid;
	}
	
	
	
	T_::T_(const string &msg, ForceT forceType)
		: context(), original(msg), translated(msg), iterToThis(InvalidT_())
	{
		if(forceType == FORCE)
			AddToList(true);
	}
	
	
	
	T_::~T_()
	{
		RemoveToList();
	}
	
	
	
	T_ &T_::operator=(const T_ &a)
	{
		if(this != &a) {
			RemoveToList();
			context = a.context;
			original = a.original;
			translated = a.translated;
			if(a.iterToThis != InvalidT_())
				AddToList(true);
		}
		return *this;
	}
	
	
	
	T_ &T_::operator=(T_ &&a) noexcept
	{
		RemoveToList();
		
		context = std::move(a.context);
		original = std::move(a.original);
		translated = std::move(a.translated);
		iterToThis = std::move(a.iterToThis);
		
		const auto invalid = InvalidT_();
		if(iterToThis != invalid)
			*iterToThis = this;
		a.iterToThis = invalid;
		return *this;
	}
	
	
	
	void T_::Update()
	{
		translated = T(original, context);
	}
	
	
	
	void T_::Clear()
	{
		RemoveToList();
		context.clear();
		original.clear();
		translated.clear();
	}
	
	
	
	void T_::AddToList(bool force)
	{
		if(!stopTranslating || force)
		{
			auto &listOfT_ = GetListOfT_();
			listOfT_.push_front(this);
			iterToThis = listOfT_.begin();
			Update();
		}
	}
	
	
	
	void T_::RemoveToList()
	{
		if(iterToThis != InvalidT_())
		{
			GetListOfT_().erase(iterToThis);
			iterToThis = InvalidT_();
		}
	}
	
	
	
	string T(const string &msgid)
	{
		if(!initialized || stopTranslating || msgid.empty())
			return msgid;
		else
			return catalog.gettext(msgid);
	}
	
	
	
	string T(const string &msgid, const string &context)
	{
		const bool mz = msgid.empty();
		const bool cz = context.empty();
		if(!initialized || stopTranslating || (mz && cz))
			return msgid;
		else if(cz)
			return catalog.gettext(msgid);
		else
			return catalog.pgettext(context, msgid);
	}
	
	
	
	string nT(const string &msgid, const string &msgid_plural, unsigned long n)
	{
		if(!initialized || stopTranslating)
		{
			if(n == 1)
				return msgid;
			else
				return msgid_plural;
		}
		else
			return catalog.ngettext(msgid, msgid_plural, n);
	}
	
	
	
	string nT(const string &msgid, const string &msgid_plural,
		const string &context, unsigned long n)
	{
		if(!initialized || stopTranslating)
		{
			if(n == 1)
				return msgid;
			else
				return msgid_plural;
		}
		else if(context.empty())
			return catalog.ngettext(msgid, msgid_plural, n);
		else
			return catalog.npgettext(context, msgid, msgid_plural, n);
	}
	
	
	
	DataNode TranslateNode(const DataNode &node)
	{
		return TranslateNode(node, "");
	}
	
	
	
	DataNode TranslateNode(const DataNode &node, const std::string &context)
	{
		if(!initialized || stopTranslating)
			return node;
		DataWriter originalNode("");
		originalNode.Write(node);
		const string originalText = originalNode.GetString();
		const string translatedText = T(originalText, context);
		istringstream translatedStream(translatedText);
		DataFile translatedNodes(translatedStream);
		return *translatedNodes.begin();
	}
	
	
	
	string Concat(const vector<T_> &a)
	{
		string s;
		for(const auto &t : a)
			s += t.Str();
		return s;
	}
	
	
	
	bool IsEmptyText(const vector<T_> &a)
	{
		for(const auto &t : a)
			if(!t.Str().empty())
				return false;
		return true;
	}
	
	
	
	void FireHooks()
	{
		for(auto it : GetListOfT_())
			it->Update();
		for(const auto hook : GetHooks())
			(*hook)();
	}
	
	
	
	void UpdateCatalog(const vector<string> &catalogFilenames)
	{
		vector<unique_ptr<istream>> holder;
		vector<pair<istream*, const string *>> files;
		for(const auto &filename : catalogFilenames)
		{
			holder.emplace_back(new ifstream(filename));
			files.push_back(make_pair(holder.back().get(), &filename));
		}
		UpdateCatalog(files);
	}
	
	
	
	void UpdateCatalog(const vector<pair<istream*, const string *>> &catalogStreams)
	{
		catalog.Clear();
		for(const auto &stream : catalogStreams)
		{
			catalog.ClearError();
			if(!catalog.Add(*stream.first))
				for(const auto &msg : catalog.GetError())
					Files::LogError(*stream.second + ": " + msg);
		}
		initialized = true;
		FireHooks();
	}
	
	
	
	void StopTranslating()
	{
		stopTranslating = true;
	}
	
	
	
	void RestartTranslating()
	{
		stopTranslating = false;
		FireHooks();
	}
	
	
	
	bool IsTranslating()
	{
		return !stopTranslating;
	}
	
	
	
	bool AddHookUpdating(function<void()> *hook)
	{
		auto r = GetHooks().insert(hook);
		(*hook)();
		return r.second;
	}
	
	
	
	void RemoveHookUpdating(function<void()> *hook)
	{
		GetHooks().erase(hook);
	}
}
