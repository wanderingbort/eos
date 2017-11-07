#pragma once
#include <fc/static_variant.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/reflect/variant.hpp>
#include <eosio/blockchain/internal/meta_utils.hpp>

namespace eosio { namespace blockchain {
   namespace config {
      static const char*    public_key_default_prefix = "EOS";
   };

   class public_key
   {
      public:
         using storage_type = fc::static_variant<fc::ecc::public_key, fc::sha256>;

         template<typename T>
         struct _storage_types;

         template<typename ...Ts>
         struct _storage_types<fc::static_variant<Ts...>> {
            template<typename T>
            static constexpr bool include = internal::meta_array_contains_v<T, Ts...>;
         };
         using storage_types = _storage_types<storage_type>;

         /**
          * Type alias used to remove overloads which are not compatible with the stored data
          */
         template<typename KeyType>
         using if_type_is_key_type = std::enable_if_t<storage_types::include<std::decay_t<KeyType>>,int>;

         public_key() = default;
         public_key( public_key&& ) = default;
         public_key( const public_key& ) = default;
         public_key& operator= (const public_key& ) = default;

         template< typename KeyType, if_type_is_key_type<KeyType> = 1 >
         public_key( KeyType&& pubkey )
            :_storage(std::forward<KeyType>(pubkey))
         {}


         template<typename KeyType, if_type_is_key_type<KeyType> = 1 >
         const KeyType& get() const {
            return _storage.template get<KeyType>();
         }

         template<typename KeyType, if_type_is_key_type<KeyType> = 1 >
         auto contains() const {
            return _storage.template contains<KeyType>();
         }

         template<typename Visitor>
         auto visit(Visitor& v) {
            return _storage.visit(v);
         }

         explicit public_key(const std::string& base58str);
         explicit operator std::string() const;


         template<typename KeyType, if_type_is_key_type<KeyType> = 1 >
         bool operator == (const KeyType& k) const {
            return contains<KeyType>() && get<KeyType>() == k;
         };

         friend bool operator == ( const public_key& p1, const public_key& p2);
         friend bool operator != ( const public_key& p1, const public_key& p2);
         friend bool operator < ( const public_key& p1, const public_key& p2);
         friend std::ostream& operator<< (std::ostream& s, const public_key& k);
         friend struct fc::reflector<public_key>;

         static bool is_valid_v1(const std::string& base58str);

      private:
         storage_type _storage;

   }; // public_key


} } // namespace eosio::blockchain

namespace fc {
void to_variant(const eosio::blockchain::public_key& var,  fc::variant& vo);

void from_variant(const fc::variant& var,  eosio::blockchain::public_key& vo);
} // namespace fc

FC_REFLECT(eosio::blockchain::public_key, (_storage) )

