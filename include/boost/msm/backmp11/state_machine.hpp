// Copyright 2025 Christian Granzin
// Copyright 2008 Christophe Henry
// henry UNDERSCORE christophe AT hotmail DOT com
// This is an extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2005 David Abrahams and Aleksey Gurtovoy. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MSM_BACKMP11_STATE_MACHINE_HPP
#define BOOST_MSM_BACKMP11_STATE_MACHINE_HPP

#include <utility>

#include <boost/assert.hpp>
#include <boost/mp11.hpp>

#include <boost/msm/row_tags.hpp>
#include <boost/msm/backmp11/detail/composite_state.hpp>
#include <boost/msm/backmp11/detail/favor_runtime_speed.hpp>
#include <boost/msm/backmp11/detail/history_impl.hpp>
#include <boost/msm/backmp11/detail/state_machine_base.hpp>
#include <boost/msm/backmp11/detail/state_visitor.hpp>
#include <boost/msm/backmp11/detail/transition_table.hpp>

namespace boost::msm::backmp11
{

namespace detail
{

// Wrapper to invoke a reflect free function
// (required for ADL).
struct invoke_reflect_free
{
    template <typename State, typename Visitor>
    void operator()(State&& state, Visitor&& visitor)
    {
        reflect(std::forward<State>(state), std::forward<Visitor>(visitor));
    }
};

} // namespace detail


/**
 * @brief Back-end for a state machine.
 *
 * Wraps a state machine front-end into an executable state machine.
 *
 * @tparam FrontEnd
 * @tparam Config
 * @tparam Derived
 */
template <class FrontEnd,
          class Config = default_state_machine_config,
          class Derived = no_derived>
class state_machine
    : public detail::composite_state<
          FrontEnd, Config,
          mp11::mp_if_c<std::is_same_v<Derived, no_derived>,
                        state_machine<FrontEnd, Config, Derived>, Derived>,
          detail::state_machine_base<
              Config,
              detail::get_nesting_role<
                  Config,
                  mp11::mp_if_c<std::is_same_v<Derived, no_derived>,
                                state_machine<FrontEnd, Config, Derived>,
                                Derived>>()>,
          detail::state_machine_tag>
{
    using derived_resolved =
        mp11::mp_if_c<std::is_same_v<Derived, no_derived>, state_machine, Derived>;
    using context_base = detail::state_machine_base<
        Config, detail::get_nesting_role<Config, derived_resolved>()>;
    using composite_state_base =
        detail::composite_state<FrontEnd, Config, derived_resolved, context_base,
                                detail::state_machine_tag>;

    static_assert(
        detail::is_composite<FrontEnd>::value,
        "FrontEnd must be a composite state");

  public:
    /// Type of the front-end (same as FrontEnd).
    using front_end_t = FrontEnd;
    /// Type of the configuration (same as Config, see @ref state_machine_config).
    using config_t = typename context_base::config_t;
    /// Type of the context (see @ref state_machine_config::context).
    using context_t = typename context_base::context_t;
    /// Type of the context (see @ref state_machine_config::observer).
    using observer_t = typename context_base::observer_t;
    /// Type of the root machine (see @ref state_machine_config::root_sm).
    using root_sm_t = typename context_base::root_sm_t;
    /// Type of the derived machine (corresponds to Derived).
    using derived_t = derived_resolved;

    // Inherited from composite_state, but re-exposed so external metafunctions
    // (state visitors, generate_state_set, ...) reach it as state_machine::internal.
    using internal = typename composite_state_base::internal;
    using states_t = typename composite_state_base::states_t;

  private:
    using state_set = typename internal::state_set;
    using state_map = typename internal::state_map;
    static constexpr auto nr_regions = internal::nr_regions;
    static constexpr bool has_event_pool = composite_state_base::has_event_pool;
    using active_state_ids_t = std::array<uint16_t, nr_regions>;
    using initial_state_ids =
        mp11::mp_transform<internal::template get_state_id,
                           typename internal::initial_states>;
    using compile_policy_impl =
        detail::compile_policy_impl<typename config_t::compile_policy>;
    using event_pool_processor = typename context_base::event_pool_processor;

    // The shared core (any enclosing composite_state and its entry visitor)
    // enters/exits this machine and routes the hidden on_state_entry_completed
    // to it; grant every composite_state access to the private orchestration,
    // mirroring the way state_machine used to befriend every state_machine.
    template <class, class, class, class, class>
    friend class detail::composite_state;

    template <class, class, class>
    friend class state_machine;

    template <typename, typename>
    friend class detail::history_impl;

    template <typename StateMachine>
    friend struct detail::transition_table_impl;

    template <typename Policy, typename>
    friend struct detail::compile_policy_impl;

    template <typename, bool, template <typename> typename...>
    friend class detail::state_visitor_base_impl;
    template <typename, typename, template <typename> typename...>
    friend class detail::event_deferral_visitor;

    friend class detail::event_occurrence;
    template <typename Event>
    friend class detail::deferred_event;

    template<typename T0, typename T1, typename T2, typename F>
    friend void reflect(state_machine<T0, T1, T2>&, F&&);
    template<typename T0, typename T1, typename T2, typename F>
    friend void reflect(const state_machine<T0, T1, T2>&, F&&);

    using history_impl = detail::history_impl<typename front_end_t::history,
                                              initial_state_ids>;

    void init()
    {
        static_assert(
            std::is_base_of_v<state_machine, derived_t>,
            "Derived must inherit from state_machine");
        if constexpr (std::is_same_v<root_sm_t, no_root_sm> ||
                      std::is_same_v<root_sm_t, derived_t>)
        {
            *this->m_root_sm = this;
            using visitor_t = detail::init_state_visitor<derived_t>;
            visitor_t visitor{self()};
            detail::visit_if<visit_mode::all_recursive,
                             visitor_t::template predicate>(self(), visitor);
        }
    }

  public:
    /**
     * @brief Constructs and forwards constructor arguments to the back-end.
     *
     * Requires constructor arguments as configured:
     * - Context& (if context = Context is set)
     * - Observer& (if observer = observer_ref<Observer> is set)
     */
    template <typename... Args>
    state_machine(Args&&... args)
        : composite_state_base(std::forward<Args>(args)...)
    {
        init();
    }

    // Copy constructor.
    state_machine(state_machine const& rhs)
        : composite_state_base(rhs)
    {
        init();
    }

    state_machine(state_machine& rhs)
        : state_machine(static_cast<state_machine const&>(rhs))
    {
    }

    // Copy assignment operator.
    state_machine& operator=(state_machine const& rhs) = default;

    // Move constructor.
    state_machine(state_machine&& rhs)
        : composite_state_base(std::move(rhs))
    {
        init();
    }

    // Move assignment operator.
    state_machine& operator=(state_machine&& rhs) = default;

    /**
     * @brief Starts the state machine with a @ref starting event.
     *
     * Sets up the initial state(s) and calls its entry method(s).
     */
    void start()
    {
        start(starting{});
    }

    /**
     * @brief Starts the state machine with a custom event.
     *
     * Sets up the initial state(s) and calls its entry method(s).
     */
    template <class Event>
    void start(Event const& initial_event)
    {
        // Assert for a case where root sm was not set up correctly
        // after construction.
        if constexpr (!std::is_same_v<typename Config::root_sm, no_root_sm>)
        {
            BOOST_ASSERT_MSG(&(this->get_root_sm()),
            "Root sm must be passed as Derived and configured as root_sm");
        }
        if (this->m_machine_state == machine_state::stopped)
        {
            on_entry(initial_event, this->get_fsm_argument());
        }
    }

    /**
     * @brief Stops the state machine with a @ref stopping event.
     *
     * Calls the active state's exit method(s).
     */
    void stop()
    {
        stop(stopping{});
    }

    /**
     * @brief Stops the state machine with a custom event.
     *
     * Calls the active state's exit method(s).
     */
    template <class Event>
    void stop(Event const& final_event)
    {
        if (this->m_machine_state == machine_state::idle)
        {
            on_exit(final_event, this->get_fsm_argument());
        }
    }

    /// Processes the event.
    template<class Event>
    process_result process_event(Event const& event)
    {
        return process_event_observed(
            compile_policy_impl::normalize_event(event),
            detail::process_info::direct_call);
    }

    /**
     * @brief Puts the event into the event pool to process it in the same
     * processing cycle.
     *
     * The event will be processed after the current event completes.
     * Behaves identically to @ref defer_event if called while no event
     * processing takes place.
     */
    template <class Event,
              bool C = has_event_pool,
              typename = std::enable_if_t<C>>
    void enqueue_event(Event const& event)
    {
        do_defer_event(compile_policy_impl::normalize_event(event), false);
    }

    /**
     * @brief Puts the event into the event pool to process it in the next
     * processing cycle.
     *
     * The event will be processed in the next call to @ref process_event or
     * @ref process_event_pool.
     */
    template <
        class Event,
        bool C = has_event_pool,
        typename = std::enable_if_t<C>>
    void defer_event(Event const& event)
    {
        do_defer_event(compile_policy_impl::normalize_event(event),
                       this->m_machine_state == machine_state::processing);
    }

  protected:
    using composite_state_base::self;
    using composite_state_base::get_fsm_argument;

    static_assert(std::is_same_v<typename config_t::fsm_parameter, local_transition_owner> ||
                    (std::is_same_v<typename config_t::fsm_parameter, typename config_t::root_sm> &&
                     !std::is_same_v<typename config_t::root_sm, no_root_sm>),
                  "fsm_parameter must be local_transition_owner or root_sm"
                 );

    template <typename Event>
    bool is_event_deferred(const Event& event) const
    {
        return compile_policy_impl::is_event_deferred(self(), event);
    }

    // Repetition of the front-end's method definition
    // required due to above signature.
    template <typename Event, typename Fsm>
    bool is_event_deferred(const Event& event, Fsm& fsm) const
    {
        return static_cast<const front_end_t*>(this)->is_event_deferred(event,
                                                                        fsm);
    }

    // Checks if an event is an end interrupt event.
    template <typename Event>
    bool is_end_interrupt_event(const Event& event) const
    {
        return compile_policy_impl::is_end_interrupt_event(*this, event);
    }

  private:
    // Main function used internally to process events.
    template <class Event>
    process_result process_event_observed(Event const& event,
                                          detail::process_info info)
    {
        if constexpr (!std::is_same_v<observer_t, no_observer>)
        {
            this->get_observer().pre_process_event(self(), event);
        }
        const auto result = process_event_impl(event, info);
        if constexpr (!std::is_same_v<observer_t, no_observer>)
        {
            this->get_observer().post_process_event(self(), event, result);
        }
        return result;
    }

    template <class Event>
    process_result process_event_impl(Event const& event, detail::process_info info)
    {
        if (this->m_machine_state != machine_state::idle)
        {
            return process_result::discarded;
        }

        if constexpr (has_event_pool)
        {
            if (info != detail::process_info::event_pool)
            {
                // If the event is deferred in the
                // active state configuration, process it later.
                // Skip the deferral check in submachine calls, since the
                // parent has already checked and dispatched the event.
                if (info != detail::process_info::submachine_call &&
                    compile_policy_impl::try_defer_event(self(), event))
                {
                    return process_result::deferred;
                }

                // Ensure we consider an event
                // that was action-deferred in the last sequence.
                this->get_event_pool().cur_seq_cnt += 1;
            }
        }

        // Process the event.
        process_result result;
        {
            detail::process_guard guard{this->m_machine_state};
            result = do_process_event(event, info);
        }

        // After handling, look if we have more to process in the event pool
        // (but only if we're not already processing from it).
        if constexpr (has_event_pool)
        {
            if (info != detail::process_info::event_pool)
            {
                this->process_event_pool();
            }
        }

        return result;
    }

    // Core logic for event processing without exceptions, queues, etc.
    // Delegates the region dispatch to the shared composite_state core, then
    // fans out no_transition on every active state if nothing consumed it.
    template<class Event>
    process_result do_process_event(Event const& event, detail::process_info info)
    {
        process_result result = this->do_dispatch(event);

        // If the event has not been handled and we have orthogonal zones,
        // then generate an error on every active state. For events coming
        // from upper machines, do not handle but let the upper sm handle
        // the error.
        if (result == process_result::discarded &&
            !(info == detail::process_info::submachine_call))
        {
            for (const auto state_id : this->m_active_state_ids)
            {
                this->no_transition(event, this->get_fsm_argument(), state_id);
            }
        }
        return result;
    }

    template <class Event>
    void do_defer_event(const Event& event, bool next_rtc_seq)
    {
        auto& event_pool = this->get_event_pool();
        const uint16_t seq_cnt = next_rtc_seq ? event_pool.cur_seq_cnt
                                              : event_pool.cur_seq_cnt - 1;
        event_pool.events.push_back(event_pool_processor::processable_event::make(
            detail::deferred_event<Event>{self(), event, seq_cnt}));
    }

    template <class Event, class Fsm>
    void on_entry(Event const& event, Fsm& fsm)
    {
        {
            detail::process_guard guard{this->m_machine_state};

            // First set all active state ids...
            history_impl::on_entry(self(), event);

            // ... then execute each state entry.
            static_cast<front_end_t*>(this)->on_entry(event, fsm);
            typename composite_state_base::template machine_entry_visitor<Event>
                visitor{self(), event};
            history_impl::on_entry(self(), visitor);
        }

        // After handling, look if we have more to process in the event pool.
        if constexpr (has_event_pool)
        {
            this->process_event_pool();
        }
    }

    template <class TargetStates, class Event, class Fsm>
    void on_explicit_entry(Event const& event, Fsm& fsm)
    {
        {
            detail::process_guard guard{this->m_machine_state};

            // First set all active state ids...
            using state_identities =
                mp11::mp_transform<mp11::mp_identity, TargetStates>;
            static constexpr bool all_regions_defined =
                mp11::mp_size<state_identities>::value == nr_regions;
            if constexpr (!all_regions_defined)
            {
                history_impl::on_entry(self(), event);
            }
            mp11::mp_for_each<state_identities>(
                [this](auto state_identity)
                {
                    using State = typename decltype(state_identity)::type;
                    static constexpr uint8_t region_id = State::zone_index;
                    static_assert(region_id < nr_regions);
                    this->m_active_state_ids[region_id] =
                        composite_state_base::template get_state_id<State>();
                });

            // ... then execute each state entry.
            static_cast<front_end_t*>(this)->on_entry(event, fsm);
            typename composite_state_base::template machine_entry_visitor<Event>
                visitor{self(), event};
            if constexpr (all_regions_defined)
            {
                mp11::mp_for_each<state_identities>(
                    [this, &visitor](auto state_identity)
                    {
                        using State = typename decltype(state_identity)::type;
                        auto& state = this->template get_state<State>();
                        visitor(state);
                    });
            }
            else
            {
                this->template visit<visit_mode::active_non_recursive>(visitor);
            }
        }

        // After handling, look if we have more to process in the event pool.
        if constexpr (has_event_pool)
        {
            this->process_event_pool();
        }
    }

    template <class TargetStates, class Event, class Fsm>
    void on_pseudo_entry(Event const& event, Fsm& fsm)
    {
        on_explicit_entry<TargetStates>(event, fsm);

        // Execute the second part of the compound transition.
        process_event(event);
    }

    // MSCV Bug:
    // Compile error if this class is named completion_event.
    template <typename State>
    class completion_event_occurrence : public detail::event_occurrence
    {
        // Merge each list of transitions into a chain if needed.
        template <typename Transitions>
        struct merge_transitions_impl;
        template <typename Transition>
        struct merge_transitions_impl<mp11::mp_list<Transition>>
        {
            using type = Transition;
        };
        template <typename... Transitions>
        struct merge_transitions_impl<mp11::mp_list<Transitions...>>
        {
            using list = mp11::mp_list<Transitions...>;
            using completion_event =
                typename mp11::mp_first<list>::transition_event;
            using type =
                detail::transition_chain<derived_t, State, list, completion_event>;
        };
        template <typename Transitions>
        using merge_transitions =
            typename merge_transitions_impl<Transitions>::type;
        using completion_transitions =
            detail::completion_transitions<derived_t, State>;
        using completion_transition = merge_transitions<completion_transitions>;

      public:
        completion_event_occurrence(uint8_t region_id)
            : event_occurrence(
                  &try_process_thunk<completion_event_occurrence, derived_t>),
              m_region_id(region_id)
        {
        }

        std::optional<process_result> try_process(derived_t& sm,
                                                  uint16_t /*seq_cnt*/)
        {
            mark_processed();
            using completion_event =
                typename completion_transition::transition_event;
            {
                detail::process_guard guard{sm.m_machine_state};
                return completion_transition::process(sm, m_region_id,
                                                      completion_event{});
            }
        }

      private:
        uint8_t m_region_id;
    };

    class terminate_event : public detail::event_occurrence
    {
      public:
        terminate_event() noexcept
            : event_occurrence(&try_process_thunk<terminate_event, derived_t>)
        {
        }

        std::optional<process_result> try_process(derived_t& sm,
                                                  uint16_t /*seq_cnt*/)
        {
            mark_processed();
            auto root_sm = *(sm.m_root_sm);
            root_sm->m_machine_state = machine_state::terminated;
            return process_result::consumed;
        }
    };

    template <typename State>
    void on_state_entry_completed(const State&, uint8_t region_id)
    {
        // Exclude composite states from completion transitions,
        // these should fire when all their regions reach a final state
        // (and final states do not exist yet).
        if constexpr(
            !detail::is_composite<State>::value &&
            detail::has_completion_transitions<derived_t, State>::value)
        {
            auto& event_pool = this->get_event_pool();
            // Process completion transitions BEFORE any other event in the
            // pool (UML Standard 2.3 15.3.14).
            event_pool.events.push_front(
                event_pool_processor::processable_event::make(
                    completion_event_occurrence<State>{region_id}));
        }
        else if constexpr (mp11::mp_contains<detail::get_flag_list<State>,
                                             TerminateFlag>::value)
        {
            auto& event_pool = this->get_event_pool();
            event_pool.events.push_front(
                event_pool_processor::processable_event::make(terminate_event{}));
        }
    }

    template <class Event, class Fsm>
    void on_exit(Event const& event, Fsm& fsm)
    {
        {
            detail::process_guard guard{this->m_machine_state};

            // First exit the substates...
            this->template visit<visit_mode::active_non_recursive>(
                [this, &event](auto& state)
                {
                    state.on_exit(event, this->get_fsm_argument());
                });
            // ... then call our own exit.
            (static_cast<front_end_t*>(this))->on_exit(event, fsm);
        }
        this->m_machine_state = machine_state::stopped;
    }

    template <typename State, typename F, typename = void>
    struct has_reflect_member : std::false_type {};
    template <typename State, typename Visitor>
    struct has_reflect_member<State, Visitor,
        std::void_t<decltype(std::declval<State&>().reflect(std::declval<Visitor&&>()))>>
        : std::true_type {};

    template <typename State, typename Visitor, typename = void>
    struct has_reflect_free : std::false_type {};
    template <typename State, typename Visitor>
    struct has_reflect_free<State, Visitor,
        std::void_t<decltype(reflect(std::declval<State&>(), std::declval<Visitor&&>()))>>
        : std::true_type {};

    template <typename Self, typename Visitor>
    static void reflect_impl(Self& self, Visitor&& visitor)
    {
        using FrontEndRef = mp11::mp_if_c<
            std::is_const_v<Self>,
            const FrontEnd&,
            FrontEnd&>;

        auto& front_end = static_cast<FrontEndRef>(self);
        if constexpr (has_reflect_member<FrontEnd, Visitor>::value)
        {
            visitor.visit_front_end(front_end, [&front_end, &visitor]() {
                front_end.reflect(std::forward<Visitor>(visitor));
            });
        }
        else if constexpr (has_reflect_free<FrontEnd, Visitor>::value)
        {
            visitor.visit_front_end(front_end, [&front_end, &visitor]() {
                detail::invoke_reflect_free{}(front_end, std::forward<Visitor>(visitor));
            });
        }
        else
        {
            visitor.visit_front_end(front_end);
        }
        // root_sm, event pool and context cannot be serialized.
        mp11::tuple_for_each(self.m_states,
        [&visitor](auto& state)
        {
            using State = std::decay_t<decltype(state)>;

            if constexpr (has_reflect_member<State, Visitor>::value ||
                          detail::has_state_machine_tag<State>::value)
            {
                visitor.visit_state(composite_state_base::template get_state_id<State>(), state, [&state, &visitor]() {
                    state.reflect(std::forward<Visitor>(visitor));
                });
            }
            else if constexpr (has_reflect_free<State, Visitor>::value)
            {
                visitor.visit_state(composite_state_base::template get_state_id<State>(), state, [&state, &visitor]() {
                    detail::invoke_reflect_free{}(state, std::forward<Visitor>(visitor));
                });
            }
            else
            {
                visitor.visit_state(composite_state_base::template get_state_id<State>(), state);
            }
        });
        visitor.visit_member("active_state_ids", self.m_active_state_ids);
        visitor.visit_member("machine_state", self.m_machine_state);
    }

    template <typename Visitor>
    void reflect(Visitor&& visitor)
    {
        reflect_impl(*this, std::forward<Visitor>(visitor));
    }

    template <typename Visitor>
    void reflect(Visitor&& visitor) const
    {
        reflect_impl(*this, std::forward<Visitor>(visitor));
    }
};

namespace detail
{

std::false_type is_state_machine(...);

template <typename A0, typename A1, typename A2>
std::true_type is_state_machine(state_machine<A0, A1, A2>*);

template <typename T>
constexpr bool is_state_machine_v =
    decltype(is_state_machine(std::declval<T*>()))::value;

} // namespace detail

/**
 * @brief Reflects on a state_machine's members with a visitor.
 *
 * The visitor has to implement the methods:
 * - visit_front_end(auto&& front_end)
 * - visit_front_end(auto&& front_end, auto&& reflect)
 * - visit_member(const char* key, auto&& member)
 * - visit_state(size_t state_id, auto&& state)
 * - visit_state(size_t state_id, auto&& state, auto&& reflect)
 */
template <typename FrontEnd, typename Config, typename Derived,
          typename Visitor>
void reflect(state_machine<FrontEnd, Config, Derived>& sm,
             Visitor&& visitor)
{
    sm.reflect(std::forward<Visitor>(visitor));
}

/**
 * @brief Reflects on a state_machine's members with a visitor.
 *
 * The visitor has to implement the methods:
 * - visit_front_end(auto&& front_end)
 * - visit_front_end(auto&& front_end, auto&& reflect)
 * - visit_member(const char* key, auto&& member)
 * - visit_state(size_t state_id, auto&& state)
 * - visit_state(size_t state_id, auto&& state, auto&& reflect)
 */
template <typename FrontEnd, typename Config, typename Derived,
          typename Visitor>
void reflect(const state_machine<FrontEnd, Config, Derived>& sm,
             Visitor&& visitor)
{
    sm.reflect(std::forward<Visitor>(visitor));
}

} // boost::msm::backmp11

#endif // BOOST_MSM_BACKMP11_STATE_MACHINE_HPP
