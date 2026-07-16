// Copyright 2026 Christian Granzin
// Copyright 2008 Christophe Henry
// henry UNDERSCORE christophe AT hotmail DOT com
// This is an extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2005 David Abrahams and Aleksey Gurtovoy. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MSM_BACKMP11_DETAIL_COMPOSITE_STATE_HPP
#define BOOST_MSM_BACKMP11_DETAIL_COMPOSITE_STATE_HPP

#include <array>
#include <cstdint>
#include <optional>
#include <tuple>
#include <type_traits>

#include <boost/assert.hpp>
#include <boost/mp11.hpp>

#include <boost/msm/row_tags.hpp>
#include <boost/msm/backmp11/common_types.hpp>
#include <boost/msm/backmp11/state_machine_config.hpp>
#include <boost/msm/backmp11/detail/common.hpp>
#include <boost/msm/backmp11/detail/event_pool_processor.hpp>
#include <boost/msm/backmp11/detail/metafunctions.hpp>
#include <boost/msm/backmp11/detail/state_machine_base.hpp>
#include <boost/msm/backmp11/detail/state_tags.hpp>
#include <boost/msm/backmp11/detail/state_visitor.hpp>
#include <boost/msm/backmp11/detail/transition_table.hpp>

namespace boost::msm::backmp11
{

// A composite is entered, exited and dispatched to by its enclosing
// state_machine, which therefore needs access to the (private) methods below.
// Mirrors state_machine befriending state_machine for nested submachines.
template <class FrontEnd, class Config, class Derived>
class state_machine;

} // namespace boost::msm::backmp11

namespace boost::msm::backmp11::detail
{

/**
 * @brief The borrowed execution context of a standalone composite_state.
 *
 * Per the UML single-execution-context idea, a composite has neither its own
 * machine_state nor its own event pool: it reads the root machine's through
 * the borrow handle @ref m_root_sm, set during the root's init traversal
 * (init_state_visitor). This is the counterpart of @ref state_machine_base:
 * both expose the same accessor surface (get_context / get_observer /
 * get_machine_state / get_root_sm / get_event_pool) so the shared core in
 * @ref composite_state can call them uniformly, but this one owns nothing and
 * delegates everything to the root.
 */
template <typename Config>
class borrowed_context
{
  public:
    using config_t   = Config;
    using context_t  = typename Config::context;
    using observer_t = typename Config::observer;
    using root_sm_t  = typename Config::root_sm;

    /// The machine_state lives on the root; borrow it.
    machine_state get_machine_state() const
    {
        return (*m_root_sm)->get_machine_state();
    }

    /// The context is shared by all machines in the hierarchy; borrow the
    /// root's. Only valid with a context set.
    template <bool C = !std::is_same_v<context_t, no_context>,
              typename = std::enable_if_t<C>>
    context_t& get_context()
    {
        return (*m_root_sm)->get_context();
    }
    template <bool C = !std::is_same_v<context_t, no_context>,
              typename = std::enable_if_t<C>>
    const context_t& get_context() const
    {
        return (*m_root_sm)->get_context();
    }

    /// The observer lives on the root; a composite borrows it so its own hooks
    /// report against the shared instance. Only valid with an observer set.
    template <bool C = !std::is_same_v<observer_t, no_observer>,
              typename = std::enable_if_t<C>>
    observer_t& get_observer()
    {
        return (*m_root_sm)->get_observer();
    }
    template <bool C = !std::is_same_v<observer_t, no_observer>,
              typename = std::enable_if_t<C>>
    const observer_t& get_observer() const
    {
        return (*m_root_sm)->get_observer();
    }

    /// Gets the root machine. Only valid with a root_sm set.
    template <bool C = !std::is_same_v<root_sm_t, no_root_sm>,
              typename = std::enable_if_t<C>>
    root_sm_t& get_root_sm()
    {
        return *(*m_root_sm);
    }
    template <bool C = !std::is_same_v<root_sm_t, no_root_sm>,
              typename = std::enable_if_t<C>>
    const root_sm_t& get_root_sm() const
    {
        return *(*m_root_sm);
    }

