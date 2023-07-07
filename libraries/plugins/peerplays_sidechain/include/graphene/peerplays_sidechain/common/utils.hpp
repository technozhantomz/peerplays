#pragma once

#include <graphene/chain/protocol/asset.hpp>

namespace graphene { namespace peerplays_sidechain {

std::string base64_encode(const std::string &s);
std::string base64_decode(const std::string &s);

std::string object_id_to_string(graphene::chain::object_id_type id);
graphene::chain::object_id_type string_to_object_id(const std::string &id);

}} // namespace graphene::peerplays_sidechain
