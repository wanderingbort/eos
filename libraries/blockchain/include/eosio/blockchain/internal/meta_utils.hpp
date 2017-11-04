#pragma once

namespace eosio { namespace blockchain { namespace internal {

   template< char... >
   struct meta_string_length;

   template< char First, char ...Rem >
   struct meta_string_length<First, Rem...> {
      static const size_t value = 1 + meta_string_length<Rem...>::value;
   };

   template<>
   struct meta_string_length<> {
      static const size_t value = 0;
   };

   template< char ...Chars >
   struct meta_string {
      static const size_t length = meta_string_length<Chars...>::value;

      template<typename Other>
      constexpr bool equals() {
         return std::is_same<meta_string<Chars...>, Other>::value;
      }
   };

   constexpr size_t c_string_length(const char* str, size_t offset = 0) {
      if (str[offset] == '\0') {
         return 0;
      }

      return 1 + c_string_length(str, offset + 1);
   }

   template< typename Provider, size_t Offset = 0 >
   struct provider_info {
      static const size_t length = c_string_length(Provider::value());
   };

   /**
    * Type which takes a provider and converts it to a meta_string
    * A provider is anything whose operator() is compile-time-constant and returns an array of or pointer to char
    * @tparam Provider
    */
   template<typename Provider, size_t Length = provider_info<Provider>::length, char ...Chars>
   struct meta_string_from_provider_impl {
      using type = typename meta_string_from_provider_impl<Provider, Length-1, Provider::value()[Length - 1], Chars...>::type;
   };

   template < typename Provider, char ...Chars >
   struct meta_string_from_provider_impl < Provider, 0, Chars... >
   {
      using type = meta_string < Chars... >;
   };

   template<typename Provider>
   using meta_string_from_provider = typename meta_string_from_provider_impl<Provider>::type;

   template<typename ...Entries>
   struct meta_array;

   namespace impl {
      template<typename Needle, typename ...Haystack>
      struct contains;

      template<typename Needle, typename First, typename ...Rem>
      struct contains<Needle, First, Rem...> {
         static const bool value = std::is_same<Needle, First>::value || contains<Needle, Rem...>::value;
      };

      template<typename Needle>
      struct contains<Needle> {
         static const bool value = false;
      };

      template<template<typename> class Mapper, typename ...Entries>
      struct meta_array_map;

      template<template<typename> class Mapper, typename First, typename ...Rem>
      struct meta_array_map<Mapper, First, Rem...> {
         using type = typename meta_array_map<Mapper, Rem...>::type::template with<Mapper<First>::type>;
      };

      template<template<typename> class Mapper, typename First>
      struct meta_array_map<Mapper, First> {
         using type = meta_array<typename Mapper<First>::type>;
      };

      template<template<typename> class Mapper>
      struct meta_array_map<Mapper> {
         using type = meta_array<>;
      };

   }

   template<typename ...Entries>
   struct meta_array {

      template<typename Needle>
      static constexpr bool contains() {
         return impl::contains<Needle, Entries...>::value;
      }

      template<typename Entry>
      using with = meta_array<Entry, Entries...>;

      template<template<typename> class Mapper>
      using map = typename impl::meta_array_map<Mapper, Entries...>::type;
   };

} } };