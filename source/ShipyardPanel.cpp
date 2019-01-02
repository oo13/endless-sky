/* ShipyardPanel.cpp
Copyright (c) 2014 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "ShipyardPanel.h"

#include "Color.h"
#include "Dialog.h"
#include "Font.h"
#include "FontSet.h"
#include "Format.h"
#include "GameData.h"
#include "Phrase.h"
#include "Planet.h"
#include "PlayerInfo.h"
#include "Point.h"
#include "Screen.h"
#include "Ship.h"
#include "SpriteSet.h"
#include "SpriteShader.h"
#include "UI.h"

class System;

using namespace std;
using namespace Gettext;

namespace {
	// The name entry dialog should include a "Random" button to choose a random
	// name using the civilian ship name generator.
	class NameDialog : public Dialog {
	public:
		NameDialog(ShipyardPanel *panel, void (ShipyardPanel::*fun)(const string &), const string &message)
			: Dialog(panel, fun, message) {}
		
		virtual void Draw() override
		{
			Dialog::Draw();
			
			randomPos = cancelPos - Point(80., 0.);
			SpriteShader::Draw(SpriteSet::Get("ui/dialog cancel"), randomPos);

			const Font &font = FontSet::Get(14);
			const string label = T("Random");
			Point labelPos = randomPos - .5 * Point(font.Width(label), font.Height());
			font.Draw(label, labelPos, *GameData::Colors().Get("medium"));
		}
		
	protected:
		virtual bool Click(int x, int y, int clicks) override
		{
			Point off = Point(x, y) - randomPos;
			if(fabs(off.X()) < 40. && fabs(off.Y()) < 20.)
			{
				input = GameData::Phrases().Get("civilian")->Get();
				return true;
			}
			return Dialog::Click(x, y, clicks);
		}
		
	private:
		Point randomPos;
	};
	
	
	
	// The format string for list of ships
	Format::ListOfWords listOfShips;
	
	// The Hook of translation.
	function<void()> updateCoreTextdomain([](){
		// TRANSLATORS: The separators of ships
		listOfShips.SetSeparators(T(": and :,\n:,\nand "));
	});
	// Set the hook.
	bool hooked = LocaleInfo::AddHookUpdatingCore(&updateCoreTextdomain);
}



ShipyardPanel::ShipyardPanel(PlayerInfo &player)
	: ShopPanel(player, false), modifier(0)
{
	for(const auto &it : GameData::Ships())
		catalog[it.second.Attributes().Category()].insert(it.first);
	
	if(player.GetPlanet())
		shipyard = player.GetPlanet()->Shipyard();
}



int ShipyardPanel::TileSize() const
{
	return SHIP_SIZE;
}



int ShipyardPanel::DrawPlayerShipInfo(const Point &point)
{
	shipInfo.Update(*playerShip, player.FleetDepreciation(), player.GetDate().DaysSinceEpoch());
	shipInfo.DrawSale(point);
	shipInfo.DrawAttributes(point + Point(0, shipInfo.SaleHeight()));
	
	return shipInfo.SaleHeight() + shipInfo.AttributesHeight();
}



bool ShipyardPanel::HasItem(const string &name) const
{
	const Ship *ship = GameData::Ships().Get(name);
	return shipyard.Has(ship);
}



void ShipyardPanel::DrawItem(const string &name, const Point &point, int scrollY)
{
	const Ship *ship = GameData::Ships().Get(name);
	zones.emplace_back(point, Point(SHIP_SIZE, SHIP_SIZE), ship, scrollY);
	if(point.Y() + SHIP_SIZE / 2 < Screen::Top() || point.Y() - SHIP_SIZE / 2 > Screen::Bottom())
		return;
	
	DrawShip(*ship, point, ship == selectedShip);
}



int ShipyardPanel::DividerOffset() const
{
	return 121;
}



int ShipyardPanel::DetailWidth() const
{
	return 3 * shipInfo.PanelWidth();
}



int ShipyardPanel::DrawDetails(const Point &center)
{
	shipInfo.Update(*selectedShip, player.StockDepreciation(), player.GetDate().DaysSinceEpoch());
	Point offset(shipInfo.PanelWidth(), 0.);
	
	shipInfo.DrawDescription(center - offset * 1.5);
	shipInfo.DrawAttributes(center - offset * .5);
	shipInfo.DrawOutfits(center + offset * .5);
	
	return shipInfo.MaximumHeight();
}



bool ShipyardPanel::CanBuy() const
{
	if(!selectedShip)
		return false;
	
	int64_t cost = player.StockDepreciation().Value(*selectedShip, day);
	
	// Check that the player has any necessary licenses.
	int64_t licenseCost = LicenseCost(&selectedShip->Attributes());
	if(licenseCost < 0)
		return false;
	cost += licenseCost;
	
	return (player.Accounts().Credits() >= cost);
}



void ShipyardPanel::Buy(bool fromCargo)
{
	int64_t licenseCost = LicenseCost(&selectedShip->Attributes());
	if(licenseCost < 0)
		return;
	
	modifier = Modifier();
	string message;
	if(licenseCost)
		// TRANSLATORS: %1%: license cost, %2%: model name
		message = Format::StringF({T("Note: you will need to pay %1% credits for "
			"the licenses required to operate this ship, in addition to its cost."
			" If that is okay with you, go ahead and enter a name for your brand new %2%!"),
			Format::Credits(licenseCost), selectedShip->ModelName(modifier)});
	else
		// TRANSLATORS: %1%: model name
		message = Format::StringF({T("Enter a name for your brand new %1%!"), selectedShip->ModelName(modifier)});
	message += nT(" (Or leave it blank to use a randomly chosen name.)",
		"(Or leave it blank to use randomly chosen names.)", modifier);
	
	GetUI()->Push(new NameDialog(this, &ShipyardPanel::BuyShip, message));
}



void ShipyardPanel::FailBuy() const
{
	if(!selectedShip)
		return;
	
	int64_t cost = player.StockDepreciation().Value(*selectedShip, day);
	
	// Check that the player has any necessary licenses.
	int64_t licenseCost = LicenseCost(&selectedShip->Attributes());
	if(licenseCost < 0)
	{
		GetUI()->Push(new Dialog(T("Buying this ship requires a special license. "
			"You will probably need to complete some sort of mission to get one.")));
		return;
	}
	
	cost += licenseCost;
	if(player.Accounts().Credits() < cost)
	{
		for(const auto &it : player.Ships())
			cost -= player.FleetDepreciation().Value(*it, day);
		if(player.Accounts().Credits() < cost)
			GetUI()->Push(new Dialog(T("You do not have enough credits to buy this ship. "
				"Consider checking if the bank will offer you a loan.")));
		else
			// TRANSLATORS: %1%: (one of) your current ship(s)
			GetUI()->Push(new Dialog(Format::StringF({T("You do not have enough credits to buy this ship. "
				"If you want to buy it, you must sell %1% first."),
				nT("your current ship", "one of your ships", player.Ships().size())})));
		return;
	}
}



bool ShipyardPanel::CanSell(bool toCargo) const
{
	return playerShip;
}



void ShipyardPanel::Sell(bool toCargo)
{
	static const int MAX_LIST = 20;
	static const int MAX_NAME_WIDTH = 250 - 30;
	
	const int count = playerShips.size();
	const Font &font = FontSet::Get(14);
	vector<string> shipNames;
	auto it = playerShips.begin();
	for(int i = 0; i < count; ++i)
		if(i == MAX_LIST - 1 && count > MAX_LIST)
		{
			const unsigned long otherCount = count - (MAX_LIST - 1);
			// TRANSLATORS: %1%: number of ship (>=2)
			shipNames.push_back(Format::StringF({nT("%1% other ship", "%1% other ships", otherCount),
				Format::Number(otherCount)}));
			break;
		}
		else
			shipNames.push_back(font.TruncateMiddle((*it++)->Name(), MAX_NAME_WIDTH));
	
	// To allow calculating the sale price of all the ships in the list,
	// temporarily copy into a shared_ptr vector:
	vector<shared_ptr<Ship>> toSell;
	for(const auto &it : playerShips)
		toSell.push_back(it->shared_from_this());
	int64_t total = player.FleetDepreciation().Value(toSell, day);
	
	auto it2 = shipNames.begin();
	// TRANSLATORS: %1%: list of ship names, %2%: line break or space, %3%: price
	const string message = Format::StringF({T("Sell %1%%2%for %3% credits?"),
		listOfShips.GetList(shipNames.size(), [&it2](){ return *it2++; }),
		(count > 2) ? T("\n") : T(" ", "ShipyardPanel"), Format::Credits(total)});
	GetUI()->Push(new Dialog(this, &ShipyardPanel::SellShip, message));
}



bool ShipyardPanel::CanSellMultiple() const
{
	return false;
}



void ShipyardPanel::BuyShip(const string &name)
{
	int64_t licenseCost = LicenseCost(&selectedShip->Attributes());
	if(licenseCost)
	{
		player.Accounts().AddCredits(-licenseCost);
		for(const string &licenseName : selectedShip->Attributes().Licenses())
			if(player.GetCondition("license: " + licenseName) <= 0)
				player.Conditions()["license: " + licenseName] = true;
	}
	
	for(int i = 1; i <= modifier; ++i)
	{
		// If no name is given, choose a random name. Otherwise, if buying
		// multiple ships, append a number to the given ship name.
		string shipName = name;
		if(name.empty())
			shipName = GameData::Phrases().Get("civilian")->Get();
		else if(modifier > 1)
			shipName += " " + to_string(i);
		
		player.BuyShip(selectedShip, shipName);
	}
	
	playerShip = &*player.Ships().back();
	playerShips.clear();
	playerShips.insert(playerShip);
}



void ShipyardPanel::SellShip()
{
	for(Ship *ship : playerShips)
		player.SellShip(ship);
	playerShips.clear();
	playerShip = nullptr;
	for(const shared_ptr<Ship> &ship : player.Ships())
		if(ship->GetSystem() == player.GetSystem() && !ship->IsDisabled())
		{
			playerShip = ship.get();
			break;
		}
	if(playerShip)
		playerShips.insert(playerShip);
	player.UpdateCargoCapacities();
}
