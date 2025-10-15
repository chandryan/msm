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
#include <boost/msm/backmp11/state_machine.hpp>
//front-end
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/msm/front/functor_row.hpp>

#include <boost/msm/back/queue_container_circular.hpp>
#include <boost/msm/back/history_policies.hpp>

#ifndef BOOST_MSM_NONSTANDALONE_TEST
#define BOOST_TEST_MODULE backmp11_upper_fsm_test
#endif
#include <boost/test/unit_test.hpp>

namespace msm = boost::msm;
namespace mp11 = boost::mp11;

using namespace msm::front;
using namespace msm::backmp11;

// Definitions to choose between back and front.
#define LOWER_MACHINE_BE LowerMachine
#define MIDDLE_MACHINE_BE MiddleMachine
#define LOWER_MACHINE_FE LowerMachine_
#define MIDDLE_MACHINE_FE MiddleMachine_

// To define what we use in the transition table.
#define LOWER_MACHINE LowerMachine
#define MIDDLE_MACHINE MiddleMachine

namespace
{
    // Events.
    struct EnterSubFsm{};
    struct ExitSubFsm{};

    // States.
    struct Default : public state<>{};
    struct UpperMachine_;
    using UpperMachine = state_machine<UpperMachine_>;
    struct MiddleMachine_;
    using MiddleMachine = state_machine<MiddleMachine_>;

    template<typename T, typename ExpectedFsm>
    struct MachineBase_ : public state_machine_def<MachineBase_<T, ExpectedFsm>>
    {
        template <typename Event, typename Fsm>
        void on_entry(const Event& /*event*/, Fsm& /*fsm*/)
        {
            static_assert(std::is_same_v<Fsm, ExpectedFsm>);
            machine_entries++;
        };

        template <typename Event, typename Fsm>
        void on_exit(const Event& /*event*/, Fsm& /*fsm*/)
        {
            static_assert(std::is_same_v<Fsm, ExpectedFsm>);
            machine_exits++;
        };

        using initial_state = Default;

        uint32_t machine_entries = 0;
        uint32_t machine_exits = 0;
    };

    struct LowerMachine_ : public MachineBase_<LowerMachine_, MiddleMachine>
    {
    };
    using LowerMachine = state_machine<LowerMachine_>;

    struct MiddleMachine_ : public MachineBase_<MiddleMachine_, UpperMachine>
    {
        using transition_table = mp11::mp_list<
            Row< Default       , EnterSubFsm , LOWER_MACHINE >,
            Row< LOWER_MACHINE , ExitSubFsm  , Default       >
        >;
    };

    struct UpperMachine_ : public MachineBase_<UpperMachine_, UpperMachine>
    {
        using transition_table = mp11::mp_list<
            Row< Default        , EnterSubFsm , MIDDLE_MACHINE >,
            Row< MIDDLE_MACHINE , ExitSubFsm  , Default        >
        >;
    };

    BOOST_AUTO_TEST_CASE( backmp11_upper_fsm_test )
    {
        UpperMachine upper_machine{};

        [[maybe_unused]] MIDDLE_MACHINE_FE& middle_machine_fe = upper_machine.get_state<MIDDLE_MACHINE_FE>();

        [[maybe_unused]] MIDDLE_MACHINE_BE& middle_machine_be = upper_machine.get_state<MIDDLE_MACHINE_FE>(back_end);

        [[maybe_unused]] LOWER_MACHINE_FE& lower_machine_fe = middle_machine_be.get_state<LOWER_MACHINE_FE>();

        // TODO:
        // Look into which Fsm is passed when to SM entry/exit actions.

        upper_machine.start(); 
        BOOST_CHECK_MESSAGE(upper_machine.machine_entries == 1, "SM entry not called correctly");

        upper_machine.process_event(EnterSubFsm()); 
        BOOST_CHECK_MESSAGE(middle_machine_fe.machine_entries == 1, "SM entry not called correctly");

        upper_machine.process_event(EnterSubFsm()); 
        BOOST_CHECK_MESSAGE(lower_machine_fe.machine_entries == 1, "SM entry not called correctly");

        upper_machine.process_event(ExitSubFsm()); 
        BOOST_CHECK_MESSAGE(lower_machine_fe.machine_exits == 1, "SM exit not called correctly");

        upper_machine.process_event(ExitSubFsm()); 
        BOOST_CHECK_MESSAGE(middle_machine_fe.machine_exits == 1, "SM exit not called correctly");

        upper_machine.stop(); 
        BOOST_CHECK_MESSAGE(upper_machine.machine_exits == 1, "SM exit not called correctly");
    }
}

