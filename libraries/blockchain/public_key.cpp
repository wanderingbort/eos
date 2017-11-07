#include <eosio/blockchain/public_key.hpp>
#include <fc/crypto/base58.hpp>
#include <fc/crypto/ripemd160.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/raw.hpp>

template<typename DataType>
struct checksummed_data
{
   checksummed_data() {}
   uint32_t     check = 0;
   DataType     data;
};

FC_REFLECT_TEMPLATE((typename T), checksummed_data<T>, (data)(check) )



namespace eosio { namespace blockchain {

using eosio::blockchain::internal::meta_array_map_t;
using eosio::blockchain::internal::meta_array_contains_v;
using eosio::blockchain::internal::meta_array_contains;
using eosio::blockchain::internal::meta_array;

template<typename KeyType>
struct public_key_type_info;

/**
 * specialize the public_key_type_info struct for secp256k1 public keys
 * NOTE: this is the default for backwards compatibility
 */
template<>
struct public_key_type_info<fc::ecc::public_key> {
   typedef fc::ecc::public_key_data    data_type;
   static constexpr bool               default_type = true;
   static constexpr char               prefix[] = "K1";
};

/**
 * specialize the public_key_type_info struct with sha256 values
 * as a test case
 */
template<>
struct public_key_type_info<fc::sha256> {
   typedef fc::sha256                  data_type;
   static constexpr bool               default_type = false;
   static constexpr char               prefix[] = "H1";
};

template<typename KeyInfoType>
struct meta_prefix {
   struct provider {
      static constexpr const char* value() {
         return public_key_type_info<KeyInfoType>::prefix;
      }
   };

   using type = internal::meta_string_from_provider<provider>;
};

template<typename KeyInfoType>
using meta_prefix_t = typename meta_prefix<KeyInfoType>::type;

template<typename ...KeyInfoTypes>
using meta_prefix_array = meta_array_map_t<meta_prefix, KeyInfoTypes...>;

/**
 * Compile time utility class used to help prove correct configuration of key types
 *
 * @tparam Ts The Key types
 */
template<typename... Ts>
struct type_info;

template<typename T, typename... Ts>
struct type_info<T, Ts...> {
   using prefix_t = meta_prefix_t<T>;
   using default_t = std::conditional_t<public_key_type_info<T>::default_type, T, typename type_info<Ts...>::default_t>;

   static const size_t default_count = (public_key_type_info<T>::default_type ? 1 : 0) + type_info<Ts...>::default_count;
   static const bool unique_prefixes = type_info<Ts...>::unique_prefixes && !(meta_prefix_array<Ts...>::template contains<prefix_t>);
};

template<>
struct type_info<> {
   using default_t = void;
   static const size_t default_count = 0;
   static const bool unique_prefixes = true;
};

/// convenience type to explode fc::static_variant
template<typename...KeyTypes>
struct type_info<fc::static_variant<KeyTypes...>> {
   using impl = type_info<KeyTypes...>;

   static const size_t default_count = impl::default_count;
   static const bool unique_prefixes = impl::unique_prefixes;
};

/**
 * Compile time asserts that prove
 *  - that there is exactly one default key type
 *  - that all prefixes are unique
 */
static_assert(type_info<public_key::storage_type>::default_count > 0, "type arguments do not define a default type");
static_assert(type_info<public_key::storage_type>::default_count <= 1, "type arguments define multiple default types");
static_assert(type_info<public_key::storage_type>::unique_prefixes, "type arguments define non unique prefixes");

static const std::string default_prefix = std::string(config::public_key_default_prefix);

template<typename ...Ts>
struct base58_str_parser;

template<typename KeyType, typename ...Rem>
struct base58_str_parser<KeyType, Rem...> {
   static bool prefix_matches(const std::string& prefix, const std::string& base58str) {
      return base58str.size() > prefix.size() && base58str.substr(0, prefix.size()) == prefix;
   }

