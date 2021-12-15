#include <graphene/chain/database.hpp>
#include <graphene/chain/nft_object.hpp>

namespace graphene
{
    namespace chain
    {
        time_point_sec nft_metadata_object::get_lottery_expiration() const
        {
            if (lottery_data)
                return lottery_data->lottery_options.end_date;
            return time_point_sec();
        }

        asset nft_metadata_object::get_lottery_jackpot(const database &db) const
        {
            if (lottery_data)
                return lottery_data->lottery_balance_id(db).jackpot;
            return asset();
        }

        share_type nft_metadata_object::get_token_current_supply(const database &db) const
        {
            share_type current_supply;
            const auto &idx_lottery_by_md = db.get_index_type<nft_index>().indices().get<by_metadata>();
            auto lottery_range = idx_lottery_by_md.equal_range(id);
            current_supply = std::distance(lottery_range.first, lottery_range.second);
            return current_supply;
        }

        vector<account_id_type> nft_metadata_object::get_holders(const database &db) const
        {
            const auto &idx_lottery_by_md = db.get_index_type<nft_index>().indices().get<by_metadata>();
            auto lottery_range = idx_lottery_by_md.equal_range(id);
            vector<account_id_type> holders;
            holders.reserve(std::distance(lottery_range.first, lottery_range.second));
            std::for_each(lottery_range.first, lottery_range.second,
                          [&](const nft_object &ticket) {
                              holders.emplace_back(ticket.owner);
                          });
            return holders;
        }

        vector<uint64_t> nft_metadata_object::get_ticket_ids(const database &db) const
        {
            const auto &idx_lottery_by_md = db.get_index_type<nft_index>().indices().get<by_metadata>();
            auto lottery_range = idx_lottery_by_md.equal_range(id);
            vector<uint64_t> tickets;
            tickets.reserve(std::distance(lottery_range.first, lottery_range.second));
            std::for_each(lottery_range.first, lottery_range.second,
                          [&](const nft_object &ticket) {
                              tickets.emplace_back(ticket.id.instance());
                          });
            return tickets;
        }

        void nft_metadata_object::distribute_benefactors_part(database &db)
        {
            transaction_evaluation_state eval(&db);
            const auto &lottery_options = lottery_data->lottery_options;
            share_type jackpot = lottery_options.ticket_price.amount * get_token_current_supply(db) + lottery_data->lottery_balance_id(db).total_progressive_jackpot.amount;

            for (auto benefactor : lottery_options.benefactors)
            {
                nft_lottery_reward_operation reward_op;
                reward_op.lottery_id = id;
                reward_op.winner = benefactor.id;
                reward_op.is_benefactor_reward = true;
                reward_op.win_percentage = benefactor.share;
                reward_op.amount = asset(jackpot.value * benefactor.share / GRAPHENE_100_PERCENT, lottery_options.ticket_price.asset_id);
                db.apply_operation(eval, reward_op);
            }
        }

