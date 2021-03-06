/* $Id: gamestate_observer.cpp 48450 2011-02-08 20:55:18Z mordante $ */
/*
   Copyright (C) 2010 - 2011 by Yurii Chernyi <terraninfo@terraninfo.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * Base gamestate observer - useful to see if the gamestate has changed
 * between two points of time
 * @file ai/gamestate_observer.cpp
 */

#include "manager.hpp"
#include "gamestate_observer.hpp"

namespace ai {
// =======================================================================

gamestate_observer::gamestate_observer()
	: gamestate_change_counter_(0)
{
	ai::manager::add_gamestate_observer(this);
}


gamestate_observer::~gamestate_observer()
{
	ai::manager::remove_gamestate_observer(this);
}


void gamestate_observer::handle_generic_event(const std::string &/*event_name*/)
{
	++gamestate_change_counter_;
}


bool gamestate_observer::is_gamestate_changed()
{
	return (gamestate_change_counter_>0);
}


void gamestate_observer::reset()
{
	gamestate_change_counter_=0;
}

// =======================================================================
} //end of namespace ai
