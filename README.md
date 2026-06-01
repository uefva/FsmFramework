# FsmFramework

[English version](README_en.md)

`FsmFramework` 是一个 C++11 事件驱动有限状态机框架原型。当前版本已经从单个 FSM demo 演进为 `Cfactory_mgr -> Cfactory -> Cfsm` 三层结构，并包含注册、认证两条示例业务流程。

```text
Cfactory_mgr
  -> RegFactory  -> RegFsm
  -> AuthFactory -> AuthFsm
```

## 当前能力

- 三层调度结构：`Cfactory_mgr` 负责消息泵和工厂注册，`Cfactory` 负责 FSM 生命周期，`Cfsm` 负责状态机公共接口。
- 两套业务状态机：注册流程 `RegFsm`，认证流程 `AuthFsm`。
- 线程安全消息泵：`SendMsg`、`PumpOnce`、`Run`、`RunUntilEmpty`、`Start`、`Stop`、`Join`。
- Factory 路由：按 `CMsg::serviceId` 找到目标工厂。
- FSM 路由：按 `CMsg::fsmId` 找到目标状态机；`MSG_INIT` 可创建新 FSM。
- 自动生命周期管理：工厂使用 `std::unique_ptr` 持有 FSM，FSM 进入 `KILL_FSM` 后由工厂回收。
- 表驱动状态转移：`RegFsm` 和 `AuthFsm` 复用 `FsmTableExecutor` 查找并执行转移规则。
- 定时器模块：`TimerManager` 到期后通过回调向 manager 投递普通 `CMsg`。
- 低耦合 FSM 接口：业务 FSM 通过 `Cfsm::SendMsg`、`StartTimer`、`StopTimer` 投递后续事件。
- 状态进入/退出钩子：`OnEnterState`、`OnExitState`。
- 消息暂存队列：`Cfsm::_save`、`Cfsm::_hold` 及对应 pop 接口。
- 更细粒度错误码：`SUCCESS`、`ERROR`、`INVALID_STATE`、`INVALID_MSG`、`TIMER_ERROR`。
- CMake 构建入口和 `reg_fsm_tests` 测试入口。

## 构建

```bash
cmake -S . -B build
cmake --build build
```

## 运行 demo

```bash
./build/reg_fsm_demo
```

Windows 下常见路径：

```bash
./build/Debug/reg_fsm_demo.exe
```

## 运行测试

```bash
cmake --build build --target test
```

也可以直接运行测试程序：

```bash
./build/reg_fsm_tests
```

Windows 下常见路径：

```bash
./build/Debug/reg_fsm_tests.exe
```

> 注意：当前测试包含 10,000,000 条消息级别的压力测试，运行时间会明显长于普通 smoke test。

## 示例流程

注册流程 `FAC_REG_FAC_ID -> RegFactory -> RegFsm`：

```text
MSG_INIT
  -> MSG_CONNECT
  -> MSG_REQ
  -> MSG_RESP
  -> MSG_TIMEOUT   (10 ms timer)
  -> MSG_CLOSE
  -> KILL_FSM
```

认证流程 `FAC_AUTH_FAC_ID -> AuthFactory -> AuthFsm`：

```text
MSG_INIT
  -> MSG_CONNECT
  -> MSG_REQ
  -> MSG_RESP
  -> MSG_CLOSE     (100 ms timer)
  -> KILL_FSM
```

详细设计说明见 [docs/design.md](docs/design.md)。
