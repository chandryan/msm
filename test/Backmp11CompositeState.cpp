// Copyright 2026 Christian Granzin
// Copyright 2010 Christophe Henry
// henry UNDERSCORE christophe AT hotmail DOT com
// This is an extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2005 David Abrahams and Aleksey Gurtovoy. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Iteration scaffold for the draft backmp11 composite_state: a front-end
// state_machine_def is wrapped *manually* into a back::composite_state (instead
// of a back::state_machine) and used directly as a submachine in the root
// machine's transition table.
//
// TODO (composite_state follow-ups, currently deferred):
// - Active-mode recursion into a composite (is_state_active<InnerState>(),
//   visit() reaching inside): the active-mode visitor reads an owned
//   m_machine_state, which a composite borrows. Needs the visitor to use
//   get_machine_state() before this can be exercised.
// - Event deferral through the borrowed root event pool.
// - Completion transitions / terminate inside a composite (needs an event
//   pool; the code paths exist but are inert under no_event_pool).
// - reflect()/serialization parity (state_machine::reflect writes machine_state,
//   which the composite does not own).

#ifndef BOOST_MSM_NONSTANDALONE_TEST
#define BOOST_TEST_MODULE backmp11_composite_state_test
#endif
#include <boost/test/unit_test.hpp>

// back-end
#include <boost/msm/backmp11/state_machine.hpp>
#include <boost/msm/backmp11/detail/composite_state.hpp>
// front-end
#include "FrontCommon.hpp"

#include "Utils.hpp"

namespace msm = boost::msm;
namespace mp11 = boost::mp11;

using namespace msm::front;
using namespace msm::backmp11;

namespace
{

// Events.
struct EnterComposite{};
struct TriggerInner{};

// States.
struct Idle : public test::StateBase{};
struct InnerState : public test::StateBase{};

// Actions.
struct InnerAction
{
    template <typename Event, typename Fsm, typename Source, typename Target>
    void operator()(const Event&, Fsm&, Source& source, Target&)
    {
        source.action_counter++;
    }
};

struct Root_;

struct default_config : state_machine_config
{
    using root_sm    = state_machine<Root_, default_config>;
    using event_pool = no_event_pool;
};

struct Composite : test::StateMachineBase_<Composite>
{
    using initial_state = InnerState;
    using transition_table = mp11::mp_list<
        Row<InnerState, TriggerInner, none, InnerAction, none>
    >;
};

struct Root_ : test::StateMachineBase_<Root_>
{
    using initial_state = Idle;
    using transition_table = mp11::mp_list<
        Row<Idle, EnterComposite, Composite, none, none>
    >;
};

using StateMachine = state_machine<Root_, default_config>;

BOOST_AUTO_TEST_CASE(enter_composite_and_process_inner_event)
{
    StateMachine sm;

    sm.start();
    BOOST_REQUIRE(sm.entry_counter == 1);

    sm.process_event(EnterComposite{});
    BOOST_REQUIRE(sm.is_state_active<Composite>());

    BOOST_REQUIRE(sm.process_event(TriggerInner{}) ==
                  process_result::consumed);

    auto& composite = sm.get_state<Composite>();
    BOOST_REQUIRE(composite.template get_state<InnerState>().action_counter == 1);

    sm.stop();
}

} // namespace
