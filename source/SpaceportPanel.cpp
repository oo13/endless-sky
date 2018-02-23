/* SpaceportPanel.cpp
Copyright (c) 2014 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "SpaceportPanel.h"

#include "text/alignment.hpp"
#include "Color.h"
#include "text/DisplayText.h"
#include "text/Font.h"
#include "text/FontSet.h"
#include "text/Format.h"
#include "GameData.h"
#include "text/Gettext.h"
#include "Interface.h"
#include "text/layout.hpp"
#include "News.h"
#include "Planet.h"
#include "PlayerInfo.h"
#include "Point.h"
#include "Random.h"
#include "UI.h"

using namespace std;
using namespace Gettext;



SpaceportPanel::SpaceportPanel(PlayerInfo &player)
	: player(player), updateCatalog([this](){ UpdateTranslation(); })
{
	SetTrapAllEvents(false);
	
	text = player.GetPlanet()->SpaceportDescription();
	AddHookUpdating(&updateCatalog);
	
	// Query the news interface to find out the wrap width.
	// TODO: Allow Interface to handle wrapped text directly.
	const Interface *newsUi = GameData::Interfaces().Get("news");
	portraitWidth = newsUi->GetBox("message portrait").Width();
	normalWidth = newsUi->GetBox("message").Width();
}



SpaceportPanel::~SpaceportPanel()
{
	RemoveHookUpdating(&updateCatalog);
}



void SpaceportPanel::UpdateNews()
{
	const News *news = PickNews();
	if(!news)
		return;
	hasNews = true;
	
	// Randomly pick which portrait, if any, is to be shown. Depending on if
	// this news has a portrait, different interface information gets filled in. 
	auto portrait = news->Portrait();
	// Cache the randomly picked results until the next update is requested.
	hasPortrait = portrait;
	newsInfo.SetSprite("portrait", portrait);
	newsInfo.SetString("name", Format::StringF(T("%1%:"), news->Name()));
	newsMessage = news->Message();
}



void SpaceportPanel::Step()
{
	if(GetUI()->IsTop(this))
	{
		Mission *mission = player.MissionToOffer(Mission::SPACEPORT);
		// Special case: if the player somehow got to the spaceport before all
		// landing missions were offered, they can still be offered here:
		if(!mission)
			mission = player.MissionToOffer(Mission::LANDING);
		if(mission)
			mission->Do(Mission::OFFER, player, GetUI());
		else
			player.HandleBlockedMissions(Mission::SPACEPORT, GetUI());
	}
}



void SpaceportPanel::Draw()
{
	if(player.IsDead())
		return;
	
	const Font &font = FontSet::Get(14);
	font.Draw({text, {480, Alignment::JUSTIFIED}}, Point(-300., 80.),
		*GameData::Colors().Get("bright"));
	
	if(hasNews)
	{
		const Interface *newsUi = GameData::Interfaces().Get("news");
		newsUi->Draw(newsInfo);
		// Depending on if the news has a portrait, the interface box that
		// gets filled in changes.
		const int newsWidth = hasPortrait ? portraitWidth : normalWidth;
		const auto newsLayout = Layout(newsWidth, Alignment::JUSTIFIED);
		font.Draw({newsMessage, newsLayout},
			newsUi->GetBox(hasPortrait ? "message portrait" : "message").TopLeft(),
			*GameData::Colors().Get("medium"));
	}
}



// Pick a random news object that applies to the player's planets and conditions.
// If there is no applicable news, this returns null.
const News *SpaceportPanel::PickNews() const
{
	vector<const News *> matches;
	const Planet *planet = player.GetPlanet();
	const map<string, int64_t> &conditions = player.Conditions();
	for(const auto &it : GameData::SpaceportNews())
		if(!it.second.IsEmpty() && it.second.Matches(planet, conditions))
			matches.push_back(&it.second);
	
	return matches.empty() ? nullptr : matches[Random::Int(matches.size())];
}



void SpaceportPanel::UpdateTranslation()
{
	const Planet *p = player.GetPlanet();
	if(p)
		text = p->SpaceportDescription();
}
