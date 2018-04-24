/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <utility>

namespace eosio { namespace net_v2 { namespace state_machine {
   namespace impl {
      template<typename Needle, typename ...Haystack>
      struct index_of;

      template<typename Needle>
      struct index_of<Needle> : public std::integral_constant<std::size_t, 0> {};

      template<typename Needle, typename ...Haystack>
      struct index_of<Needle, Needle, Haystack...> : public std::integral_constant<std::size_t, 0> {};

      template<typename Needle, typename NotNeedle, typename ...Haystack>
      struct index_of<Needle, NotNeedle, Haystack...> : public std::integral_constant<std::size_t, 1 + index_of<Needle, Haystack...>::value> {};

      template<typename Needle, typename ...Haystack>
      constexpr std::size_t index_of_v = index_of<Needle, Haystack>::value;

      template<typename Needle, typename ...Haystack>
      constexpr bool contains_v = index_of_v<Needle, Haystack...> < sizeof...(Haystack);
   }

   template<typename State>
   struct next_state{};

   template<typename ... States>
   struct next_states {
      std::size_t which;
      bool valid;

      next_states()
      :which(0)
      ,valid(false)
      {}


      template<typename State, std::enable_if_t<impl::contains_v<State, States...>, int> = 0>
      next_states(const next_state<State>& other)
      :which(impl::index_of_v<State, States...>)
      ,valid(true)
      {
      }


      template<typename Choice>
      static auto of() -> std::enable_if_t<impl::contains_v<Choice, States...>, next_states> {
         return next_states<Choice>();
      }
   };

   namespace impl {
      struct state_trait_helpers {
         template <typename State>
         static constexpr auto
         test_enter(int) -> decltype(std::declval<State&>().enter(), std::true_type())
         {
            return std::true_type();
         }

         template<typename>
         static constexpr std::false_type test_enter(...) {
            return std::false_type();
         }

         template <typename State>
         static constexpr auto
         test_exit(int) -> decltype(std::declval<State&>().exit(), std::true_type())
         {
            return std::true_type();
         }

         template<typename>
         static constexpr std::false_type test_exit(...)
         {
            return std::false_type();
         }

         template<typename Construct, typename From>
         static constexpr auto
         test_can_construct_from(int) -> decltype(Construct(std::declval<const From&>()), std::true_type())
         {
            return std::true_type();
         };

         template<typename, typename>
         static constexpr std::false_type test_can_construct_from(...)
         {
            return std::false_type();
         }

         template<typename State, typename ... Args>
         static constexpr auto
         test_has_exact_on(int) -> decltype(std::declval<State&>().on(std::declval<Args>()...), std::true_type())
         {
            return std::true_type();
         };

         template<typename...>
         static constexpr std::false_type test_has_exact_on(...)
         {
            return std::false_type();
         }

         template<typename State, typename ... Args>
         static constexpr auto
         test_has_void_exact_on(int) -> std::enable_if_t<std::is_same<decltype(std::declval<State&>().on(std::declval<Args>()...)), void>::value  , std::true_type>
         {
            return std::true_type();
         };

         template<typename...>
         static constexpr std::false_type test_has_void_exact_on(...)
         {
            return std::false_type();
         }

         template<typename State, typename ... Args>
         static constexpr auto
         test_has_exact_post(int) -> decltype(std::declval<State&>().post(std::declval<Args>()...), std::true_type())
         {
            return std::true_type();
         };

         template<typename...>
         static constexpr std::false_type test_has_exact_post(...) {
            return std::false_type();
         }
      };

      template<typename State>
      struct state_traits {
         static constexpr bool has_enter_v = state_trait_helpers::test_enter<State>(0);
         static constexpr bool has_exit_v = state_trait_helpers::test_exit<State>(0);

         template<typename Other>
         static constexpr bool can_construct_from_v = state_trait_helpers::test_can_construct_from<State, Other>(0);

         template<typename ... Args>
         static constexpr bool has_exact_on_v = state_trait_helpers::test_has_exact_on<State, Args...>(0);

         template<typename ... Args>
         static constexpr bool has_void_exact_on_v = state_trait_helpers::test_has_void_exact_on<State, Args...>(0);

