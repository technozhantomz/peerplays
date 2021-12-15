/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "database_fixture.hpp"

#include <graphene/chain/sport_object.hpp>
#include <graphene/chain/event_object.hpp>
#include <graphene/chain/event_group_object.hpp>
#include <graphene/chain/betting_market_object.hpp>

using namespace graphene::chain;

#define CREATE_ICE_HOCKEY_BETTING_MARKET(never_in_play, delay_before_settling) \
  create_sport({{"en", "Ice Hockey"}, {"zh_Hans", "冰球"}, {"ja", "アイスホッケー"}}); \
  generate_blocks(1); \
  const sport_id_type ice_hockey_id = (*db.get_index_type<sport_object_index>().indices().get<by_id>().rbegin()).id; \
  create_event_group({{"en", "NHL"}, {"zh_Hans", "國家冰球聯盟"}, {"ja", "ナショナルホッケーリーグ"}}, ice_hockey_id); \
  generate_blocks(1); \
  const event_group_id_type nhl_id = (*db.get_index_type<event_group_object_index>().indices().get<by_id>().rbegin()).id; \
  create_event({{"en", "Washington Capitals/Chicago Blackhawks"}, {"zh_Hans", "華盛頓首都隊/芝加哥黑鷹"}, {"ja", "ワシントン・キャピタルズ/シカゴ・ブラックホークス"}}, {{"en", "2016-17"}}, nhl_id); \
  generate_blocks(1); \
  const event_id_type capitals_vs_blackhawks_id = (*db.get_index_type<event_object_index>().indices().get<by_id>().rbegin()).id; \
  create_betting_market_rules({{"en", "NHL Rules v1.0"}}, {{"en", "The winner will be the team with the most points at the end of the game.  The team with fewer points will not be the winner."}}); \
  generate_blocks(1); \
  const betting_market_rules_id_type betting_market_rules_id = (*db.get_index_type<betting_market_rules_object_index>().indices().get<by_id>().rbegin()).id; \
  create_betting_market_group({{"en", "Moneyline"}}, capitals_vs_blackhawks_id, betting_market_rules_id, asset_id_type(), never_in_play, delay_before_settling); \
  generate_blocks(1); \
  const betting_market_group_id_type moneyline_betting_markets_id = (*db.get_index_type<betting_market_group_object_index>().indices().get<by_id>().rbegin()).id; \
  create_betting_market(moneyline_betting_markets_id, {{"en", "Washington Capitals win"}}); \
  generate_blocks(1); \
  const betting_market_id_type capitals_win_market_id = (*db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin()).id; \
  create_betting_market(moneyline_betting_markets_id, {{"en", "Chicago Blackhawks win"}}); \
  generate_blocks(1); \
  const betting_market_id_type& blackhawks_win_market_id = (*db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin()).id; \
  (void)capitals_win_market_id; (void)blackhawks_win_market_id;

