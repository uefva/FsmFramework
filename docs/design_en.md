# FsmFramework Design Document

[中文版本](design.md)

This document describes the current implementation, core architecture, module responsibilities, runtime flow, test entry points, and future evolution of `FsmFramework`.

## 1. Overview

`FsmFramework` is a C++11 event-driven finite state machine framework prototype. The current code is organized as a three-layer `Cfactory_mgr -> Cfactory -> Cfsm` architecture:

- `Cfactory_mgr` is the top-level manager. It owns factory registration, the message pump, message dispatch, the worker thread, and timer entry points.
- `Cfactory` is the base FSM factory. It creates, finds, dispatches to, and reclaims FSM instances.
- `Cfsm` is the base FSM class. It stores state, lifecycle hooks, deferred message queues, and low-coupling helpers for messages and timers.

The current demo includes two business flows:

- `FAC_REG_FAC_ID -> RegFactory -> RegFsm`
- `FAC_AUTH_FAC_ID -> AuthFactory -> AuthFsm`

## 2. Repository Layout

```text
FsmFramework/
├── CMakeLists.txt
├── README.md
├── README_en.md
├── docs/
│   ├── design.md
│   └── design_en.md
├── reg_fsm/
│   ├── main.cpp
│   ├── inc/
│   │   ├── AuthFactory.h
│   │   ├── AuthFsm.h
│   │   ├── CMsg.h
│   │   ├── Cfactory.h
│   │   ├── Cfactory_mgr.h
│   │   ├── Cfsm.h
│   │   ├── FsmTableExecutor.h
│   │   ├── Logger.h
│   │   ├── RegFactory.h
│   │   ├── RegFsm.h
│   │   ├── TimerManager.h
│   │   └── common.h
│   └── src/
│       ├── AuthFactory.cpp
│       ├── AuthFsm.cpp
│       ├── CMsg.cpp
│       ├── Cfactory.cpp
│       ├── Cfactory_mgr.cpp
│       ├── Cfsm.cpp
│       ├── Logger.cpp
│       ├── RegFactory.cpp
│       ├── RegFsm.cpp
│       └── TimerManager.cpp
└── tests/
    └── framework_tests.cpp
```

Core code lives under `reg_fsm`; `tests/framework_tests.cpp` covers fast correctness tests; `benchmarks/framework_benchmark.cpp` covers framework benchmarks; `docs/design_en.md` is the English design document.

## 3. Core Types

### 3.1 Message

`CMsg` is the unified message object.

```cpp
class CMsg
{
public:
    MsgType type = MSG_INIT;
    unsigned int serviceId = 0;
    unsigned int fsmId = 0;
    unsigned int sessionId = 0;
    std::vector<char> msg;
};
```

| Field | Description |
|---|---|
| `type` | Current event type used to find an FSM transition rule. |
| `serviceId` | Target factory ID used by `Cfactory_mgr` for routing. |
| `fsmId` | Target FSM instance ID; `0` may create a new FSM by factory rules. |
| `sessionId` | Business session ID reserved for upper-layer protocols or workflow tracking. |
| `msg` | Message payload; not heavily used by the current demo. |

### 3.2 Events

```cpp
enum MsgType
{
    MSG_INIT,
    MSG_CONNECT,
    MSG_REQ,
    MSG_RESP,
    MSG_TIMEOUT,
    MSG_CLOSE
};
```

| Event | Description |
|---|---|
| `MSG_INIT` | Initialize a business flow. |
| `MSG_CONNECT` | Prepare or establish a connection. |
| `MSG_REQ` | Send or process a request. |
| `MSG_RESP` | Process a response. |
| `MSG_TIMEOUT` | Process a timeout path. |
| `MSG_CLOSE` | Close the flow and finish the FSM. |

### 3.3 States

```cpp
enum Tstate
{
    IDLE = 0,
    WORKING,
    KILL_FSM
};
```

| State | Description |
|---|---|
| `IDLE` | The FSM is created but has not entered business processing. |
| `WORKING` | The FSM is processing the business flow. |
| `KILL_FSM` | The FSM is finished and is waiting for factory cleanup. |

### 3.4 Error Codes

```cpp
enum EerrNo
{
    INIT = 0,
    SUCCESS,
    ERROR,
    INVALID_STATE,
    INVALID_MSG,
    TIMER_ERROR,
};
```

| Code | Description |
|---|---|
| `INIT` | Initial or not yet processed. |
| `SUCCESS` | Processing succeeded. |
| `ERROR` | General failure. |
| `INVALID_STATE` | The current state does not allow the operation. |
| `INVALID_MSG` | The message is invalid for the current state, or the routing target does not exist. |
| `TIMER_ERROR` | Timer operation failed. |

