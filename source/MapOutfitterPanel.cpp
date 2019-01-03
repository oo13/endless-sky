/* MapOutfitterPanel.cpp
Copyright (c) 2015 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "MapOutfitterPanel.h"

#include "Format.h"
#include "GameData.h"
#include "LocaleInfo.h"
#include "Outfit.h"
#include "Planet.h"
#include "PlayerInfo.h"
#include "Point.h"
#include "Screen.h"
#include "Sprite.h"
#include "StellarObject.h"
#include "System.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

using namespace std;
using namespace Gettext;

namespace {
	const T_ MINE = T_("Mine this here");
	const T_ LABEL[] = {T_("Has no outfitter"), T_("Has outfitter"), T_("Sells this outfit")};
}



MapOutfitterPanel::MapOutfitterPanel(PlayerInfo &player)
	: MapSalesPanel(player, true)
{
	Init();
}



MapOutfitterPanel::MapOutfitterPanel(const MapPanel &panel, bool onlyHere)
	: MapSalesPanel(panel, true)
{
	Init();
	onlyShowSoldHere = onlyHere;
}



const Sprite *MapOutfitterPanel::SelectedSprite() const
{
	return selected ? selected->Thumbnail() : nullptr;
}



const Sprite *MapOutfitterPanel::CompareSprite() const
{
	return compare ? compare->Thumbnail() : nullptr;
}



const ItemInfoDisplay &MapOutfitterPanel::SelectedInfo() const
{
	return selectedInfo;
}



const ItemInfoDisplay &MapOutfitterPanel::CompareInfo() const
{
	return compareInfo;
}



const string &MapOutfitterPanel::KeyLabel(int index) const
{
	if(index == 2 && selected && selected->Get("installable") < 0)
		return MINE;
	
	return LABEL[index];
}



void MapOutfitterPanel::Select(int index)
{
	if(index < 0 || index >= static_cast<int>(list.size()))
		selected = nullptr;
	else
	{
		selected = list[index];
		selectedInfo.Update(*selected, player);
	}
	UpdateCache();
}



void MapOutfitterPanel::Compare(int index)
{
	if(index < 0 || index >= static_cast<int>(list.size()))
		compare = nullptr;
	else
	{
		compare = list[index];
		compareInfo.Update(*compare, player);
	}
}



double MapOutfitterPanel::SystemValue(const System *system) const
{
	if(!system)
		return numeric_limits<double>::quiet_NaN();
	
	auto it = player.Harvested().lower_bound(pair<const System *, const Outfit *>(system, nullptr));
	for( ; it != player.Harvested().end() && it->first == system; ++it)
		if(it->second == selected)
			return 1.;
	
	if(!system->IsInhabited(player.Flagship()))
		return numeric_limits<double>::quiet_NaN();
	
	double value = -.5;
	for(const StellarObject &object : system->Objects())
		if(object.GetPlanet())
		{
			const auto &outfitter = object.GetPlanet()->Outfitter();
			if(outfitter.Has(selected))
				return 1.;
			if(!outfitter.empty())
				value = 0.;
		}
	return value;
}



int MapOutfitterPanel::FindItem(const string &text) const
{
	int bestIndex = 9999;
	int bestItem = -1;
	for(unsigned i = 0; i < list.size(); ++i)
	{
		int index = Search(list[i]->Name(), text);
		if(index >= 0 && index < bestIndex)
		{
			bestIndex = index;
			bestItem = i;
			if(!index)
				return i;
		}
	}
	return bestItem;
}



void MapOutfitterPanel::DrawItems()
{
	list.clear();
	Point corner = Screen::TopLeft() + Point(0, scroll);
	for(const string &category : categories)
	{
		auto it = catalog.find(category);
		if(it == catalog.end())
			continue;
		
		// Draw the header. If this category is collapsed, skip drawing the items.
		if(DrawHeader(corner, category))
			continue;
		
		for(const Outfit *outfit : it->second)
		{
			string price = Format::Credits(outfit->Cost()) + T(" credits", "MapOUtfitterPanel");
			
			string info;
			if(outfit->Get("installable") < 0.)
				info = T("(Mined from asteroids)");
			else
			{
				double space = -outfit->Get("outfit space");
				string kind_of_outfit(T("outfit", "kind of outfit"));
				if(space && -outfit->Get("weapon capacity") == space)
					kind_of_outfit = T("weapon", "kind of outfit");
				else if(space && -outfit->Get("engine capacity") == space)
					kind_of_outfit = T("engine", "kind of outfit");
				// TRANSLATORS: %1%: space, %2%: ton(s), %3%: kind of outfit
				info = Format::StringF({T("%1% %2% of %3% space"), Format::Number(space),
					nT("ton", "tons", "MapOutfitterPanel", abs(space)), kind_of_outfit});
			}
			
			bool isForSale = true;
			if(selectedSystem && player.HasVisited(selectedSystem))
			{
				isForSale = false;
				for(const StellarObject &object : selectedSystem->Objects())
					if(object.GetPlanet() && object.GetPlanet()->Outfitter().Has(outfit))
					{
						isForSale = true;
						break;
					}
			}
			if(!isForSale && onlyShowSoldHere)
				continue;
			
			Draw(corner, outfit->Thumbnail(), isForSale, outfit == selected,
				outfit->Name(), price, info);
			list.push_back(outfit);
		}
	}
	maxScroll = corner.Y() - scroll - .5 * Screen::Height();
}



void MapOutfitterPanel::Init()
{
	catalog.clear();
	set<const Outfit *> seen;
	for(const auto &it : GameData::Planets())
		if(player.HasVisited(it.second.GetSystem()))
			for(const Outfit *outfit : it.second.Outfitter())
				if(!seen.count(outfit))
				{
					catalog[outfit->Category()].push_back(outfit);
					seen.insert(outfit);
				}
	for(const auto &it : player.Harvested())
		if(!seen.count(it.second))
		{
			catalog[it.second->Category()].push_back(it.second);
			seen.insert(it.second);
		}
	
	for(auto &it : catalog)
		sort(it.second.begin(), it.second.end(),
			[](const Outfit *a, const Outfit *b) {return a->Identifier() < b->Identifier();});
}
