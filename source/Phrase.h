/* Phrase.h
Copyright (c) 2014 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef PHRASE_H_
#define PHRASE_H_

#include "DataNode.h"

#include <functional>
#include <string>
#include <vector>



// Class representing a set of rules for generating random ship names.
class Phrase {
public:
	Phrase() = default;
	Phrase(const Phrase &a) = delete;
	Phrase &operator=(const Phrase &a) = delete;
	~Phrase() noexcept;
	
	void Load(const DataNode &node);
	
	const std::string &Name() const;
	std::string Get() const;
	
	// Parse all nodes.
	void ParseAllNodes();
	
	
private:
	bool ReferencesPhrase(const Phrase *phrase) const;
	
	
private:
	// Parse one nodes.
	void ParseNode(const DataNode &node);
	
	class Part {
	public:
		std::vector<std::string> words;
		std::vector<const Phrase *> phrases;
		std::vector<std::function<std::string(const std::string&)>> replaceRules;
	};
	
	
private:
	std::string name;
	std::vector<std::vector<Part>> parts;
	
	// Original data node.
	std::vector<DataNode> originalNode;
};



#endif