// create the basic betting market, plus groups for the first, second, and third period results
#define CREATE_EXTENDED_ICE_HOCKEY_BETTING_MARKET(never_in_play, delay_before_settling) \
  CREATE_ICE_HOCKEY_BETTING_MARKET(never_in_play, delay_before_settling) \
  create_betting_market_group({{"en", "First Period Result"}}, capitals_vs_blackhawks_id, betting_market_rules_id, asset_id_type(), never_in_play, delay_before_settling); \
  generate_blocks(1); \
  const betting_market_group_id_type first_period_result_betting_markets_id = (*db.get_index_type<betting_market_group_object_index>().indices().get<by_id>().rbegin()).id; \
  create_betting_market(first_period_result_betting_markets_id, {{"en", "Washington Capitals win"}}); \
  generate_blocks(1); \
  const betting_market_id_type first_period_capitals_win_market_id = (*db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin()).id; \
  create_betting_market(first_period_result_betting_markets_id, {{"en", "Chicago Blackhawks win"}}); \
  generate_blocks(1); \
  const betting_market_id_type first_period_blackhawks_win_market_id = (*db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin()).id; \
  (void)first_period_capitals_win_market_id; (void)first_period_blackhawks_win_market_id; \
  \
  create_betting_market_group({{"en", "Second Period Result"}}, capitals_vs_blackhawks_id, betting_market_rules_id, asset_id_type(), never_in_play, delay_before_settling); \
  generate_blocks(1); \
  const betting_market_group_id_type second_period_result_betting_markets_id = (*db.get_index_type<betting_market_group_object_index>().indices().get<by_id>().rbegin()).id; \
  create_betting_market(second_period_result_betting_markets_id, {{"en", "Washington Capitals win"}}); \
  generate_blocks(1); \
  const betting_market_id_type second_period_capitals_win_market_id = (*db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin()).id; \
  create_betting_market(second_period_result_betting_markets_id, {{"en", "Chicago Blackhawks win"}}); \
  generate_blocks(1); \
  const betting_market_id_type second_period_blackhawks_win_market_id = (*db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin()).id; \
  (void)second_period_capitals_win_market_id; (void)second_period_blackhawks_win_market_id; \
  \
  create_betting_market_group({{"en", "Third Period Result"}}, capitals_vs_blackhawks_id, betting_market_rules_id, asset_id_type(), never_in_play, delay_before_settling); \
  generate_blocks(1); \
  const betting_market_group_id_type third_period_result_betting_markets_id = (*db.get_index_type<betting_market_group_object_index>().indices().get<by_id>().rbegin()).id; \
  create_betting_market(third_period_result_betting_markets_id, {{"en", "Washington Capitals win"}}); \
  generate_blocks(1); \
  const betting_market_id_type third_period_capitals_win_market_id = (*db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin()).id; \
  create_betting_market(third_period_result_betting_markets_id, {{"en", "Chicago Blackhawks win"}}); \
  generate_blocks(1); \
  const betting_market_id_type third_period_blackhawks_win_market_id = (*db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin()).id; \
  (void)third_period_capitals_win_market_id; (void)third_period_blackhawks_win_market_id;

