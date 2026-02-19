// Copyright 2010 Christophe Henry
// henry UNDERSCORE christophe AT hotmail DOT com
// This is an extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2005 David Abrahams and Aleksey Gurtovoy. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MSM_NONSTANDALONE_TEST
#define BOOST_TEST_MODULE orthogonal_deferred4_test
#endif
#include <boost/test/unit_test.hpp>

// back-end
#include "BackCommon.hpp"
//front-end
#include "FrontCommon.hpp"

namespace mpl = boost::mpl;
using namespace boost::msm::front;

namespace
{

// Events
struct Ping {};

// Actions
struct Action
{
    template <typename Event, typename Fsm, typename Source, typename Target>
    void operator()(const Event&, Fsm& fsm, Source&, Target&)
    {
        fsm.action_counter++;
    }
};

// Guards
struct Conditional
{
    template <typename Event, typename Fsm, typename Source, typename Target>
    bool operator()(const Event&, Fsm& fsm, Source&, Target&)
    {
        return fsm.conditional;
    }
};

struct Machine_ : test::StateMachineBase_<Machine_>
{
    typedef int activate_deferred_events;

    struct Waiting : state<> {};
    struct Ready   : state<> {};
    struct Sink    : state<> {};

    // Two orthogonal regions
    typedef mpl::vector<Waiting, Sink> initial_state;

    using transition_table = mpl::vector<
        //        Source   Event  Target  Action   Guard
        Row<      Waiting, Ping,  none,   Defer ,  none>,       // region 1: defer
        Row<      Sink,    Ping,  none,   Action,  Conditional> // region 2: handle conditionally
    >;

    size_t action_counter{};
    bool conditional{};
};

// Back contains the suggested fix, back11 contains the current implementation.
using test_machines = mpl::vector<boost::msm::back::state_machine<Machine_>,
                                  boost::msm::back11::state_machine<Machine_>>;

BOOST_AUTO_TEST_CASE_TEMPLATE(guard_reject_test, test_machine, test_machines)
{
    test_machine sm;

    sm.start(); 

    // Protection with a guard works for both implementations.
    sm.process_event(Ping{});
    std::cout << sm.action_counter << std::endl;
    BOOST_REQUIRE(sm.action_counter == 0);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(handle_discard_test, test_machine, test_machines)
{
    test_machine sm;

    sm.start();
    sm.conditional = true;

    // - suggested fix: action_counter is 2 instead of 1
    // - current implementation: segfault
    sm.conditional = true;
    sm.process_event(Ping{});
    std::cout << sm.action_counter << std::endl;
    BOOST_REQUIRE(sm.action_counter == 1);
}

} // nmespace
