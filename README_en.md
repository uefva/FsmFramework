# FsmFramework

[中文版本](README.md)

`FsmFramework` is a C++11 event-driven finite state machine framework prototype. The current version has evolved from a single FSM demo into a three-layer `Cfactory_mgr -> Cfactory -> Cfsm` architecture, with registration and authentication flows as examples.

```text
Cfactory_mgr
  -> RegFactory  -> RegFsm
  -> AuthFactory -> AuthFsm
```

## Current Capabilities

- Three-layer dispatch model: `Cfactory_mgr` owns the message pump and factory registry, `Cfactory` owns FSM lifecycle, and `Cfsm` defines the shared FSM interface.
- Two business FSMs: `RegFsm` for registration and `AuthFsm` for authentication.
- Thread-safe message pump: `SendMsg`, `PumpOnce`, `Run`, `RunUntilEmpty`, `Start`, `Stop`, and `Join`.
- Factory routing by `CMsg::serviceId`.
- FSM routing by `CMsg::fsmId`; `MSG_INIT` may create a new FSM.
- Automatic lifecycle management: factories own FSMs with `std::unique_ptr`, and reclaim them after they enter `KILL_FSM`.
- Table-driven transitions: `RegFsm` and `AuthFsm` share `FsmTableExecutor` for transition lookup and execution.
- Timer module: `TimerManager` posts normal `CMsg` events back to the manager through a callback.
- Low-coupling FSM helpers: business FSMs post follow-up events through `Cfsm::SendMsg`, `StartTimer`, and `StopTimer`.
- State enter/exit hooks: `OnEnterState` and `OnExitState`.
- Deferred message queues: `Cfsm::_save`, `Cfsm::_hold`, and their pop helpers.
- More specific error codes: `SUCCESS`, `ERROR`, `INVALID_STATE`, `INVALID_MSG`, and `TIMER_ERROR`.
- CMake build entry and a `reg_fsm_tests` test executable.

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Run the Demo

```bash
./build/reg_fsm_demo
```

Common Windows path:

```bash
./build/Debug/reg_fsm_demo.exe
```

## Run Tests

```bash
cmake --build build --target test
```

You can also run the test executable directly:

```bash
./build/reg_fsm_tests
```

Common Windows path:

```bash
./build/Debug/reg_fsm_tests.exe
```

> Note: the current test program includes stress tests with 10,000,000 messages, so it can take noticeably longer than a small smoke test.

## Example Flows

Registration flow `FAC_REG_FAC_ID -> RegFactory -> RegFsm`:

```text
MSG_INIT
  -> MSG_CONNECT
  -> MSG_REQ
  -> MSG_RESP
  -> MSG_TIMEOUT   (10 ms timer)
  -> MSG_CLOSE
  -> KILL_FSM
```

Authentication flow `FAC_AUTH_FAC_ID -> AuthFactory -> AuthFsm`:

```text
MSG_INIT
  -> MSG_CONNECT
  -> MSG_REQ
  -> MSG_RESP
  -> MSG_CLOSE     (100 ms timer)
  -> KILL_FSM
```

See [docs/design.md](docs/design.md) for the detailed design document.
