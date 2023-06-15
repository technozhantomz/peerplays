#pragma once

#include <graphene/chain/protocol/vote.hpp>

namespace graphene { namespace chain {

   /**
       * @class voters_info_object
       * @ingroup object
    */
   struct voters_info_object {
      vote_id_type vote_id;
      vector<account_id_type> voters;
   };

   /**
       * @class voters_info
       * @brief tracks information about a voters info
       * @ingroup object
    */
   struct voters_info {
      optional<voters_info_object>           voters_for_committee_member;
      optional<voters_info_object>           voters_for_witness;
      optional<vector<voters_info_object> >  voters_for_workers;
      optional<vector<voters_info_object> >  voters_against_workers;
      optional<voters_info_object>           voters_for_son;
   };

} } // graphene::chain

FC_REFLECT( graphene::chain::voters_info_object,
   (vote_id)
   (voters) )

FC_REFLECT( graphene::chain::voters_info,
   (voters_for_committee_member)
   (voters_for_witness)
   (voters_for_workers)
   (voters_against_workers)
   (voters_for_son) )