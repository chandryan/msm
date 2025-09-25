# msm
Boost.org msm module
[Please look at the generated documentation](http://htmlpreview.github.com/?https://github.com/boostorg/msm/blob/master/doc/HTML/index.html)
You might need to force browser reloading (F5)

# Brainstorming "MSM2"

Idea:
Provide a new version of MSM in which all legacy code is removed and only the recent and most commonly used features from MSM are available.

It concentrates on the provision of the following feature scope from MSM:
- Backend: backmp11
- Frontend: Functor and PUML

The minimum targeted version for MSM2 is C++17 for basic support, C++20 for enhanced features.


## Updated dependencies to other boost components

Removed dependencies:
- boost::any is replaced by std::any
- boost::bind is replaced by std::bind (or lambdas where possible)
- boost::circular_buffer is removed (queue container policy is still available, a circular_buffer policy can be implemented by the user if needed)
- boost::core is removed since it's not used
- boost::function is replaced by std::function
- boost::mpl is replaced by boost::mp11
- boost::parameter is replaced by a config structure, which bundles all parameters (more info below)
- boost::phoenix is removed because eUML support is removed
- boost::preprocessor is removed because related functions are implemented with variadic templates
- boost::proto is removed because eUML support is removed
- boost::tuple is removed because it was not used
- boost::type_traits is replaced by type traits from std
- boost::typeof is removed because eUML support is removed


Remaining dependencies:
- boost::config
- boost::fusion (as long as PUML needs it)
- boost::mp11
- boost::serialization


## Refurbished documentation

The documentation is written with XXX (tbd) instead of docbook.

Desired features for the documentation tooling:

- Format is something more human-friendly to edit in text like AsciiDoc, RST or MarkDown.
- Sources can be easily included through directives; best case the machines of the tests can be used as tests in the CI and examples in the docu


## Removed functions & features

### Support for C++03 (MSM's `back`) resp. C++11 (MSM's `back11` / `backmp11`)

Backend:
Minimum version is C++17, only one backend in namespace `back` remains (a descendant of MSM's `backmp11` backend).

Frontend:
Minimum version is C++17 for the functor frontend, C++20 for the PUML frontend.


### eUML

Too macro-heavy, difficult to use and induces header dependency to boost::proto in the backend, even if eUML is not used.
Use the PUML frontend for UML-like syntax in the frontend.


### FSM checks for orthogonality and unreachable states

Too rarely used and would require a complete rework of the related code to migrate from MPL to Mp11.


### sm_ptr

Not needed with the functor frontend and was already deprecated in MSM, thus removed in MSM2.


## Reworked/Enhanced features


### Simplified API for actions and guards in the functor frontend

The functor frontend supports an alternative, simplified API, which omits SourceState and TargetState. If an action or guard defines both APIs, the complex API is invoked. The following APIs are then available:

- Complex API: void operator()(const auto& event, auto& fsm, auto& source_state, auto& target_state)
- Simple API: void operator()(const auto& event, auto& fsm)

This removes the need for boilerplate in user code if the source and target state are not required. It also enables re-usage of action functors in states' on_entry and on_exit methods.


### History states

The histoy states of a state machine are defined in the frontend instead of the backend (as they belong to its behavior).
The history states can be defined via `using history_states = mp11::mp_list<...>;`.


### Defining parameters for the backend

All parameters are grouped in and passed via a `StateMachineConfig` struct. Motivation:
- Removal of dependency to boost::parameter
- Easier to define one config and use the same for multiple state machines
- Easier to add more parameters for new feature support in the future

Example for such a config:

```
struct my_state_machine_config
{
	using compile_policy = ...; // Defaults to favor_runtime_speed if not set.
	using queue_container_policy = ...; // Defaults to queue_container_deque if not set.
	using context = ...; // Defaults to no_context if not set.
};
```

**TODO:**
One alternative to be investigated could be requiring the user to derive from the backend and define using directives within it.


### Referring to composite states in the frontend

For the sake of a cleaner separation between backend and frontend:
If a composite state is used in a frontend's transition table, its frontend has to be referred to instead of its backend.
The backend of the upper SM automatically instantiates the backends of the sub-SMs with the same `StateMachineConfig` which was used to instantiate the upper SM.


### Visitor API

The need to define a BaseState, accept_sig and accept method in the frontend is removed.

Instead there is a generic API that supports two overloads via tag dispatch to either iterate over only active states or all states:
- void state_machine::visit(F&& f, back::active_states_t)
- void state_machine::visit(F&& f, back::all_states_t)

The functor f needs to fulfill the API `void (auto& state)`.

Also these bugs are fixed:
- If the SM is not started yet, no active state is visited instead of the initial state(s)
- If the SM is stopped, no active state is visited instead of the last active state(s)


## New features

### Fork and join pseudo states within a SM

The frontend supports the definition of fork and join pseudo states:

A fork can be defined via a target state `fork_pseudo_state<TargetStates...>`.
When a transition with a fork pseudo state as target gets executed, all target states listed in the fork pseudo state get activated and the source state gets retired.

A join can be defined via a target state such as `join_pseudo_state<TargetState, SourceStates...>`.
When a transition with a join target state gets executed, the source state gets retired.
When all source states of the join pseudo state get retired, the target state gets activated.


### Context support

The backend supports the definition of a context to support use cases such as dependency injection or getting a reference to the upper SM in hierarchical state machines.

An `context` parameter can be defined in a state_machine_config to pass a context:
- 'using context = my_context;'


### Interceptor support

The backend supports the definition of interceptors with certain hooks to support use cases such as logging instrumentation.

An `interceptor` parameter can be defined in a state_machine_config to pass one or more interceptors:
- 'using interceptor = my_logging_interceptor;'
- 'using interceptor = mp11::mp_list<my_logging_interceptor, my_metrics_interceptor>;'

The following hooks can be implemented in an interceptor:
- void pre_process_event(const auto& event)
- void post_process_event(const auto& event)
- void pre_state_entry(const auto& event, auto& fsm)
- void post_state_entry(const auto& event, auto& fsm)
- void pre_state_exit(const auto& event, auto& fsm)
- void post_state_exit(const auto& event, auto& fsm)
- void pre_execute_action(const auto& event, auto& fsm, auto& source_state, auto& target_state)
- void post_execute_action(const auto& event, auto& fsm, auto& source_state, auto& target_state)
- void pre_execute_guard(const auto& event, auto& fsm, auto& source_state, auto& target_state)
- void post_execute_guard(const auto& event, auto& fsm, auto& source_state, auto& target_state)

Tbd if it makes sense to add separate hooks for "composite_state_entry/composite_state_exit".

Tbd if technically feasible:
The interceptor supports an advanced API to manipulate the user-facing API for calling actions and guards.
This could be used for example to pass the upper SM or the SM's context instead of the current SM.