    // The event pool lives on the root; borrow it. Only referenced from the
    // (inert under no_event_pool) completion/terminate paths for now.
    // @todo (borrow) get_event_pool is protected on the root, so instantiating
    // this for a pooled composite needs a grant; also add an enqueue/defer
    // proxy through m_root_sm.
    template <bool C = !std::is_same_v<typename Config::event_pool, no_event_pool>,
              typename = std::enable_if_t<C>>
    auto& get_event_pool()
    {
        return (*m_root_sm)->get_event_pool();
    }

  private:
    // The root handle is wired by the init traversal, read by the enclosing
    // machine's dispatch (forward to the exit pseudostate) and by the
    // composite's own terminate path. non_propagating keeps copy/move
    // assignment from carrying a stale pointer.
    template <typename>
    friend class init_state_visitor;
    template <typename StateMachine>
    friend struct transition_table_impl;
    template <typename, typename, typename, typename, typename>
    friend class composite_state;

    non_propagating<root_sm_t*> m_root_sm{nullptr};
};

/**
 * @brief A composite state: a front::state_machine_def used directly as a
 *        submachine, and the shared core of @ref state_machine.
 *
 * Follows the UML single-execution-context idea: it owns its contained states
 * but delegates its execution context to a pluggable @p Context base. A
 * standalone composite plugs in @ref borrowed_context (borrows the root's
 * machine_state / event pool); a full back::state_machine plugs in
 * @ref state_machine_base (owns them) and derives from this template, adding
 * the user-facing API, run-to-completion and history on top.
 *
 * @tparam FrontEnd  The front-end (a front::state_machine_def).
 * @tparam Config    The configuration (shared with the enclosing machine).
 * @tparam Derived   The most-derived type for CRTP; @ref no_derived selects the
 *                   composite itself (a standalone composite).
 * @tparam Context   The execution-context base (borrowed or owned).
 * @tparam Tag       The `internal::tag` (composite vs state_machine).
 */
template <typename FrontEnd, typename Config, typename Derived = no_derived,
          typename Context = borrowed_context<Config>,
          typename Tag = composite_state_tag>
class composite_state : public FrontEnd, public Context
{
    static_assert(is_composite<FrontEnd>::value,
                  "FrontEnd must be a composite state");

  public:
    /// Type of the front-end (a front::state_machine_def used directly as a
    /// submachine, i.e. without wrapping it in a back::state_machine).
    using front_end_t = FrontEnd;
    /// Type of the configuration (shared with the enclosing machine).
    using config_t = Config;
    /// Type of the context (borrowed from the root machine).
    using context_t = typename Config::context;
    /// Type of the observer (borrowed from the root machine).
    using observer_t = typename Config::observer;
    /// Type of the root machine whose execution context is borrowed.
    using root_sm_t = typename Config::root_sm;
    /// The most-derived type (CRTP); a standalone composite is its own.
    using derived_t = mp11::mp_if_c<std::is_same_v<Derived, no_derived>,
                                    composite_state, Derived>;

    /// Wrapper for an exit pseudostate,
    /// which upper machines can use to connect to it.
    template <class ExitPseudostate>
    struct exit_pt : public ExitPseudostate
    {
        // tags
        struct internal
        {
            using tag = detail::exit_pseudostate_be_tag;
        };
        using state = ExitPseudostate;
        using owner = derived_t;
        using event = typename ExitPseudostate::event;
        using forward_fn_t = void (*)(void* /*root_sm*/, const void* /*event*/);

        template <typename RootSm>
        void init()
        {
            m_forward_fn = &call_enqueue_event<RootSm, event>;
        }

        // forward event to the root sm.
        template <class ForwardEvent>
        void forward_event(void* root_sm, const ForwardEvent& forward_event)
        {
            static_assert(
                std::is_convertible_v<ForwardEvent, event>,
                "ForwardEvent must be convertible to exit pseudostate's event");
            // Call if handler set.
            // If not, this state is simply a terminate state.
            if (m_forward_fn)
            {
                m_forward_fn(root_sm, &forward_event);
            }
        }

