/* $Id: map_exception.hpp 48450 2011-02-08 20:55:18Z mordante $ */
/*
   Copyright (C) 2003 - 2011 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/** @file map_exception.hpp  */

#ifndef MAP_EXCEPTION_H_INCLUDED
#define MAP_EXCEPTION_H_INCLUDED

struct incorrect_map_format_exception {
	incorrect_map_format_exception(const std::string& msg) : msg_(msg) {}
	const std::string msg_;
};

#endif