         template<typename ... Args>
         static constexpr bool has_exact_post_v = state_trait_helpers::test_has_exact_post<State, Args...>(0));

      };

      template<typename State, typename Message, typename ... Args>
      struct on_traits;

      template<typename State, typename Message>
      struct on_traits<State, Message> {
         static constexpr bool exists = state_trait_helpers::test_has_exact_on<State, Message>(0);
         static constexpr int arg_count = 0;
         static constexpr bool is_void = exists && state_trait_helpers::test_has_void_exact_on<State, Message>(0);
      };

      template<typename State, typename Message, typename FirstArg, typename ... Args>
      struct on_traits<State, Message, FirstArg, Args...> {
         using next_type = on_traits<State, Message, Args...>;

         static constexpr bool exact = state_trait_helpers::test_has_exact_on<State, Message, FirstArg, Args...>(0);
         static constexpr bool exists = exact || next_type::exists;
         static constexpr bool is_void = exact ? state_trait_helpers::test_has_void_exact_on<State, Message, FirstArg, Args...>(0).value : next_type::is_void;
         static constexpr int arg_count = exact ? sizeof...(Args) + 1 : next_type::arg_count;
      };

      struct enter_visitor : public fc::visitor<void> {
         template<typename State>
         auto operator() (State& s) const -> std::enable_if_t<state_traits<State>::has_enter_v> {
            s.enter();
         }

         // do nothing there is no enter
         void operator() ( ... ) {}
      };

      struct exit_visitor : public fc::visitor<void> {
         template<typename State>
         auto operator() (State& s) const -> std::enable_if_t<state_traits<State>::has_exit_v> {
            s.exit();
         }

         // do nothing, there is no exit
         void operator() ( ... ) {}
      };

      template<typename Machine, typename NextType>
      struct transition_helper;

      template<typename Machine>
      struct transition_helper<Machine, next_states<>> {
         template<typename ...Args>
         static void apply(Machine&, std::size_t, Args...) {
            // ran of the end... should probably assert
         }
      };

      template<typename Machine, typename State, typename ... States>
      struct transition_helper<Machine, next_states<State, States...>> {
         template<typename ... Args>
         static void apply(Machine& m, std::size_t which, Args... args) {
            if (which == 0) {
               m.set_state<State>(args...);
            } else {
               transition_helper<Machine, next_states<States...>>::apply(m, which - 1, args...);
            }
         }
      };

      template<typename Machine, typename MessageType, typename ArgTuple>
      struct on_visitor;

      template<typename Machine, typename MessageType, typename ... Args>
      struct on_visitor<std::tuple<Args...>> : public fc::visitor<void> {
         using args_type = std::tuple<Args...>;
         Machine& m;
         const MessageType& msg;
         args_type& args;

         template<typename State>
         using _on_traits = on_traits<State, MessageType, Args...>;

         on_visitor(Machine& m, const MessageType& msg, args_type& args)
         :m(m), msg(msg), args(args)
         {
         }

         template<typename State, std::size_t ... I>
         void call_void_on(State &s, std::index_sequence<I...>) {
            s.on(msg, args.get<I>() ... );
         };

         template<typename State, std::size_t ... I>
         void call_on(State &s, std::index_sequence<I...>) {
            auto res = s.on(msg, args.get<I>() ... );
            if (res.valid) {
               transition_helper<Machine, decltype(res)>::apply(m, res.which, args.get<I>() ... );
            }
         };

         template<typename State>
         auto operator() (State& s) -> std::enable_if_t<_on_traits<State>::exists && _on_traits<State>::is_void> {
            call_void_on(s, std::make_index_sequence< _on_traits<State>::arg_count >{});
         }

         template<typename State>
         auto operator() (State& s) -> std::enable_if_t<_on_traits<State>::exists && !_on_traits<State>::is_void > {
            call_on(s, std::make_index_sequence< _on_traits<State>::arg_count >{});
         }

         // do nothing, there is no exit
         void operator() ( ... ) {}
      };

      template<typename Machine, typename MessageType, typename ArgsTuple>
      auto make_on_visitor(Machine& m, const MessageType& msg, ArgsTuple &tuple) {
         return on_visitor<Machine, MessageType, ArgsTuple>(m, msg, tuple);
      }