## 4. Architecture and Responsibilities

### 4.1 `Cfactory_mgr`

`Cfactory_mgr` is the top-level entry point. Its responsibilities are:

1. Own multiple factories through `std::unique_ptr<Cfactory>`.
2. Use the thread-safe `_pump` queue for external messages, FSM-generated messages, and timer messages.
3. Enqueue messages with `SendMsg` and wake the message pump.
4. Dispatch messages continuously in a background thread through `Run`/`Start`.
5. Support tests or single-threaded driving through `PumpOnce`/`RunUntilEmpty`.
6. Find factories by `serviceId` and call `FacMsgPrc`.
7. Provide `StartTimer`, `StopTimer`, and `StopAllTimers` through `TimerManager`.
8. Reject new messages after `Stop`, then exit the loop after the queue is drained.

### 4.2 `Cfactory`

`Cfactory` is the base FSM factory. The current version keeps common routing logic in the base class, so concrete factories usually only need to implement `CreateFsm()`.

Responsibilities:

1. Store the factory ID that matches `CMsg::serviceId`.
2. Own FSM instances through `std::unique_ptr<Cfsm>`.
3. Protect the FSM list and `_nextFsmId` with `_fsm_lock`.
4. Create a new FSM when the message is `MSG_INIT` and no existing FSM is found.
5. Return `INVALID_MSG` when a non-`MSG_INIT` message targets a missing FSM.
6. Dispatch through `PrePrcMsg`, `ProcessMsg`, and `PostPrcMsg`.
7. Reclaim an FSM with `KillFsm` after it enters `KILL_FSM`.

### 4.3 `Cfsm`

`Cfsm` is the base FSM class. It stores `fsmId`, current state, processing result, owner factory pointer, saved messages, and held messages.

Key APIs:

| API | Description |
|---|---|
| `GetState` / `SetState` | Get or set state; `SetState` triggers enter/exit hooks. |
| `PrePrcMsg` | Message pre-processing hook. |
| `ProcessMsg` | Main message processing logic. |
| `PostPrcMsg` | Message post-processing hook. |
| `Create` / `Destroy` | FSM initialization and cleanup. |
| `SendMsg` | Post a message through the owning factory's manager. |
| `StartTimer` / `StopTimer` | Operate timers through the manager. |
| `SaveMsg` / `HoldMsg` | Save or hold messages that cannot be processed immediately. |

`PrePrcMsg`, `ProcessMsg`, and `PostPrcMsg` are declared as pure virtual functions but also have default base implementations. This keeps `Cfsm` abstract while allowing derived FSMs to explicitly reuse base behavior.

### 4.4 `FsmTableExecutor`

`FsmTableExecutor.h` is the shared helper for table-driven FSM execution:

1. `FindFsmTransition` finds a transition by `current state + current event`.
2. `ExecuteFsmTransition` rejects `KILL_FSM` first.
3. It calls base `Cfsm::ProcessMsg` as the common processing entry.
4. It returns `INVALID_MSG` when no valid transition is found.
5. On success, it logs, runs the business action, changes state, and posts the next event if configured.

### 4.5 `Logger`

`Logger` is the lightweight logging module used by the project. It still uses `std::cout` as the output backend, but framework code now logs through one shared entry point. Default `INFO` logs are optimized for flow readability, while `DEBUG/WARN/ERROR` logs are used for debugging and failure location.

Current capabilities:

- Log levels: `DEBUG`, `INFO`, `WARN`, `ERROR`, and `OFF`.
- Module names: for example `Cfactory_mgr`, `Cfactory`, `RegFsm`, `AuthFsm`, and `TimerManager`.
- Millisecond timestamps shown as `HH:MM:SS.mmm`.
- `INFO` format: timestamp, level, module, and business summary only.
- `DEBUG/WARN/ERROR` format: appends file name, line number, and thread ID.
- Enum-to-string helpers: `MsgTypeToString`, `StateToString`, and `ErrNoToString`.
- Thread safety: a mutex keeps each log line intact.

Example:

```text
[15:04:36.850][INFO][RegFsm] fsm=1 event=MSG_INIT state=IDLE->WORKING next=MSG_CONNECT
[15:04:36.858][WARN][Cfactory_mgr] DispatchMsg unknown serviceId=4 event=MSG_INIT at=Cfactory_mgr.cpp:229 thread=2
```

### 4.6 `TimerManager`

`TimerManager` has been extracted from `Cfactory_mgr`. It owns timer threads and posts messages through a callback. The current implementation is still a prototype: each timer creates one thread, sleeps for `timeoutMs`, checks `active`, and posts the message if still active.

