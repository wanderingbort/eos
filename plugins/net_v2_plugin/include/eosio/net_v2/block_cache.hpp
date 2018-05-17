/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosio/net_v2/types.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>

namespace bmi = boost::multi_index;
using boost::multi_index_container;
using bmi::indexed_by;
using bmi::ordered_non_unique;
using bmi::hashed_unique;
using bmi::member;
using bmi::tag;

namespace eosio { namespace net_v2 {

   struct block_cache_object {
      block_id_type    id;
      block_id_type    prev;
      signed_block_ptr blk;
      bytes_ptr        raw;
      dynamic_bitset   session_acks;

      bytes_ptr get_raw() {
         if (!raw) {
            // this is also potentially wasteful if this was a block from the net code...
            raw = std::make_shared<bytes>();
            auto size = fc::raw::pack_size(*blk);
            raw->resize(size);
            fc::datastream<char*> ds(raw->data(), raw->size());
            fc::raw::pack(ds, *blk);
         }

         return raw;
      }
   };

   using block_cache = multi_index_container<
      block_cache_object,
      indexed_by<
         hashed_unique<
            tag<by_id>,
            member< block_cache_object, block_id_type, &block_cache_object::id>,
            std::hash<block_id_type>
         >
      >
   >;

} } // namespace eosio::net_v2