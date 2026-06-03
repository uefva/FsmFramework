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
- Lightweight logging module: `Logger` supports levels, modules, and timestamps; default `INFO` logs show flow summaries, while `DEBUG/WARN/ERROR` add source location details.
- CMake build entry, a fast `reg_fsm_tests` test executable, and a `reg_fsm_benchmark` benchmark executable.

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
ctest -C Debug --test-dir build --output-on-failure
```

You can also run the test executable directly:

```bash
./build/reg_fsm_tests
```

Common Windows path:

```bash
./build/Debug/reg_fsm_tests.exe
```

## Run Benchmarks

`reg_fsm_benchmark` validates message-pump throughput, FSM routing, concurrent sending, real flows, and timer behavior. It is not part of the default test suite.

```bash
./build/Debug/reg_fsm_benchmark.exe --case=all --messages=10000000 --log-level=off
```

Common smoke commands:

```bash
./build/Debug/reg_fsm_benchmark.exe --case=noop --messages=10000
./build/Debug/reg_fsm_benchmark.exe --case=multi_fsm --messages=10000 --fsm-count=32
./build/Debug/reg_fsm_benchmark.exe --case=concurrent --messages=10000 --producers=4
./build/Debug/reg_fsm_benchmark.exe --case=timer --timers=1000 --timer-delay-ms=1
```

Benchmark output is grouped by case, with `key=value` fields on short lines for easier reading, copying, and baseline storage.

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

Default logs focus on FSM flow summaries:

```text
[15:04:36.850][INFO][RegFsm] fsm=1 event=MSG_INIT state=IDLE->WORKING next=MSG_CONNECT
[15:04:36.858][WARN][Cfactory_mgr] DispatchMsg unknown serviceId=4 event=MSG_INIT at=Cfactory_mgr.cpp:229 thread=2
```

See [docs/design.md](docs/design.md) for the detailed design document.