Limitations:

- Many timers create many threads.
- `StopTimer` only prevents message posting after expiration; it does not interrupt `sleep_for`.
- `StopAndJoin` must wait for already-started timer threads to finish.

### 4.7 `RegFsm` and `AuthFsm`

`RegFsm` and `AuthFsm` both derive from `Cfsm` and describe their business flows with transition tables. Each transition contains:

- source state `from`
- event `event`
- target state `to`
- whether it posts a follow-up event `hasNext`
- follow-up event `nextEvent`
- timer delay `delayMs`
- log text `log`, currently kept mainly as transition-rule documentation
- business action pointer `action`

## 5. Runtime Flow

### 5.1 High-Level Architecture

```mermaid
flowchart TD
    App["main.cpp / Application"] --> Mgr["Cfactory_mgr"]
    Mgr --> Pump["Thread-safe message pump"]
    Mgr --> Timer["TimerManager"]
    Mgr --> RegFac["RegFactory"]
    Mgr --> AuthFac["AuthFactory"]
    RegFac --> RegFsm["RegFsm instances"]
    AuthFac --> AuthFsm["AuthFsm instances"]
    RegFsm --> Mgr
    AuthFsm --> Mgr
    Timer --> Pump
```

### 5.2 Message Dispatch

```mermaid
flowchart TD
    A["SendMsg(CMsg)"] --> B["Push into _pump"]
    B --> C["Notify _pump_cv"]
    C --> D["Run() wakes up"]
    D --> E["Pop one message"]
    E --> F["DispatchMsg(msg)"]
    F --> G{"FindFactory(serviceId)"}
    G -- "found" --> H["factory->FacMsgPrc(msg)"]
    G -- "missing" --> I["return INVALID_MSG"]
    H --> J{"FindFsm(fsmId)"}
    J -- "found" --> K["DispatchToFsm"]
    J -- "missing + MSG_INIT" --> L["AddFsm"]
    J -- "missing + other event" --> M["return INVALID_MSG"]
    L --> K
    K --> N["PrePrcMsg -> ProcessMsg -> PostPrcMsg"]
    N --> O{"state == KILL_FSM?"}
    O -- "yes" --> P["KillFsm"]
    O -- "no" --> Q["wait for next message"]
```

### 5.3 Registration Flow

| Current Event | State Change | Next Event |
|---|---|---|
| `MSG_INIT` | `IDLE -> WORKING` | `MSG_CONNECT` |
| `MSG_CONNECT` | `WORKING -> WORKING` | `MSG_REQ` |
| `MSG_REQ` | `WORKING -> WORKING` | `MSG_RESP` |
| `MSG_RESP` | `WORKING -> WORKING` | `MSG_TIMEOUT` after 10 ms |
| `MSG_TIMEOUT` | `WORKING -> WORKING` | `MSG_CLOSE` |
| `MSG_CLOSE` | `WORKING -> KILL_FSM` | none |

```mermaid
stateDiagram-v2
    [*] --> IDLE
    IDLE --> WORKING: MSG_INIT / send MSG_CONNECT
    WORKING --> WORKING: MSG_CONNECT / send MSG_REQ
    WORKING --> WORKING: MSG_REQ / send MSG_RESP
    WORKING --> WORKING: MSG_RESP / timer MSG_TIMEOUT after 10 ms
    WORKING --> WORKING: MSG_TIMEOUT / send MSG_CLOSE
    WORKING --> KILL_FSM: MSG_CLOSE
    KILL_FSM --> [*]: factory recycles FSM
```

### 5.4 Authentication Flow

| Current Event | State Change | Next Event |
|---|---|---|
| `MSG_INIT` | `IDLE -> WORKING` | `MSG_CONNECT` |
| `MSG_CONNECT` | `WORKING -> WORKING` | `MSG_REQ` |
| `MSG_REQ` | `WORKING -> WORKING` | `MSG_RESP` |
| `MSG_RESP` | `WORKING -> WORKING` | `MSG_CLOSE` after 100 ms |
| `MSG_CLOSE` | `WORKING -> KILL_FSM` | none |

```mermaid
stateDiagram-v2
    [*] --> IDLE
    IDLE --> WORKING: MSG_INIT / send MSG_CONNECT
    WORKING --> WORKING: MSG_CONNECT / send MSG_REQ
    WORKING --> WORKING: MSG_REQ / send MSG_RESP
    WORKING --> WORKING: MSG_RESP / timer MSG_CLOSE after 100 ms
    WORKING --> KILL_FSM: MSG_CLOSE
    KILL_FSM --> [*]: factory recycles FSM
```

## 6. Build and Run