      private:
        template <typename RootSm, typename Event>
        static void call_enqueue_event(void* root_sm, const void* event)
        {
            // root_sm points at the root's state_machine_base subobject (the
            // type m_root_sm stores), which is not at offset 0 in RootSm
            // (FrontEnd precedes the context base). Recover RootSm* through that
            // base type so the offset is re-applied before the call; a direct
            // static_cast<RootSm*> from void* would skip it. The base is
            // spelled exactly as state_machine derives it.
            using root_base = state_machine_base<
                typename RootSm::config_t,
                get_nesting_role<typename RootSm::config_t,
                                 typename RootSm::derived_t>()>;
            static_cast<RootSm*>(static_cast<root_base*>(root_sm))
                ->enqueue_event(*static_cast<const Event*>(event));
        }

        forward_fn_t m_forward_fn{};
    };

    /// Wrapper for a direct entry,
    /// which upper machines can use to connect to it.
    template <class EntryPseudostate>
    struct entry_pt : public EntryPseudostate
    {
        // tags
        struct internal
        {
            using tag = detail::entry_pseudostate_be_tag;
        };

        using state = EntryPseudostate;
        using owner = derived_t;
    };

    /// Wrapper for a direct entry,
    /// which upper machines can use to connect to it.
    template <class State>
    struct direct : public State
    {
        // tags
        struct internal
        {
            using tag = detail::explicit_entry_be_tag;
        };
        using state = State;
        using owner = derived_t;
    };

    /// Compile-time description of this composite, mirroring the `internal`
    /// struct of @ref state_machine so the existing metafunctions
    /// (generate_state_set, is_composite, the visitors, ...) treat a composite
    /// state and a back::state_machine uniformly.
    struct internal
    {
        using tag = Tag;

        using initial_states = to_mp_list_t<typename front_end_t::initial_state>;
        static constexpr auto nr_regions = mp11::mp_size<initial_states>::value;

        using state_set = generate_state_set<composite_state>;
        using state_map = generate_state_map<state_set>;
        template <typename State>
        using get_state_id = detail::get_state_id<state_map, State>;

        using submachines = mp11::mp_copy_if<state_set, is_composite>;
    };

    /// Container with all contained states. Owned by the composite.
    using states_t = mp11::mp_rename<typename internal::state_set, std::tuple>;

    /// Whether the borrowed execution context provides an event pool. Mirrors
    /// state_machine_base::has_event_pool; the pool itself lives on the root.
    static constexpr bool has_event_pool =
        !std::is_same_v<typename Config::event_pool, no_event_pool>;

  protected:
    // Shared with the derived state_machine (which keeps its owned-context
    // orchestration on top of these).
    static constexpr auto nr_regions = internal::nr_regions;
    using state_set          = typename internal::state_set;
    using state_map          = typename internal::state_map;
    using active_state_ids_t = std::array<uint16_t, nr_regions>;
    using initial_state_ids =
        mp11::mp_transform<internal::template get_state_id,
                           typename internal::initial_states>;
    using compile_policy_impl =
        detail::compile_policy_impl<typename config_t::compile_policy>;
    using event_pool_processor =
        detail::event_pool_processor<typename Config::event_pool>;

  public:
    /// Returns the compile-time id of a contained state.
    template <typename State>
    static constexpr size_t get_state_id()
    {
        using stored_state = convert_state<composite_state, State>;
        static_assert(
            mp11::mp_map_contains<typename internal::state_map, stored_state>::value,
            "The state must be contained in the composite");
        return detail::get_state_id<typename internal::state_map, stored_state>::value;
    }

    /// Returns the id of a state.
    template <typename State>
    static constexpr size_t get_state_id(const State&)
    {
        return get_state_id<State>();
    }

    /// Gets a contained state.
    template <class State>
    auto& get_state()
    {
        return std::get<
            convert_state<composite_state, std::remove_reference_t<State>>>(
            m_states);
    }

    /// Gets a contained state.
    template <class State>
    const auto& get_state() const
    {
        return std::get<
            convert_state<composite_state, std::remove_reference_t<State>>>(
            m_states);
    }

