/* $Id: attack.cpp 49411 2011-05-07 15:11:50Z soliton $ */
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

/**
 * @file ai/default/attack.cpp
 * Calculate & analyse attacks of the default ai
 */

#include "../../global.hpp"

#include "ai.hpp"
#include "../actions.hpp"
#include "../manager.hpp"

#include "../../attack_prediction.hpp"
#include "foreach.hpp"
#include "../../map.hpp"
#include "../../log.hpp"

static lg::log_domain log_ai("ai/attack");
#define LOG_AI LOG_STREAM(info, log_ai)
#define ERR_AI LOG_STREAM(err, log_ai)

namespace ai {

void attack_analysis::analyze(const gamemap& map, unit_map& units,
				  const readonly_context& ai_obj,
                                  const move_map& dstsrc, const move_map& srcdst,
                                  const move_map& enemy_dstsrc, double aggression)
{
	const unit_map::const_iterator defend_it = units.find(target);
	assert(defend_it != units.end());

	// See if the target is a threat to our leader or an ally's leader.
	map_location adj[6];
	get_adjacent_tiles(target,adj);
	size_t tile;
	for(tile = 0; tile != 6; ++tile) {
		const unit_map::const_iterator leader = units.find(adj[tile]);
		if(leader != units.end() && leader->second.can_recruit() && ai_obj.current_team().is_enemy(leader->second.side()) == false) {
			break;
		}
	}

	leader_threat = (tile != 6);
	uses_leader = false;

	target_value = defend_it->second.cost();
	target_value += (double(defend_it->second.experience())/
	                 double(defend_it->second.max_experience()))*target_value;
	target_starting_damage = defend_it->second.max_hitpoints() -
	                         defend_it->second.hitpoints();

	// Calculate the 'alternative_terrain_quality' -- the best possible defensive values
	// the attacking units could hope to achieve if they didn't attack and moved somewhere.
	// This is used for comparative purposes, to see just how vulnerable the AI is
	// making itself.
	alternative_terrain_quality = 0.0;
	double cost_sum = 0.0;
	for(size_t i = 0; i != movements.size(); ++i) {
		const unit_map::const_iterator att = units.find(movements[i].first);
		const double cost = att->second.cost();
		cost_sum += cost;
		alternative_terrain_quality += cost*ai_obj.best_defensive_position(movements[i].first,dstsrc,srcdst,enemy_dstsrc).chance_to_hit;
	}
	alternative_terrain_quality /= cost_sum*100;

	avg_damage_inflicted = 0.0;
	avg_damage_taken = 0.0;
	resources_used = 0.0;
	terrain_quality = 0.0;
	avg_losses = 0.0;
	chance_to_kill = 0.0;

	double def_avg_experience = 0.0;
	double first_chance_kill = 0.0;

	double prob_dead_already = 0.0;
	assert(!movements.empty());
	std::vector<std::pair<map_location,map_location> >::const_iterator m;

	battle_context *prev_bc = NULL;
	const combatant *prev_def = NULL;

	for (m = movements.begin(); m != movements.end(); ++m) {
		// We fix up units map to reflect what this would look like.
		std::pair<map_location,unit> *up = units.extract(m->first);
		up->first = m->second;
		units.insert(up);

		if (up->second.can_recruit()) {
			uses_leader = true;
			// FIXME: suokko's r29531 omitted this line
			leader_threat = false;
		}

		int att_weapon = -1, def_weapon = -1;
		bool from_cache = false;
		battle_context *bc;

		// This cache is only about 99% correct, but speeds up evaluation by about 1000 times.
		// We recalculate when we actually attack.
		std::map<std::pair<map_location, const unit_type *>,std::pair<battle_context::unit_stats,battle_context::unit_stats> >::iterator usc;
		const unit_type* up_type = up->second.type();
		if(up_type) {
			usc = ai_obj.unit_stats_cache().find(std::pair<map_location, const unit_type *>(target, up_type));
		} else {
			usc = ai_obj.unit_stats_cache().end();
		}
		// Just check this attack is valid for this attacking unit (may be modified)
		if (usc != ai_obj.unit_stats_cache().end() &&
				usc->second.first.attack_num <
				static_cast<int>(up->second.attacks().size())) {

			from_cache = true;
			bc = new battle_context(usc->second.first, usc->second.second);
		} else {
			bc = new battle_context(units, m->second, target, att_weapon, def_weapon, aggression, prev_def);
		}
		const combatant &att = bc->get_attacker_combatant(prev_def);
		const combatant &def = bc->get_defender_combatant(prev_def);

		delete prev_bc;
		prev_bc = bc;
		prev_def = &bc->get_defender_combatant(prev_def);

		if (!from_cache && up_type) {
			ai_obj.unit_stats_cache().insert(std::pair<std::pair<map_location, const unit_type *>,std::pair<battle_context::unit_stats,battle_context::unit_stats> >
											(std::pair<map_location, const unit_type *>(target, up_type),
											 std::pair<battle_context::unit_stats,battle_context::unit_stats>(bc->get_attacker_stats(),
																											  bc->get_defender_stats())));
		}

		// Note we didn't fight at all if defender is already dead.
		double prob_fought = (1.0 - prob_dead_already);

		/** @todo 1.9 add combatant.prob_killed */
		double prob_killed = def.hp_dist[0] - prob_dead_already;
		prob_dead_already = def.hp_dist[0];

		double prob_died = att.hp_dist[0];
		double prob_survived = (1.0 - prob_died) * prob_fought;

		double cost = up->second.cost();
		const bool on_village = map.is_village(m->second);
		// Up to double the value of a unit based on experience
		cost += (double(up->second.experience())/double(up->second.max_experience()))*cost;
		resources_used += cost;
		avg_losses += cost * prob_died;

		// add half of cost for poisoned unit so it might get chance to heal
		avg_losses += cost * up->second.get_state(unit::STATE_POISONED) /2;

		// Double reward to emphasize getting onto villages if they survive.
		if (on_village) {
			avg_damage_taken -= game_config::poison_amount*2 * prob_survived;
		}

		terrain_quality += (double(bc->get_defender_stats().chance_to_hit)/100.0)*cost * (on_village ? 0.5 : 1.0);

		double advance_prob = 0.0;
		// The reward for advancing a unit is to get a 'negative' loss of that unit
		if (!up->second.advances_to().empty()) {
			int xp_for_advance = up->second.max_experience() - up->second.experience();
			int kill_xp, fight_xp;

			// See bug #6272... in some cases, unit already has got enough xp to advance,
			// but hasn't (bug elsewhere?).  Can cause divide by zero.
			if (xp_for_advance <= 0)
				xp_for_advance = 1;

			fight_xp = defend_it->second.level();
			kill_xp = game_config::kill_xp(fight_xp);

			if (fight_xp >= xp_for_advance) {
				advance_prob = prob_fought;
				avg_losses -= up->second.cost() * prob_fought;
			} else if (kill_xp >= xp_for_advance) {
				advance_prob = prob_killed;
				avg_losses -= up->second.cost() * prob_killed;
				// The reward for getting a unit closer to advancement
				// (if it didn't advance) is to get the proportion of
				// remaining experience needed, and multiply it by
				// a quarter of the unit cost.
				// This will cause the AI to heavily favor
				// getting xp for close-to-advance units.
				avg_losses -= up->second.cost() * 0.25 *
					fight_xp * (prob_fought - prob_killed)
					/ xp_for_advance;
			} else {
				avg_losses -= up->second.cost() * 0.25 *
					(kill_xp * prob_killed + fight_xp * (prob_fought - prob_killed))
					/ xp_for_advance;
			}

			// The reward for killing with a unit that plagues
			// is to get a 'negative' loss of that unit.
			if (bc->get_attacker_stats().plagues) {
				avg_losses -= prob_killed * up->second.cost();
			}
		}

		// If we didn't advance, we took this damage.
		avg_damage_taken += (up->second.hitpoints() - att.average_hp()) * (1.0 - advance_prob);

		/**
		 * @todo 1.9: attack_prediction.cpp should understand advancement
		 * directly.  For each level of attacker def gets 1 xp or
		 * kill_experience.
		 */
		int fight_xp = up->second.level();
		int kill_xp = game_config::kill_xp(fight_xp);
		def_avg_experience += fight_xp * (1.0 - att.hp_dist[0]) + kill_xp * att.hp_dist[0];
		if (m == movements.begin()) {
			first_chance_kill = def.hp_dist[0];
		}
	}

	if (!defend_it->second.advances_to().empty() &&
		def_avg_experience >= defend_it->second.max_experience() - defend_it->second.experience()) {
		// It's likely to advance: only if we can kill with first blow.
		chance_to_kill = first_chance_kill;
		// Negative average damage (it will advance).
		avg_damage_inflicted = defend_it->second.hitpoints() - defend_it->second.max_hitpoints();
	} else {
		chance_to_kill = prev_def->hp_dist[0];
		avg_damage_inflicted = defend_it->second.hitpoints() - prev_def->average_hp(map.gives_healing(defend_it->first));
	}

	delete prev_bc;
	terrain_quality /= resources_used;

	// Restore the units to their original positions.
	for (m = movements.begin(); m != movements.end(); ++m) {
		units.move(m->second, m->first);
	}
}

bool attack_analysis::attack_close(const map_location& loc) const
{
	std::set<map_location> &attacks = manager::get_ai_info().recent_attacks;
	for(std::set<map_location>::const_iterator i = attacks.begin(); i != attacks.end(); ++i) {
		if(distance_between(*i,loc) < 4) {
			return true;
		}
	}

	return false;
}


double attack_analysis::rating(double aggression, const readonly_context& ai_obj) const
{
	if(leader_threat) {
		aggression = 1.0;
	}

	if(uses_leader) {
		aggression = ai_obj.get_leader_aggression();
	}

	double value = chance_to_kill*target_value - avg_losses*(1.0-aggression);

	if(terrain_quality > alternative_terrain_quality) {
		// This situation looks like it might be a bad move:
		// we are moving our attackers out of their optimal terrain
		// into sub-optimal terrain.
		// Calculate the 'exposure' of our units to risk.

#ifdef SUOKKO
		//FIXME: this code was in sukko's r29531  Correct?
		const double exposure_mod = uses_leader ? ai_obj.current_team().caution()* 8.0 : ai_obj.current_team().caution() * 4.0;
		const double exposure = exposure_mod*resources_used*((terrain_quality - alternative_terrain_quality)/10)*vulnerability/std::max<double>(0.01,support);
#else
		const double exposure_mod = uses_leader ? 2.0 : ai_obj.get_caution();
		const double exposure = exposure_mod*resources_used*(terrain_quality - alternative_terrain_quality)*vulnerability/std::max<double>(0.01,support);
#endif
		LOG_AI << "attack option has base value " << value << " with exposure " << exposure << ": "
			<< vulnerability << "/" << support << " = " << (vulnerability/std::max<double>(support,0.1)) << "\n";
		value -= exposure*(1.0-aggression);
	}

	// If this attack uses our leader, and the leader can reach the keep,
	// and has gold to spend, reduce the value to reflect the leader's
	// lost recruitment opportunity in the case of an attack.
	if(uses_leader && ai_obj.leader_can_reach_keep() && ai_obj.current_team().gold() > 20) {
		value -= double(ai_obj.current_team().gold())*0.5;
	}

	// Prefer to attack already damaged targets.
	value += ((target_starting_damage/3 + avg_damage_inflicted) - (1.0-aggression)*avg_damage_taken)/10.0;

       // If the unit is surrounded and there is no support,
	   // or if the unit is surrounded and the average damage is 0,
	   // the unit skips its sanity check and tries to break free as good as possible.
       if(!is_surrounded || (support != 0 && avg_damage_taken != 0))
       {
               // Sanity check: if we're putting ourselves at major risk,
			   // and have no chance to kill, and we're not aiding our allies
			   // who are also attacking, then don't do it.
               if(vulnerability > 50.0 && vulnerability > support*2.0
			   && chance_to_kill < 0.02 && aggression < 0.75
			   && !attack_close(target)) {
                       return -1.0;
               }
        }

	if(!leader_threat && vulnerability*terrain_quality > 0.0) {
		value *= support/(vulnerability*terrain_quality);
	}

	value /= ((resources_used/2) + (resources_used/2)*terrain_quality);

	if(leader_threat) {
		value *= 5.0;
	}

	LOG_AI << "attack on " << target << ": attackers: " << movements.size()
		<< " value: " << value << " chance to kill: " << chance_to_kill
		<< " damage inflicted: " << avg_damage_inflicted
		<< " damage taken: " << avg_damage_taken
		<< " vulnerability: " << vulnerability
		<< " support: " << support
		<< " quality: " << terrain_quality
		<< " alternative quality: " << alternative_terrain_quality << "\n";

	return value;
}

/**
 * There is no real hope for us: we should try to do some damage to the enemy.
 * We can spend some cycles here, since it's rare.
 */
bool ai_default::desperate_attack(const map_location &loc)
{
	const unit &u = units_.find(loc)->second;
	LOG_AI << "desperate attack by '" << u.type_id() << "' " << loc << "\n";

	map_location adj[6];
	get_adjacent_tiles(loc, adj);

	double best_kill_prob = 0.0;
	unsigned int best_weapon = 0;
	unsigned best_dir = 0;

	for (unsigned n = 0; n != 6; ++n)
	{
		const unit *enemy = get_visible_unit(units_, adj[n], current_team());
		if (!enemy || !current_team().is_enemy(enemy->side()) || enemy->incapacitated())
			continue;
		const std::vector<attack_type> &attacks = u.attacks();
		for (unsigned i = 0; i != attacks.size(); ++i)
		{
			// Skip weapons with attack_weight=0
			if (attacks[i].attack_weight() == 0)
				continue;
			battle_context bc(units_, loc, adj[n], i);
			combatant att(bc.get_attacker_stats());
			combatant def(bc.get_defender_stats());
			att.fight(def);
			if (def.hp_dist[0] <= best_kill_prob)
				continue;
			best_kill_prob = def.hp_dist[0];
			best_weapon = i;
			best_dir = n;
		}
	}

	if (best_kill_prob > 0.0) {
		attack_result_ptr attack_res = execute_attack_action(loc, adj[best_dir], best_weapon);
		if (!attack_res->is_ok()) {
			ERR_AI << "desperate attack failed"<<std::endl;
		}
		return attack_res->is_gamestate_changed();
	}

	double least_hp = u.hitpoints() + 1;

	// Who would do most damage to us when they attack?  (approximate: may be different ToD)
	for (unsigned n = 0; n != 6; ++n)
	{
		const unit *enemy = get_visible_unit(units_, adj[n], current_team());
		if (!enemy || !current_team().is_enemy(enemy->side()) || enemy->incapacitated())
			continue;
		const std::vector<attack_type> &attacks = enemy->attacks();
		for (unsigned i = 0; i != attacks.size(); ++i)
		{
			// SKip weapons with attack_weight=0
			if (attacks[i].attack_weight() == 0)
				continue;
			battle_context bc(units_, adj[n], loc, i);
			combatant att(bc.get_attacker_stats());
			combatant def(bc.get_defender_stats());
			att.fight(def);
			if (def.average_hp() < least_hp) {
				least_hp = def.average_hp();
				best_dir = n;
			}
		}
	}

	// It is possible that there were no adjacent units to attack...
	if (least_hp != u.hitpoints() + 1) {
		battle_context bc(units_, loc, adj[best_dir], -1, -1, 0.5);
		attack_result_ptr attack_res = execute_attack_action(loc, adj[best_dir], bc.get_attacker_stats().attack_num);
		if (!attack_res->is_ok()) {
			ERR_AI << "desperate attack failed" << std::endl;
		}
		return attack_res->is_gamestate_changed();
	}
	return false;
}

} //end of namespace ai