      template<typename ... Args>
      struct post_visitor<std::tuple<Args...>> : public fc::visitor<void> {
         using args_type = std::tuple<Args...>;
         args_type& args;

         post_visitor(args_type& args)
         :args(args)
         {
         }

         template<typename State, std::size_t ... I>
         void call_post(State &s, std::index_sequence<I...>) {
            s.post(args.get<I>() ... );
         };

         template<typename State>
         auto operator() (State& s) -> std::enable_if_t<state_traits<State>::template has_exact_post_v<Args...>> {
            call_post(s, std::index_sequence_for<Args...>{});
         }

         // do nothing, there is no exit
         void operator() ( ... ) {}
      };

      template<typename ArgsTuple>
      auto make_post_visitor(ArgsTuple &tuple) {
         return post_visitor<ArgsTuple>(tuple);
      }

      struct exit_visitor : public fc::visitor<void> {
         template<typename State>
         auto operator() (State& s) const -> std::enable_if_t<state_traits<State>::has_exit_v> {
            s.exit();
         }

         // do nothing, there is no exit
         void operator() ( ... ) {}
      };

      template<typename NextState>
      struct construct_next_state_visitor : public fc::visitor<NextState> {
         template<typename State>
         auto operator() (const State& s) const -> std::enable_if_t<state_traits<NextState>::can_construct_from<State>, NextState> {
            return NextState(s);
         }

         // do nothing, there is no exit
         NextState operator() ( ... ) {
            return NextState();
         }
      };

      template<typename Cls, typename Type, Type Cls::* Val>
      struct state_machine_member {};

      template<typename Cls, typename ... Vals>
      struct container_post_visitor_impl;

      template<typename Cls, typename Type, Type Cls::* Val>
      struct container_post_visitor_impl<Cls, state_machine_member<Cls, Type, Val>> {
         template<typename ...Args>
         static void call(Cls& container, Args ... args) {
            (container.*Val).post(args...);
         }
      };

      template<typename Cls, typename First, typename ... Rest>
      struct container_post_visitor_impl<Cls, First, Rest...> {
         template<typename ...Args>
         static void call(Cls& container, Args ... args) {
            container_post_visitor_impl<Cls, First>::call(container, args...);
            container_post_visitor_impl<Cls, Rest...>::call(container, args...);
         }
      };

      template<typename Cls, typename MemberTuple>
      struct container_post_visitor;

      template<typename Cls, typename ... Members>
      struct container_post_visitor<Cls, std::tuple<Members...>> {

         template<typename ...Args>
         static void call(Cls &container, Args ... args) {
            container_post_visitor_impl<Cls, Members...>::call(container, args...);
         }
      };


   };

   struct no_default_state {
      template<typename ... Args>
      next_states<> on(Args...) {
         // should assert
         return next_states<>{};
      }
   };

   template<typename ... States>
   struct machine {
      template<typename NextState, typename ...Args>
      void set_state(Args... args) {
         state.visit(impl::exit_visitor(args...));
         state = state.visit(impl::construct_next_state_visitor<NextState>());
         state.visit(impl::enter_visitor(args...));
      }

      using state_type = fc::static_variant<States...>;
      state_type state;

      template<typename T, typename ... Args>
      void post( const T& event_or_message, Args args...) {
         // call current state handlers if they exist
         state.visit(impl::make_on_visitor(*this, event_or_message, std::forward_as_tuple(args...)));

         // call post if it exists to support state_machine as a state
         state.visit(impl::make_post_visitor(std::forward_as_tuple(event_or_message, args...)));
      };

      // support for state_machine as a state
      void enter() {
         state.visit(impl::enter_visitor());
      }

      void exit() {
         state.visit(impl::exit_visitor());
      }
   };

   template<typename Derived>
   struct container {

      template<typename Type, Type Derived::* Val>
      using member = impl::state_machine_member<Derived, Type, Val>;

      template<typename T, typename ... Args>
      void post( const T& event_or_message, Args args...) {
         Derived& upcast = *reinterpret_cast<Derived*>(this);
         impl::container_post_visitor<Derived, typename Derived::state_machine_member_list>::call(upcast, event_or_message, upcast, args ...);
      };

   };
} } }