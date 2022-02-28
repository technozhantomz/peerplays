/*
 * Copyright (c) 2018 Peerplays Blockchain Standards Association, and contributors.
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
#pragma once
#include <graphene/chain/database.hpp>
#include <graphene/chain/betting_market_object.hpp>
#include <graphene/chain/event_object.hpp>

namespace graphene { namespace bookie {
using namespace chain;

namespace detail
{

/**
    *  @brief This secondary index will allow a reverse lookup of all events that happened
 */
class persistent_event_index : public secondary_index
{
public:
   virtual void object_inserted( const object& obj ) override;
   virtual void object_modified( const object& after  ) override;

   map< event_id_type, event_object > ephemeral_event_object;
};

#if 0 // we no longer have competitors, just leaving this here as an example of how to do a secondary index
class events_by_competitor_index : public secondary_index
{
   public:
      virtual ~events_by_competitor_index() {}

      virtual void object_inserted( const object& obj ) override;
      virtual void object_removed( const object& obj ) override;
      virtual void about_to_modify( const object& before ) override;
      virtual void object_modified( const object& after  ) override;
   protected:

      map<competitor_id_type, set<persistent_event_id_type> > competitor_to_events;
};

void events_by_competitor_index::object_inserted( const object& obj ) 
{
   const persistent_event_object& event_obj = *boost::polymorphic_downcast<const persistent_event_object*>(&obj);
   for (const competitor_id_type& competitor_id : event_obj.competitors)
      competitor_to_events[competitor_id].insert(event_obj.id);
   for (const competitor_id_type& competitor_id : event_obj.competitors)
      competitor_to_events[competitor_id].insert(event_obj.id);
}
void events_by_competitor_index::object_removed( const object& obj ) 
{
   const persistent_event_object& event_obj = *boost::polymorphic_downcast<const persistent_event_object*>(&obj);
   for (const competitor_id_type& competitor_id : event_obj.competitors)
      competitor_to_events[competitor_id].erase(event_obj.id);
}
void events_by_competitor_index::about_to_modify( const object& before ) 
{
   object_removed(before);
}
void events_by_competitor_index::object_modified( const object& after ) 
{
   object_inserted(after);
}
#endif

/**
    *  @brief This secondary index will allow a reverse lookup of all betting_market_group that happened
 */
class persistent_betting_market_group_index : public secondary_index
{
public:
   struct internal_type
   {
      internal_type() = default;

      internal_type(const betting_market_group_object& other)
            : ephemeral_betting_market_group_object{other}
      {}

      internal_type& operator=(const betting_market_group_object& other)
      {
         ephemeral_betting_market_group_object = other;
         return *this;
      }

      friend bool operator==(const internal_type& lhs, const internal_type& rhs);
      friend bool operator<(const internal_type& lhs, const internal_type& rhs);
      friend bool operator>(const internal_type& lhs, const internal_type& rhs);

      betting_market_group_object ephemeral_betting_market_group_object;
      share_type total_matched_bets_amount;
   };

public:
   virtual void object_inserted( const object& obj ) override;
   virtual void object_modified( const object& after  ) override;

   map< betting_market_group_id_type, internal_type > internal;
};

inline bool operator==(const persistent_betting_market_group_index::internal_type& lhs, const persistent_betting_market_group_index::internal_type& rhs)
{
   return lhs.ephemeral_betting_market_group_object == rhs.ephemeral_betting_market_group_object;
}

inline bool operator<(const persistent_betting_market_group_index::internal_type& lhs, const persistent_betting_market_group_index::internal_type& rhs)
{
   return lhs.ephemeral_betting_market_group_object < rhs.ephemeral_betting_market_group_object;
}

inline bool operator>(const persistent_betting_market_group_index::internal_type& lhs, const persistent_betting_market_group_index::internal_type& rhs)
{
   return !operator<(lhs, rhs);
}

/**
    *  @brief This secondary index will allow a reverse lookup of all betting_market_object that happened
 */
class persistent_betting_market_index : public secondary_index
{
public:
   virtual void object_inserted( const object& obj ) override;
   virtual void object_modified( const object& after  ) override;

   map< betting_market_id_type, betting_market_object > ephemeral_betting_market_object;
};

/**
    *  @brief This secondary index will allow a reverse lookup of all bet_object that happened
 */
class persistent_bet_index : public secondary_index
{
public:
   struct internal_type
   {
      internal_type() = default;

      internal_type(const bet_object& other)
            : ephemeral_bet_object{other}
      {}

      internal_type& operator=(const bet_object& other)
      {
         ephemeral_bet_object = other;
         return *this;
      }

      account_id_type get_bettor_id() const { return ephemeral_bet_object.bettor_id; }
      bool is_matched() const { return amount_matched != share_type(); }

      friend bool operator==(const internal_type& lhs, const internal_type& rhs);
      friend bool operator<(const internal_type& lhs, const internal_type& rhs);
      friend bool operator>(const internal_type& lhs, const internal_type& rhs);

      bet_object ephemeral_bet_object;
      // total amount of the bet that matched
      share_type amount_matched;
      std::vector<operation_history_id_type> associated_operations;
   };

public:
   virtual void object_inserted( const object& obj ) override;
   virtual void object_modified( const object& after  ) override;

   map< bet_id_type, internal_type > internal;
};

inline bool operator==(const persistent_bet_index::internal_type& lhs, const persistent_bet_index::internal_type& rhs)
{
   return lhs.ephemeral_bet_object == rhs.ephemeral_bet_object;
}

inline bool operator<(const persistent_bet_index::internal_type& lhs, const persistent_bet_index::internal_type& rhs)
{
   return lhs.ephemeral_bet_object < rhs.ephemeral_bet_object;
}

inline bool operator>(const persistent_bet_index::internal_type& lhs, const persistent_bet_index::internal_type& rhs)
{
   return !operator<(lhs, rhs);
}

} } } //graphene::bookie::detail