    /// Returns the active state ids of the composite.
    const active_state_ids_t& get_active_state_ids() const
    {
        return m_active_state_ids;
    }

    /// Visits the states (only active states, recursive).
    template <typename Visitor>
    void visit(Visitor&& visitor)
    {
        visit<visit_mode::active_recursive>(std::forward<Visitor>(visitor));
    }

    /// Visits the states (only active states, recursive).
    template <typename Visitor>
    void visit(Visitor&& visitor) const
    {
        visit<visit_mode::active_recursive>(std::forward<Visitor>(visitor));
    }

    /// Visits the states with a @ref visit_mode.
    template <visit_mode Mode, typename Visitor>
    void visit(Visitor&& visitor)
    {
        detail::visit_if<Mode>(self(), std::forward<Visitor>(visitor));
    }

    /// Visits the states with a @ref visit_mode.
    template <visit_mode Mode, typename Visitor>
    void visit(Visitor&& visitor) const
    {
        detail::visit_if<Mode>(self(), std::forward<Visitor>(visitor));
    }

    /// Checks whether a state is currently active.
    template <typename State>
    bool is_state_active() const
    {
        using stored_state = detail::convert_state<composite_state, State>;
        using visitor_t = detail::is_state_active_visitor<stored_state>;
        visitor_t visitor;
        detail::visit_if<visit_mode::active_recursive,
                         visitor_t::template predicate>(self(), visitor);
        return visitor.result();
    }

    /// Checks if a flag is active, using the BinaryOp (default @ref flag_or) as folding function.
    template <typename Flag, typename BinaryOp = flag_or>
    bool is_flag_active() const
    {
        using visitor_t = detail::is_flag_active_visitor<Flag, BinaryOp>;
        visitor_t visitor;
        detail::visit_if<visit_mode::active_recursive,
                         visitor_t::template predicate>(self(), visitor);
        return visitor.result();
    }

    /**
     * @brief Non-user-facing entry point to process an event inside this
     *        composite.
     *
     * Invoked from the enclosing machine's dispatch via
     * `process_event_observed(event, process_info::submachine_call)`, never
     * directly by a user. Unlike back::state_machine it neither owns a
     * machine_state nor drives run-to-completion: the root's guard is already
     * held by the enclosing dispatch.
     *
     * The observer is fired here, around this composite's dispatch, exactly as
     * a nested back::state_machine would. Both the observer and (info's) call
     * contract come from the root; @p info is always
     * process_info::submachine_call here and is unused at this level.
     */
    template <class Event>
    process_result process_event_observed(Event const& event,
                                          process_info /*info*/)
    {
        if constexpr (!std::is_same_v<observer_t, no_observer>)
        {
            this->get_observer().pre_process_event(self(), event);
        }
        const auto result = do_dispatch(event);
        if constexpr (!std::is_same_v<observer_t, no_observer>)
        {
            this->get_observer().post_process_event(self(), event, result);
        }
        return result;
    }

  protected:
    // The contained states are default-constructed; a full machine forwards
    // context/observer arguments through here to the owned Context base.
    using Context::Context;

    // The dispatch tables and the transition-table builder reach into these
    // (state_set, m_states, get_fsm_argument()), exactly as they do for
    // state_machine; grant them the same access. The enclosing state_machine
    // enters/exits this composite, nested composites reach each other, and the
    // init traversal wires m_root_sm.
    template <typename, typename>
    friend struct detail::compile_policy_impl;
    template <typename StateMachine>
    friend struct detail::transition_table_impl;
    template <typename>
    friend class detail::init_state_visitor;
    template <class, class, class>
    friend class boost::msm::backmp11::state_machine;
    template <class, class, class, class, class>
    friend class composite_state;

    // Argument forwarded to actions/guards. Mirrors state_machine: the local
    // transition owner is this composite; otherwise it is the borrowed root.
    using fsm_parameter_t = mp11::mp_if_c<
        std::is_same_v<typename config_t::fsm_parameter, local_transition_owner>,
        derived_t,
        typename config_t::root_sm>;

