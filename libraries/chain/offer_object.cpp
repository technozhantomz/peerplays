#include <graphene/chain/database.hpp>
#include <graphene/chain/offer_object.hpp>

namespace graphene
{
    namespace chain
    {

        void offer_item_index::object_inserted(const object &obj)
        {
            assert(dynamic_cast<const offer_object *>(&obj));
            const offer_object &oo = static_cast<const offer_object &>(obj);
            if (!oo.buying_item)
            {
                for (const auto &id : oo.item_ids)
                {
                    _locked_items.emplace(id);
                }
            }
        }

        void offer_item_index::object_modified(const object &after)
        {
            assert(dynamic_cast<const offer_object *>(&after));
            const offer_object &oo = static_cast<const offer_object &>(after);
            if (oo.buying_item && oo.bidder)
            {
                for (const auto &id : oo.item_ids)
                {
                    _locked_items.emplace(id);
                }
            }
        }

        void offer_item_index::object_removed(const object &obj)
        {
            assert(dynamic_cast<const offer_object *>(&obj));
            const offer_object &oo = static_cast<const offer_object &>(obj);

            if (!oo.buying_item || oo.bidder)
            {
                for (const auto &id : oo.item_ids)
                {
                    _locked_items.erase(id);
                }
            }
        }

    } // namespace chain
} // namespace graphene