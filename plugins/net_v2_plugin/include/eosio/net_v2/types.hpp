/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosio/chain/block.hpp>
#include <eosio/chain/types.hpp>

using namespace eosio::chain;

namespace eosio { namespace net_v2 {

using packed_transacton_ptr = std::shared_ptr<packed_transaction>;
using signed_block_ptr = std::shared_ptr<signed_block>;
using bytes_ptr = std::shared_ptr<bytes>;
using dynamic_bitset = std::vector<bool>;

} }