The root `CMakeLists.txt` defines two targets:

- `reg_fsm_demo`: demo executable.
- `reg_fsm_tests`: fast correctness test executable.
- `reg_fsm_benchmark`: framework benchmark executable.

```bash
cmake -S . -B build
cmake --build build
./build/reg_fsm_demo
ctest -C Debug --test-dir build --output-on-failure
```

On Windows Debug builds, executables are commonly located at:

```bash
./build/Debug/reg_fsm_demo.exe
./build/Debug/reg_fsm_tests.exe
./build/Debug/reg_fsm_benchmark.exe
```

## 7. Test Coverage

`tests/framework_tests.cpp` currently covers:

1. Default `CMsg` fields.
2. `RegFsm` returning `INVALID_MSG` for an invalid event in `IDLE`.
3. `AuthFsm` returning `INVALID_MSG` for an invalid event in `IDLE`.
4. A smoke test for the `Cfactory_mgr -> Cfactory -> Cfsm` pipeline.

`benchmarks/framework_benchmark.cpp` covers:

1. `noop`: single FSM no-op processing for message-pump baseline throughput.
2. `multi_fsm`: multi-FSM routing by `fsmId` for factory/FSM lookup cost.
3. `concurrent`: multiple producers calling `SendMsg` to measure queue lock contention.
4. `real_flow`: `RegFsm/AuthFsm` real flows with table execution, actions, logs, and timers.
5. `timer`: many timers created and fired for before/after timer refactor comparison.

Benchmarks are not part of the default `ctest` suite. Common commands:

```bash
./build/Debug/reg_fsm_benchmark.exe --case=all --messages=10000000 --log-level=off
./build/Debug/reg_fsm_benchmark.exe --case=noop --messages=10000
./build/Debug/reg_fsm_benchmark.exe --case=multi_fsm --messages=10000 --fsm-count=32
./build/Debug/reg_fsm_benchmark.exe --case=concurrent --messages=10000 --producers=4
./build/Debug/reg_fsm_benchmark.exe --case=timer --timers=1000 --timer-delay-ms=1
```

Output is grouped by case, with `key=value` fields on short lines so results are easier to read, copy, save as baselines, and compare across versions.

## 8. Current Limitations

| Limitation | Description |
|---|---|
| Logging is still synchronous console output | `Logger` unifies the entry point and improves default readability, but it does not yet have a file backend or async queue. |
| `TimerManager` is still a one-thread-per-timer prototype | Many timers create many threads. |
| `StopTimer` does not interrupt `sleep_for` | It only prevents posting after expiration. |
| `SaveMsg` / `HoldMsg` do not yet have a full scheduling policy | Queue helpers exist, but no resume policy is defined yet. |
| The message payload is not wrapped | `std::vector<char>` does not yet have protocol helpers or a resource pool. |
| Tests still use `assert` | No GoogleTest-style unit test framework is integrated yet. |

## 9. Recommended Evolution

Recommended next steps, ordered by risk and payoff:

1. Extend `Logger` with file output, an async queue, module filtering, and runtime configuration.
2. Upgrade `TimerManager` to a single worker with a condition variable and priority queue to avoid many timer threads.
3. Extend `FsmTableExecutor` into a fuller transition executor to reduce repeated business FSM code.
4. Introduce GoogleTest or another test framework, and split stress tests from normal unit tests.
5. Define explicit scheduling semantics for `SaveMsg` / `HoldMsg`.
6. Add payload helpers or a message buffer pool for `CMsg::msg`.
7. Generate Mermaid or DOT state diagrams from transition tables.

## 10. Adding a New Business FSM

For example, to add `LoginFsm`:

1. Add a new factory ID in `common.h`, such as `FAC_LOGIN_FAC_ID`.
2. Add `LoginFsm.h/.cpp` and derive it from `Cfsm`.
3. Add `LoginFactory.h/.cpp`, derive it from `Cfactory`, and implement `CreateFsm()`.
4. Define the transition table, business actions, and `PostNextEvent` in `LoginFsm`.
5. Reuse `ExecuteFsmTransition` to implement `ProcessMsg`.
6. Register `LoginFactory` in `main.cpp` or the application entry point.
7. Add the new source files to `CMakeLists.txt`.
8. Add tests for both the valid flow and invalid transitions.

## 11. Summary

The project now has a runnable, testable, and extensible FSM framework shape. Compared with the early demo, the latest implementation includes common factory routing, a shared transition table executor, a standalone timer manager, more specific error codes, a lightweight logger, a background message pump, and stress test entry points. The next valuable improvements are timer internals, a test framework, payload management, logging backend enhancements, and state diagram generation.
