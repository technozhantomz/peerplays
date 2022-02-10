#pragma once

#include <graphene/chain/protocol/vote.hpp>

namespace graphene { namespace chain {

   /**
       * @class votes_info_object
       * @tparam IdType id type of the object
       * @ingroup object
    */
   template<typename IdType>
   struct votes_info_object {
      votes_info_object() = default;
      votes_info_object(const vote_id_type& vote_id_, uint64_t id_)
        : vote_id{vote_id_}
        , id{id_}
      {}

      vote_id_type vote_id;
      IdType id;
   };

   /**
       * @class votes_info
       * @brief tracks information about a votes info
       * @ingroup object
    */
   struct votes_info {
      optional< vector< votes_info_object<committee_member_id_type> > > votes_for_committee_members;
      optional< vector< votes_info_object<witness_id_type> > > votes_for_witnesses;
      optional< vector< votes_info_object<worker_id_type> > > votes_for_workers;
      optional< vector< votes_info_object<worker_id_type> > > votes_against_workers;
      optional< vector< votes_info_object<son_id_type> > > votes_for_sons;
   };

} } // graphene::chain

FC_REFLECT_TEMPLATE( (typename IdType), graphene::chain::votes_info_object<IdType>,
   (vote_id)
   (id) )

FC_REFLECT( graphene::chain::votes_info,
   (votes_for_committee_members)
   (votes_for_witnesses)
   (votes_for_workers)
   (votes_against_workers)
   (votes_for_sons) )