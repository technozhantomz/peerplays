#pragma once

#include <graphene/chain/protocol/vote.hpp>

namespace graphene { namespace chain {

   /**
       * @class votes_info_object
       * @ingroup object
    */
   struct votes_info_object {
      vote_id_type vote_id;
      object_id_type id;
   };

   /**
       * @class votes_info
       * @brief tracks information about a votes info
       * @ingroup object
    */
   struct votes_info {
      optional< vector< votes_info_object > > votes_for_committee_members;
      optional< vector< votes_info_object > > votes_for_witnesses;
      optional< vector< votes_info_object > > votes_for_workers;
      optional< vector< votes_info_object > > votes_against_workers;
      optional< vector< votes_info_object > > votes_for_sons;
   };

} } // graphene::chain

FC_REFLECT( graphene::chain::votes_info_object,
   (vote_id)
   (id) )

FC_REFLECT( graphene::chain::votes_info,
   (votes_for_committee_members)
   (votes_for_witnesses)
   (votes_for_workers)
   (votes_against_workers)
   (votes_for_sons) )