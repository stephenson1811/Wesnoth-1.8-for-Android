/* $Id: multiplayer_connect.hpp 48450 2011-02-08 20:55:18Z mordante $ */
/*
   Copyright (C) 2007 - 2011
   Part of the Battle for Wesnoth Project http://www.wesnoth.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/** @file multiplayer_connect.hpp */

#ifndef MULTIPLAYER_CONNECT_H_INCLUDED
#define MULTIPLAYER_CONNECT_H_INCLUDED

#include "multiplayer_connect_ui.hpp"

#include "config.hpp"
#include "gamestatus.hpp"
#include "leader_list.hpp"
#include "network.hpp"

#include <string>

namespace ai {
	struct description;
}

namespace mp {

class connect_side {
	typedef connect_side side;
public:
	void hide_ai_algorithm_combo(bool invis);

	connect_side(connect& parent, const config& cfg, int index, side_ui* ui);

	connect_side(const connect_side& a);

	/** Returns true if this side changed since last call to changed(). */
	bool changed();

	/**
	 * Gets a config object representing this side.
	 *
	 * If include_leader is set to true, the config objects include the
	 * "type=" defining the leader type, else it does not.
	 */
	config get_config() const;

	/**
	 * Returns true if this side is waiting for a network player and
	 * players allowed.
	 */
	bool available(const std::string& name = "") const;

	/** Returns true, if the player has chosen his leader and this side is ready for the game to start */
	bool ready_for_start() const;

	/** Return true if players are allowed to take this side. */
	bool allow_player() const;

	/** Sets the controller of a side. */
	void set_controller(mp::controller controller);
	mp::controller get_controller() const;

	/** Adds an user to the user list combo. */
	void update_user_list();

	/** Returns the username of this side. */
	const std::string& get_current_player() const
		{ return current_player_; }

	const std::string& get_player_id() const;

	/** Sets the username of this side. */
	void set_player_id(const std::string& player_id);

	/** Sets if the joining player has chosen his leader. */
	void set_ready_for_start(bool ready_for_start);

	const std::string& get_save_id() const;

	/**
	 * Imports data from the network into this side, and updates the UI
	 * accordingly.
	 */
	void import_network_user(const config& data);

	/** Resets this side to its default state, and updates the UI accordingly. */
	void reset(mp::controller controller);

	/** Resolves the random leader / factions. */
	void resolve_random();

	bool should_show_ai_combo() const;

	void swap_player(int index);

	void set_controller(int cntr);
	void set_colour(int index);
	void set_faction(int index);
	void set_ai(int index);
	void set_team(int index);
	void set_gold(int val);
	void set_income(int val);

	void update_gender_list();

	void change() { changed_ = true; }
private:
	connect_side();
	void init_ai_algorithm_combo();
	void update_ai_algorithm_combo();

	/** Fill or refresh the faction combo using the proper team color. */
	void update_faction_combo();
	void update_controller_ui();
	void update_ui();

	void update_faction_ui();

	/**
	 * The mp::connect widget owning this mp::connect::side.
	 *
	 * Used in the constructor, must be first.
	 */
	connect* parent_;

	/**
	 * A non-const config. Be careful not to insert keys when only reading.
	 *
	 * (Use cfg_.get_attribute().)
	 */
	config cfg_;

	// Configurable variables
	int index_;
	std::string id_;
	std::string player_id_;
	std::string save_id_;
	std::string current_player_;
	mp::controller controller_;
	int faction_;
	int team_;
	int colour_;
	int gold_;
	int income_;
	std::string leader_;
	std::string gender_;
	std::string ai_algorithm_;
	bool ready_for_start_;

	// Flags for controlling the dialog widgets of the game lobby
	bool gold_lock_;
	bool income_lock_;
	bool team_lock_;
	bool colour_lock_;

	bool allow_player_;
	bool allow_changes_;
	bool enabled_;
	bool faction_enabled_;
	bool changed_;

	side_ui* ui_;
	leader_list_manager llm_;
};

struct connected_user {
	connected_user(const std::string& name, mp::controller controller,
			network::connection connection) :
		name(name), controller(controller), connection(connection)
	{};
	std::string name;
	mp::controller controller;
	network::connection connection;
	operator std::string() const
	{
		return name;
	}
};

typedef std::vector<connected_user> connected_user_list;
typedef std::vector<connect_side> side_list;

class connect
{
	friend class connect_side;
	typedef connect_side side;
public:
	connect(connect_ui* ui, const config& game_config,
			config& gamelist, const mp_game_settings& params,
			mp::controller default_controller, bool local_players_only = false);

	void take_reserved_side(connect::side& side, const config& data);

	/**
	 * Returns the game state, which contains all information about the current
	 * scenario.
	 */
	const game_state& get_state();

	/**
	 * Updates the current game state, resolves random factions, and sends a
	 * "start game" message to the network.
	 */
	void start_game();

	void try_launch();

	const mp_game_settings& params() const { return params_; }

	void process_network_data(const config& data, const network::connection sock);
	void process_network_error(network::error& error);
	bool accept_connections();
	void process_network_connection(const network::connection sock);

	void full_update() { update_playerlist_state(); update_and_send_diff(); }

private:
	// Those 2 functions are actually the steps of the (complex)
	// construction of this class.

	/**
	 * Called by the constructor to initialize the game from a
	 * create::parameters structure.
	 */
	void load_game();
	void lists_init();

	/** Convenience function. */
	config* current_config();

	/** Updates the level_ variable to reflect the sides in the sides_ vector. */
	void update_level();

	/** Updates the level, and send a diff to the clients. */
	void update_and_send_diff(bool update_time_of_day = false);

	/** Returns true if there still are sides available for this game. */
	bool sides_available() const;

	/** Returns true if all sides are ready to start the game. */
	bool sides_ready() const;

	/**
	 * Validates whether the game can be started.
	 *
	 * returns                       Can the game be started?
	 */
	bool can_start_game() const;

	/**
	 * Updates the state of the player list, the launch button and of the start
	 * game label, to reflect the actual state.
	 */
	void update_playerlist_state(bool silent=true);

	/** Returns the index of a player, from its id, or -1 if the player was not found. */
	connected_user_list::iterator find_player(const std::string& id);

	/** Returns the side which is taken by a given player, or -1 if none was found. */
	int find_player_side(const std::string& id) const;
	
	void set_result(ui::result res) { ui_->set_result(res); }
	ui::result get_result() { return ui_->get_result(); }

	/** Adds a player. */
	void update_user_combos();

	const config& game_config_;
	config& gamelist_;

	bool local_only_;

	config level_;

	/** This is the "game state" object which is created by this dialog. */
	game_state state_;

	mp_game_settings params_;

	/** The list of available sides for the current era. */
	std::vector<const config *> era_sides_;

	// Lists used for combos
	std::vector<std::string> player_types_;
	std::vector<std::string> player_factions_;
	std::vector<std::string> player_teams_;
	std::vector<std::string> player_colours_;
	std::vector<ai::description*> ai_algorithms_;

	// team_name list and "Team" prefix
	std::vector<std::string> team_names_;
	std::vector<std::string> user_team_names_;
	const std::string team_prefix_;

	side_list sides_;
	connected_user_list users_;

	bool message_full_;

	controller default_controller_;

	connect_ui* ui_;
}; // end class connect

} // end namespace mp

#endif

