// Copyright 2025 Christian Granzin
// Copyright 2010 Christophe Henry
// henry UNDERSCORE christophe AT hotmail DOT com
// This is an extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2005 David Abrahams and Aleksey Gurtovoy. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// back-end
#include "BackCommon.hpp"
// front-end
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/msm/front/functor_row.hpp>

#ifndef BOOST_MSM_NONSTANDALONE_TEST
#define BOOST_TEST_MODULE defer_test
#endif
#include <boost/test/unit_test.hpp>

namespace msm = boost::msm;
namespace front = msm::front;
using front::Row;
using front::Internal;
using front::none;
using front::state_machine_def;
namespace mp11 = boost::mp11;

namespace {

struct Event1 {};
struct Event2 {};
struct SwitchToStateHandleNone {};
struct SwitchToStateHandleAll {};

struct Action
{
    template <typename Event, typename Fsm, typename SourceState, typename TargetState>
    void operator()(const Event&, Fsm& fsm, SourceState&, TargetState&)
    {
        fsm.action_calls++;
    }
};

struct StateHandleAll : front::state<> {};

struct StateHandleNone : front::state<> {};

struct StateDeferEvent1 : front::state<>
{
    using deferred_events = mp11::mp_list<Event1>;
};

struct StateDeferEvent1And2 : front::state<>
{
    using deferred_events = mp11::mp_list<Event1, Event2>;
};

struct StateMachine_ : front::state_machine_def<StateMachine_> 
{
    using initial_state =
        mp11::mp_list<StateHandleAll, StateDeferEvent1, StateDeferEvent1And2>;

    using transition_table = mp11::mp_list<
        Row<StateHandleNone , SwitchToStateHandleAll  , StateHandleAll  , none  >,
        Row<StateHandleAll  , SwitchToStateHandleNone , StateHandleNone , none  >,
        Row<StateHandleAll  , Event1                  , none            , Action>,
        Row<StateHandleAll  , Event2                  , none            , Action>
    >;

    size_t action_calls{};
};

// Pick a back-end
using Fsms = mp11::mp_list<
#ifndef BOOST_MSM_TEST_SKIP_BACKMP11
    msm::backmp11::state_machine_adapter<StateMachine_>
#endif // BOOST_MSM_TEST_SKIP_BACKMP11
    // msm::back::state_machine<StateMachine_>
    // back11 requires a const boost::any overload to identify the Kleene event.
    // Leave it out of this test to ensure backwards compatibility.
    // msm::back11::state_machine<Front>
    >;


// TODO:
// Test case where an event is deferred in both regions.
BOOST_AUTO_TEST_CASE_TEMPLATE(defer_test, Fsm, Fsms)
{     
    Fsm fsm;

    fsm.start();
    
    fsm.process_event(Event1{});
    BOOST_REQUIRE(fsm.get_deferred_events_queue().size() == 2);
    fsm.process_event(Event2{});
    BOOST_REQUIRE(fsm.get_deferred_events_queue().size() == 3);

    // fsm.process_event(Event1{});

    fsm.stop();
}

} // namespace