    const fsm_parameter_t& get_fsm_argument() const
    {
        if constexpr (std::is_same_v<typename config_t::fsm_parameter,
                                     local_transition_owner>)
        {
            return self();
        }
        else
        {
            return this->get_root_sm();
        }
    }

    fsm_parameter_t& get_fsm_argument()
    {
        return const_cast<fsm_parameter_t&>(
            static_cast<const composite_state&>(*this).get_fsm_argument());
    }

    // Dispatches an event to every region and, if unhandled, to the internal
    // table. The shared dispatch core: state_machine's do_process_event wraps
    // this with its no_transition fan-out, a composite calls it directly.
    template <class Event>
    process_result do_dispatch(Event const& event)
    {
        using dispatch_table =
            typename compile_policy_impl::template dispatch_table<derived_t,
                                                                  Event>;
        process_result result = process_result::discarded;

        for (uint8_t region_id = 0; region_id < nr_regions; ++region_id)
        {
            result |= dispatch_table::dispatch(self(), region_id, event);
        }

        if (!detail::any(result & detail::consumed_or_deferred))
        {
            result |= dispatch_table::internal_dispatch(self(), event);
        }

        return result;
    }

    template <typename Event>
    class machine_entry_visitor
    {
      public:
        machine_entry_visitor(derived_t& self, const Event& event)
            : m_self(self), m_event(event)
        {
        }

        template <typename State>
        void operator()(State& state)
        {
            if constexpr (!std::is_same_v<observer_t, no_observer>)
            {
                m_self.get_observer()
                    .template pre_process_transition<front::none, Event, State,
                                                     front::none, front::none>(
                        m_self, m_region_id);
            }
            state.on_entry(m_event, m_self.get_fsm_argument());
            m_self.template dispatch_state_entry_completed<State>(state,
                                                                  m_region_id++);
            if constexpr (!std::is_same_v<observer_t, no_observer>)
            {
                m_self.get_observer()
                    .template post_process_transition<front::none, Event, State,
                                                      front::none, front::none>(
                        m_self, m_region_id, process_result::consumed);
            }
        }

      private:
        derived_t& m_self;
        const Event& m_event;
        uint8_t m_region_id{};
    };

    // Routes to the most-derived on_state_entry_completed: a state_machine
    // hides the composite's inert version with its RTC/pool one. Kept as a
    // member of composite_state so the shared entry visitor only needs to reach
    // this (and state_machine only needs to befriend composite_state), instead
    // of the visitor itself needing access to the derived machine's internals.
    template <typename State>
    void dispatch_state_entry_completed(State& state, uint8_t region_id)
    {
        self().template on_state_entry_completed<State>(state, region_id);
    }

    // Entered from the enclosing machine's transition (call_entry) or its
    // initial-state entry. Unlike state_machine there is no own machine_state
    // to guard and no event pool to drain -- the root owns both.
    template <class Event, class Fsm>
    void on_entry(Event const& event, Fsm& fsm)
    {
        // First set each region to its initial state...
        m_active_state_ids = value_array<initial_state_ids>;

        // ... then run the front-end's own entry, then enter each region's
        // initial state.
        static_cast<front_end_t*>(this)->on_entry(event, fsm);
        machine_entry_visitor<Event> visitor{self(), event};
        mp11::mp_for_each<initial_state_ids>(
            [this, &visitor](auto state_id)
            {
                visitor(std::get<decltype(state_id)::value>(m_states));
            });
    }

