#pragma once
#include <graphene/chain/protocol/operations.hpp>
#include <graphene/db/generic_index.hpp>

namespace graphene
{
    namespace chain
    {
        class database;

        struct by_expiration_date
        {
        };
        class offer_object : public graphene::db::abstract_object<offer_object>
        {
        public:
            static const uint8_t space_id = protocol_ids;
            static const uint8_t type_id = offer_object_type;

            account_id_type issuer;

            set<nft_id_type> item_ids;
            optional<account_id_type> bidder;
            optional<asset> bid_price;
            asset minimum_price;
            asset maximum_price;

            bool buying_item;
            fc::time_point_sec offer_expiration_date;

            offer_id_type get_id() const { return id; }
        };

        class offer_history_object
            : public graphene::db::abstract_object<offer_history_object>
        {
        public:
            static const uint8_t space_id = implementation_ids;
            static const uint8_t type_id = impl_offer_history_object_type;

            account_id_type issuer;

            set<nft_id_type> item_ids;
            optional<account_id_type> bidder;
            optional<asset> bid_price;
            asset minimum_price;
            asset maximum_price;

            bool buying_item;
            fc::time_point_sec offer_expiration_date;
            result_type result;

            offer_history_id_type get_id() const { return id; }
        };

        class offer_item_index : public secondary_index
        {
        public:
            virtual void object_inserted(const object &obj) override;
            virtual void object_removed(const object &obj) override;
            virtual void about_to_modify(const object &before) override{};
            virtual void object_modified(const object &after) override;

            set<nft_id_type> _locked_items;
        };

        struct compare_by_expiration_date
        {
            bool operator()(const fc::time_point_sec &o1,
                            const fc::time_point_sec &o2) const
            {
                return o1 < o2;
            }
        };

        using offer_multi_index_type = multi_index_container<
            offer_object,
            indexed_by<
                ordered_unique<tag<by_id>, member<object, object_id_type, &object::id>>,
                ordered_non_unique<tag<by_expiration_date>,
                                   member<offer_object, fc::time_point_sec,
                                          &offer_object::offer_expiration_date>,
                                   compare_by_expiration_date>>>;

        using offer_history_multi_index_type = multi_index_container<
            offer_history_object,
            indexed_by<
                ordered_unique<tag<by_id>, member<object, object_id_type, &object::id>>,
                ordered_non_unique<tag<by_expiration_date>,
                                   member<offer_history_object, fc::time_point_sec,
                                          &offer_history_object::offer_expiration_date>,
                                   compare_by_expiration_date>>>;

        using offer_index = generic_index<offer_object, offer_multi_index_type>;

        using offer_history_index =
            generic_index<offer_history_object, offer_history_multi_index_type>;

    } // namespace chain
} // namespace graphene

FC_REFLECT_DERIVED(graphene::chain::offer_object, (graphene::db::object),
                   (issuer)(item_ids)(bidder)(bid_price)(minimum_price)(
                       maximum_price)(buying_item)(offer_expiration_date))

FC_REFLECT_DERIVED(graphene::chain::offer_history_object,
                   (graphene::db::object),
                   (issuer)(item_ids)(bidder)(bid_price)(minimum_price)(
                       maximum_price)(buying_item)(offer_expiration_date)(result))
