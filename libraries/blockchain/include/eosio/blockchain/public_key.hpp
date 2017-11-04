#pragma once
#include <fc/reflect/reflect.hpp>
#include <fc/reflect/variant.hpp>
#include <eosio/blockchain/internal/meta_utils.hpp>

namespace eosio { namespace blockchain { namespace crypto {

/**
 * Templated type information structure used by default
 * function implementations
 *
 * data_type - the type of the serializable data
 * prefix() - constexpr method that returns the prefix used in stringification
 * default_type - optional boolean indicating that this is the default type
 *
 *
 * @tparam KeyType
 */
template< typename KeyType >
struct public_key_type_info;

namespace impl {
   using eosio::blockchain::internal::meta_array;

   /**
    *
    * @tparam KeyType - the type of public key
    * @param key - the public key
    * @return a data value suitable for serialization
    */
   template<typename KeyType>
   auto public_key_to_data( const KeyType& key ) {
      return public_key_type_info<KeyType>::data_type(key);
   }

   template<typename KeyType>
   constexpr const char* public_key_prefix() {
      return public_key_type_info<KeyType>::prefix();
   }

   template<typename KeyType>
   struct public_key_is_default_impl {
      using info = public_key_type_info<KeyType>;

      template<typename T>
      static constexpr std::true_type has_default(decltype(T::default_type)) {
         return std::true_type();
      }

      template<typename T>
      static constexpr std::false_type _value(...) {
         return std::false_type();
      }

      typedef decltype(has_default<info>(true)) type;

      static constexpr bool get_default(std::true_type) {
         return info::default_type;
      }

      static constexpr bool get_default(std::false_type) {
         return false;
      }

      static constexpr bool value() { return get_default(type()); }
   };

   template<typename KeyType>
   constexpr bool public_key_is_default() {
      return public_key_is_default_impl<KeyType>::value();
   }

   template<typename KeyInfoType>
   struct meta_prefix {
      struct provider {
         static constexpr const char* value() {
            return public_key_prefix<KeyInfoType>();
         }
      };

      using type = internal::meta_string_from_provider<provider>;
   };

   template<typename ...KeyInfoTypes>
   using meta_prefix_array = typename meta_array<KeyInfoTypes...>::template map<meta_prefix>;

   template<typename... Ts>
   struct type_info;

   template<typename T, typename... Ts>
   struct type_info<T, Ts...> {
      static const size_t default_count = (public_key_is_default<T>() ? 1 : 0) + type_info<Ts...>::default_count;
      static const bool unique_prefixes = type_info<Ts...>::unique_prefixes && !meta_prefix_array<Ts...>::template contains<typename meta_prefix<T>::type>();
   };

   template<>
   struct type_info<> {
      static const size_t default_count = 0;
      static const bool unique_prefixes = true;
   };
}

template<typename ...KeyTypes>
class composite_public_key
{
   static_assert(impl::type_info<KeyTypes...>::default_count > 0, "type arguments do not define a default type");
   static_assert(impl::type_info<KeyTypes...>::default_count <= 1, "type arguments define multiple default types");
   static_assert(impl::type_info<KeyTypes...>::unique_prefixes, "type arguments define non unique prefixes");

   public:
      composite_public_key();
      composite_public_key( composite_public_key&& ) = default;
      composite_public_key( const composite_public_key& ) = default;
      composite_public_key& operator= (const composite_public_key& ) = default;

      template< typename KeyType, std::enable_if_t<impl::meta_array<KeyTypes...>::template contains<typename std::decay_t<KeyType>>()>* = nullptr >
      composite_public_key( KeyType&& pubkey )
      :_storage(std::forward<KeyType>(pubkey))
      {}

      explicit composite_public_key(const std::string& base58str);
      explicit operator std::string() const;

      template<typename KeyType>
      const KeyType& get() const {
         return _storage.template get<KeyType>();
      }

      template<typename KeyType>
      auto contains() const {
         return _storage.template contains<KeyType>();
      }

      template<typename Visitor>
      auto visit(Visitor& v) {
         return _storage.visit(v);
      }

      friend bool operator == ( const composite_public_key& p1, const composite_public_key& p2);
      friend bool operator != ( const composite_public_key& p1, const composite_public_key& p2);
      friend bool operator < ( const composite_public_key& p1, const composite_public_key& p2);
      friend std::ostream& operator<< (std::ostream& s, const composite_public_key& k);

      template<typename Visitor>
      friend void visit( const Visitor& v );

      static bool is_valid_v1(const std::string& base58str);

      fc::static_variant<KeyTypes...> _storage;
}; // public_key


} } } // namespace eosio::blockchain::crypto

namespace fc {
template<typename ...Ts>
void to_variant(const eosio::blockchain::crypto::composite_public_key<Ts...>& var,  fc::variant& vo);

template<typename ...Ts>
void from_variant(const fc::variant& var,  eosio::blockchain::crypto::composite_public_key<Ts...>& vo);
} // namespace fc

FC_REFLECT_TEMPLATE( (typename ...Ts), eosio::blockchain::crypto::composite_public_key<Ts...>, (_storage) )