    template <class TargetStates, class Event, class Fsm>
    void on_explicit_entry(Event const& event, Fsm& fsm)
    {
        // First set the targeted region(s) to the explicit state(s)...
        using state_identities =
            mp11::mp_transform<mp11::mp_identity, TargetStates>;
        static constexpr bool all_regions_defined =
            mp11::mp_size<state_identities>::value == nr_regions;
        if constexpr (!all_regions_defined)
        {
            m_active_state_ids = value_array<initial_state_ids>;
        }
        mp11::mp_for_each<state_identities>(
            [this](auto state_identity)
            {
                using State = typename decltype(state_identity)::type;
                static constexpr uint8_t region_id = State::zone_index;
                static_assert(region_id < nr_regions);
                m_active_state_ids[region_id] = get_state_id<State>();
            });

        // ... then run the front-end's own entry, then enter them.
        static_cast<front_end_t*>(this)->on_entry(event, fsm);
        machine_entry_visitor<Event> visitor{self(), event};
        if constexpr (all_regions_defined)
        {
            mp11::mp_for_each<state_identities>(
                [this, &visitor](auto state_identity)
                {
                    using State = typename decltype(state_identity)::type;
                    visitor(this->get_state<State>());
                });
        }
        else
        {
            visit<visit_mode::active_non_recursive>(visitor);
        }
    }

    template <class TargetStates, class Event, class Fsm>
    void on_pseudo_entry(Event const& event, Fsm& fsm)
    {
        on_explicit_entry<TargetStates>(event, fsm);

        // Execute the second part of the compound transition on this composite.
        process_event_observed(compile_policy_impl::normalize_event(event),
                               process_info::submachine_call);
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
            // @todo (borrow) The RTC guard belongs to the root; when completion
            // transitions on a composite are exercised, guard the root's
            // machine_state instead of an owned one.
            return completion_transition::process(sm, m_region_id,
                                                  completion_event{});
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
            (*sm.m_root_sm)->m_machine_state = machine_state::terminated;
            return process_result::consumed;
        }
    };

    template <typename State>
    void on_state_entry_completed(const State&, uint8_t region_id)
    {
        // Exclude composite states from completion transitions,
        // these should fire when all their regions reach a final state
        // (and final states do not exist yet).
        if constexpr (!detail::is_composite<State>::value &&
                      detail::has_completion_transitions<derived_t, State>::value)
        {
            // @todo (borrow) Route completion transitions through the root's
            // event pool (get_event_pool is not yet borrowed); requires an
            // event pool, so inert under no_event_pool.
            auto& event_pool = this->get_event_pool();
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
        // First exit the active substate in each region (matched by runtime id
        // -- deliberately not visit<active_non_recursive>, which reads an owned
        // machine_state the composite does not have), then the front-end's own
        // exit.
        using state_identities = mp11::mp_transform<mp11::mp_identity, state_set>;
        for (const auto active_state_id : m_active_state_ids)
        {
            mp11::mp_for_each<state_identities>(
                [&](auto state_identity)
                {
                    using State = typename decltype(state_identity)::type;
                    if (active_state_id == get_state_id<State>())
                    {
                        std::get<State>(m_states).on_exit(event,
                                                          get_fsm_argument());
                    }
                });
        }
        static_cast<front_end_t*>(this)->on_exit(event, fsm);
    }

    derived_t& self()
    {
        return *static_cast<derived_t*>(this);
    }

    const derived_t& self() const
    {
        return *static_cast<const derived_t*>(this);
    }

    // --- Owned state --------------------------------------------------------
    // A composite owns the contained states and, per region, which of them is
    // currently active. The execution context (machine_state, event pool, root
    // handle) lives in the Context base.
    states_t           m_states{};
    active_state_ids_t m_active_state_ids{value_array<initial_state_ids>};
};

// Wraps a front composite (front::state_machine_def) used directly in a
// transition table into the matching back-end composite_state, sharing the
// enclosing machine's configuration. Specializes convert_state_impl (declared
// in detail/metafunctions.hpp). It lives here, next to composite_state, so that
// header stays free of any reference to this type; this also means a translation
// unit must include this header before generating a state set that contains a
// front composite -- which it always does, since the back-end entry point
// (state_machine.hpp) includes it.
template <typename StateMachine, typename State>
struct convert_state_impl<
    StateMachine, State,
    std::enable_if_t<front::detail::has_composite_state_tag<State>::value>>
{
    using type = composite_state<State, typename StateMachine::config_t>;
};

}

#endif // BOOST_MSM_BACKMP11_DETAIL_COMPOSITE_STATE_HPP