   static public_key::storage_type apply(const std::string& base58str)
   {
      using data_type = typename public_key_type_info<KeyType>::data_type;
      std::string prefix( public_key_type_info<KeyType>::prefix );
      size_t prefix_len = 0;

      if (public_key_type_info<KeyType>::default_type && prefix_matches(default_prefix, base58str)) {
         prefix_len = default_prefix.size();
      } else if (prefix_matches(prefix, base58str)) {
         prefix_len = prefix.size();
      }

      if (prefix_len > 0) {
         auto bin = fc::from_base58(base58str.substr(prefix_len));
         auto bin_key = fc::raw::unpack<checksummed_data<data_type>>(bin);
         const auto& key_data = bin_key.data;
         FC_ASSERT(fc::ripemd160::hash((const char *)&key_data, sizeof(key_data))._hash[0] == bin_key.check);
         return public_key::storage_type(KeyType(key_data));
      }

      return base58_str_parser<Rem...>::apply(base58str);
   }
};

template<>
struct base58_str_parser<> {
   static public_key::storage_type apply(const std::string& base58str) {
      FC_ASSERT(false, "Public Key ${base58str} has invalid prefix", ("base58str", base58str));
   }
};


/**
 * Destructure a variant and call the parse_base58str on it
 * @tparam Ts
 * @param base58str
 * @return
 */
template<typename ...Ts>
struct base58_str_parser<fc::static_variant<Ts...>> {
   static public_key::storage_type apply(const std::string& base58str) {
      return base58_str_parser<Ts...>::apply(base58str);
   }
};

struct base58str_visitor : public fc::visitor<std::string> {
   template< typename KeyType >
   std::string operator()( const KeyType& key ) const {
      using data_type = typename public_key_type_info<KeyType>::data_type;
      checksummed_data<data_type> data;
      data.data = data_type(key);
      data.check = fc::ripemd160::hash( (char *)&data.data, sizeof(data.data))._hash[0];
      auto packed = fc::raw::pack( data );
      auto prefix = public_key_type_info<KeyType>::default_type ? default_prefix : std::string( public_key_type_info<KeyType>::prefix );

      return prefix + fc::to_base58( packed.data(), packed.size() );
   }
};

public_key::public_key(const std::string& base58str)
:_storage(base58_str_parser<storage_type>::apply(base58str))
{
}

public_key::operator std::string() const {
   return _storage.visit(base58str_visitor());
}

bool public_key::is_valid_v1(const std::string& base58str)
{
   try {
      base58_str_parser<storage_type>::apply(base58str);
      return true;
   } catch (...) {
      return false;
   }
}

struct eq_visitor : public fc::visitor<bool> {
   eq_visitor(const public_key &b)
   :_b(b)
   {}

   template< typename KeyType >
   bool operator()( const KeyType& a ) const {
      const auto& b = _b.template get<KeyType>();
      return a == b;
   }

   const public_key &_b;
};

bool operator == ( const public_key& p1, const public_key& p2) {
   return p1._storage.which() == p1._storage.which() && p1._storage.visit(eq_visitor(p2));;
}

bool operator != ( const public_key& p1, const public_key& p2)
{
   return !(p1 == p2);
}

struct less_visitor : public fc::visitor<bool> {
   less_visitor(const public_key &b)
      :_b(b)
   {}

   template< typename KeyType >
   bool operator()( const KeyType& a ) const {
      using data_type = typename public_key_type_info<KeyType>::data_type;
      const auto& b = _b.template get<KeyType>();
      return data_type(a) < data_type(b);
   }

   const public_key &_b;
};

bool operator < ( const public_key& p1, const public_key& p2)
{
   return p1._storage.which() < p1._storage.which() || p1._storage.visit(less_visitor(p2));
}

std::ostream& operator<< (std::ostream& s, const public_key& k)
{
   s << "public_key(" << std::string(k) << ')';
   return s;
}

/*
    composite_public_key::public_key(const std::string& base58str)
    {
      // TODO:  Refactor syntactic checks into static is_valid()
      //        to make public_key API more similar to address API
       std::string prefix( config::public_key_prefix );

       const size_t prefix_len = prefix.size();
       FC_ASSERT(base58str.size() > prefix_len);
       FC_ASSERT(base58str.substr(0, prefix_len) ==  prefix , "", ("base58str", base58str));
       auto bin = fc::from_base58(base58str.substr(prefix_len));
       auto bin_key = fc::raw::unpack<binary_key>(bin);
       key_data = bin_key.data;
       FC_ASSERT(fc::ripemd160::hash(key_data.data, key_data.size())._hash[0] == bin_key.check);
    }

    public_key::operator fc::ecc::public_key_data() const
    {
       return key_data;
    };

    public_key::operator fc::ecc::public_key() const
    {
       return fc::ecc::public_key(key_data);
    };

    public_key::operator std::string() const
    {
       binary_key k;
       k.data = key_data;
       k.check = fc::ripemd160::hash( k.data.data, k.data.size() )._hash[0];
       auto data = fc::raw::pack( k );
       return config::public_key_prefix + fc::to_base58( data.data(), data.size() );
    }

    bool operator == (const public_key& p1, const fc::ecc::public_key& p2)
    {
       return p1.key_data == p2.serialize();
    }

    bool operator == (const public_key& p1, const public_key& p2)
    {
       return p1.key_data == p2.key_data;
    }

    bool operator != (const public_key& p1, const public_key& p2)
    {
       return p1.key_data != p2.key_data;
    }
    
    bool operator <(const public_key& p1, const public_key& p2)
    {
        return p1.key_data < p2.key_data;
    };

    std::ostream& operator<<(std::ostream& s, const public_key& k) {
       s << "public_key(" << std::string(k) << ')';
       return s;
    }
*/

} } // eosio::blockchain

namespace fc
{
    using namespace std;
    void to_variant(const eosio::blockchain::public_key& var, fc::variant& vo)
    {
        vo = std::string(var);
    }

    void from_variant(const fc::variant& var, eosio::blockchain::public_key& vo)
    {
        vo = eosio::blockchain::public_key(var.as_string());
    }
    
} // fc