#define CREATE_TENNIS_BETTING_MARKET() \
  create_betting_market_rules({{"en", "Tennis Rules v1.0"}}, {{"en", "The winner is the player who wins the last ball in the match."}}); \
  generate_blocks(1); \
  const betting_market_rules_id_type tennis_rules_id = (*db.get_index_type<betting_market_rules_object_index>().indices().get<by_id>().rbegin()).id; \
  create_sport({{"en", "Tennis"}}); \
  generate_blocks(1); \
  const sport_id_type tennis_id = (*db.get_index_type<sport_object_index>().indices().get<by_id>().rbegin()).id; \
  create_event_group({{"en", "Wimbledon"}}, tennis_id); \
  generate_blocks(1); \
  const event_group_id_type wimbledon_id = (*db.get_index_type<event_group_object_index>().indices().get<by_id>().rbegin()).id; \
  create_event({{"en", "R. Federer/T. Berdych"}}, {{"en", "2017"}}, wimbledon_id); \
  generate_blocks(1); \
  const event_id_type berdych_vs_federer_id = (*db.get_index_type<event_object_index>().indices().get<by_id>().rbegin()).id; \
  create_event({{"en", "M. Cilic/S. Querrye"}}, {{"en", "2017"}}, wimbledon_id); \
  generate_blocks(1); \
  const event_id_type& cilic_vs_querrey_id = (*db.get_index_type<event_object_index>().indices().get<by_id>().rbegin()).id; \
  create_betting_market_group({{"en", "Moneyline 1st sf"}}, berdych_vs_federer_id, tennis_rules_id, asset_id_type(), false, 0); \
  generate_blocks(1); \
  const betting_market_group_id_type moneyline_berdych_vs_federer_id = (*db.get_index_type<betting_market_group_object_index>().indices().get<by_id>().rbegin()).id; \
  create_betting_market_group({{"en", "Moneyline 2nd sf"}}, cilic_vs_querrey_id, tennis_rules_id, asset_id_type(), false, 0); \
  generate_blocks(1); \
  const betting_market_group_id_type moneyline_cilic_vs_querrey_id = (*db.get_index_type<betting_market_group_object_index>().indices().get<by_id>().rbegin()).id; \
  create_betting_market(moneyline_berdych_vs_federer_id, {{"en", "T. Berdych defeats R. Federer"}}); \
  generate_blocks(1); \
  const betting_market_id_type berdych_wins_market_id = (*db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin()).id; \
  create_betting_market(moneyline_berdych_vs_federer_id, {{"en", "R. Federer defeats T. Berdych"}}); \
  generate_blocks(1); \
  const betting_market_id_type federer_wins_market_id = (*db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin()).id; \
  create_betting_market(moneyline_cilic_vs_querrey_id, {{"en", "M. Cilic defeats S. Querrey"}}); \
  generate_blocks(1); \
  const betting_market_id_type cilic_wins_market_id = (*db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin()).id; \
  create_betting_market(moneyline_cilic_vs_querrey_id, {{"en", "S. Querrey defeats M. Cilic"}});\
  generate_blocks(1); \
  const betting_market_id_type querrey_wins_market_id = (*db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin()).id; \
  create_event({{"en", "R. Federer/M. Cilic"}}, {{"en", "2017"}}, wimbledon_id); \
  generate_blocks(1); \
  const event_id_type& cilic_vs_federer_id = (*db.get_index_type<event_object_index>().indices().get<by_id>().rbegin()).id; \
  create_betting_market_group({{"en", "Moneyline final"}}, cilic_vs_federer_id, tennis_rules_id, asset_id_type(), false, 0); \
  generate_blocks(1); \
  const betting_market_group_id_type moneyline_cilic_vs_federer_id = (*db.get_index_type<betting_market_group_object_index>().indices().get<by_id>().rbegin()).id; \
  create_betting_market(moneyline_cilic_vs_federer_id, {{"en", "R. Federer defeats M. Cilic"}}); \
  generate_blocks(1); \
  const betting_market_id_type federer_wins_final_market_id = (*db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin()).id; \
  create_betting_market(moneyline_cilic_vs_federer_id, {{"en", "M. Cilic defeats R. Federer"}}); \
  generate_blocks(1); \
  const betting_market_id_type cilic_wins_final_market_id = (*db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin()).id; \
  (void)federer_wins_market_id;(void)cilic_wins_market_id;(void)federer_wins_final_market_id; (void)cilic_wins_final_market_id; (void)berdych_wins_market_id; (void)querrey_wins_market_id;

// set up a fixture that places a series of two matched bets, we'll use this fixture to verify
// the result in all three possible outcomes
struct simple_bet_test_fixture : database_fixture {
   betting_market_id_type capitals_win_betting_market_id;
   betting_market_id_type blackhawks_win_betting_market_id;
   betting_market_group_id_type moneyline_betting_markets_id;

   simple_bet_test_fixture()
   {
      ACTORS( (alice)(bob) );
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

      // give alice and bob 10k each
      transfer(account_id_type(), alice_id, asset(10000));
      transfer(account_id_type(), bob_id, asset(10000));

      // place bets at 10:1
      place_bet(alice_id, capitals_win_market_id, bet_type::back, asset(100, asset_id_type()), 11 * GRAPHENE_BETTING_ODDS_PRECISION);
      place_bet(bob_id, capitals_win_market_id, bet_type::lay, asset(1000, asset_id_type()), 11 * GRAPHENE_BETTING_ODDS_PRECISION);

      // reverse positions at 1:1
      place_bet(alice_id, capitals_win_market_id, bet_type::lay, asset(1100, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);
      place_bet(bob_id, capitals_win_market_id, bet_type::back, asset(1100, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

      capitals_win_betting_market_id = capitals_win_market_id;
      blackhawks_win_betting_market_id = blackhawks_win_market_id;

      // close betting to prepare for the next operation which will be grading or cancel
      update_betting_market_group(moneyline_betting_markets_id, graphene::chain::keywords::_status = betting_market_group_status::closed);
      generate_blocks(1);
   }
};
