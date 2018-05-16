/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <chain/net_v2/types.hpp>

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

   struct transaction_cache_object {
      transaction_id_type   id;
      fc::time_point        expiration;
      packed_transacton_ptr trx;
      bytes_ptr             raw;
      dynamic_bitset        session_acks;

      bytes_ptr get_raw() {
         if (!raw) {
            // this is also potentially wasteful if this was a block from the net code...
            raw = std::make_shared<bytes>();
            auto size = fc::raw::pack_size(*trx);
            raw->resize(size);
            fc::datastream<char*> ds(raw->data(), raw->size());
            fc::raw::pack(ds, *trx);
         }

         return raw;
      }
   };

   struct by_id;
   struct by_expiration;
   using transaction_cache = multi_index_container<
      transaction_cache_object,
      indexed_by<
         hashed_unique<
            tag< by_id >,
            member < transaction_cache_object, transaction_id_type, &transaction_cache_object::id >,
            std::hash<transaction_id_type>
         >,
         ordered_non_unique<
            tag< by_expiration >,
            member< transaction_cache_object, fc::time_point, &transaction_cache_object::expiration >
         >
      >
   >;

} } // eosio::net_v2