        map<account_id_type, vector<uint16_t>> nft_metadata_object::distribute_winners_part(database &db)
        {
            transaction_evaluation_state eval(&db);
            auto current_supply = get_token_current_supply(db);
            auto &lottery_options = lottery_data->lottery_options;

            auto holders = get_holders(db);
            vector<uint64_t> ticket_ids = get_ticket_ids(db);
            FC_ASSERT(current_supply.value == (int64_t)holders.size());
            FC_ASSERT(get_lottery_jackpot(db).amount.value == current_supply.value * lottery_options.ticket_price.amount.value);
            map<account_id_type, vector<uint16_t>> structurized_participants;
            for (account_id_type holder : holders)
            {
                if (!structurized_participants.count(holder))
                    structurized_participants.emplace(holder, vector<uint16_t>());
            }
            uint64_t jackpot = get_lottery_jackpot(db).amount.value;
            auto selections = lottery_options.winning_tickets.size() <= holders.size() ? lottery_options.winning_tickets.size() : holders.size();
            auto winner_numbers = db.get_random_numbers(0, holders.size(), selections, false);

            auto &tickets(lottery_options.winning_tickets);

            if (holders.size() < tickets.size())
            {
                uint16_t percents_to_distribute = 0;
                for (auto i = tickets.begin() + holders.size(); i != tickets.end();)
                {
                    percents_to_distribute += *i;
                    i = tickets.erase(i);
                }
                for (auto t = tickets.begin(); t != tickets.begin() + holders.size(); ++t)
                    *t += percents_to_distribute / holders.size();
            }
            auto sweeps_distribution_percentage = db.get_global_properties().parameters.sweeps_distribution_percentage();
            for (size_t c = 0; c < winner_numbers.size(); ++c)
            {
                auto winner_num = winner_numbers[c];
                nft_lottery_reward_operation reward_op;
                reward_op.lottery_id = id;
                reward_op.is_benefactor_reward = false;
                reward_op.winner = holders[winner_num];
                if (ticket_ids.size() > winner_num)
                {
                    reward_op.winner_ticket_id = ticket_ids[winner_num];
                }
                reward_op.win_percentage = tickets[c];
                reward_op.amount = asset(jackpot * tickets[c] * (1. - sweeps_distribution_percentage / (double)GRAPHENE_100_PERCENT) / GRAPHENE_100_PERCENT, lottery_options.ticket_price.asset_id);
                db.apply_operation(eval, reward_op);

                structurized_participants[holders[winner_num]].push_back(tickets[c]);
            }
            return structurized_participants;
        }

        void nft_metadata_object::distribute_sweeps_holders_part(database &db)
        {
            transaction_evaluation_state eval(&db);
            auto &asset_bal_idx = db.get_index_type<account_balance_index>().indices().get<by_asset_balance>();
            auto sweeps_params = db.get_global_properties().parameters;
            uint64_t distribution_asset_supply = sweeps_params.sweeps_distribution_asset()(db).dynamic_data(db).current_supply.value;
            const auto range = asset_bal_idx.equal_range(boost::make_tuple(sweeps_params.sweeps_distribution_asset()));
            asset remaining_jackpot = get_lottery_jackpot(db);
            uint64_t holders_sum = 0;
            for (const account_balance_object &holder_balance : boost::make_iterator_range(range.first, range.second))
            {
                int64_t holder_part = remaining_jackpot.amount.value / (double)distribution_asset_supply * holder_balance.balance.value * SWEEPS_VESTING_BALANCE_MULTIPLIER;
                db.adjust_sweeps_vesting_balance(holder_balance.owner, holder_part);
                holders_sum += holder_part;
            }
            uint64_t balance_rest = remaining_jackpot.amount.value * SWEEPS_VESTING_BALANCE_MULTIPLIER - holders_sum;
            db.adjust_sweeps_vesting_balance(sweeps_params.sweeps_vesting_accumulator_account(), balance_rest);
            db.modify(lottery_data->lottery_balance_id(db), [&](nft_lottery_balance_object &obj) {
                obj.jackpot -= remaining_jackpot;
            });
        }

        void nft_metadata_object::end_lottery(database &db)
        {
            transaction_evaluation_state eval(&db);
            const auto &lottery_options = lottery_data->lottery_options;

            FC_ASSERT(is_lottery());
            FC_ASSERT(lottery_options.is_active && (lottery_options.end_date <= db.head_block_time() || lottery_options.ending_on_soldout));

            auto participants = distribute_winners_part(db);
            if (participants.size() > 0)
            {
                distribute_benefactors_part(db);
                distribute_sweeps_holders_part(db);
            }

            nft_lottery_end_operation end_op;
            end_op.lottery_id = get_id();
            db.apply_operation(eval, end_op);
        }
    } // namespace chain
} // namespace graphene
