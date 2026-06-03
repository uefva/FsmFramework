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
- 轻量日志模块：`Logger` 支持级别、模块和时间戳；默认 `INFO` 输出流程摘要，`DEBUG/WARN/ERROR` 追加定位信息。
- CMake 构建入口、`reg_fsm_tests` 快速测试入口和 `reg_fsm_benchmark` 压测入口。

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
ctest -C Debug --test-dir build --output-on-failure
```

也可以直接运行测试程序：

```bash
./build/reg_fsm_tests
```

Windows 下常见路径：

```bash
./build/Debug/reg_fsm_tests.exe
```

## 运行压测

`reg_fsm_benchmark` 用于验证框架消息泵、FSM 路由、并发投递、真实流程和定时器能力，不加入默认测试。

```bash
./build/Debug/reg_fsm_benchmark.exe --case=all --messages=10000000 --log-level=off
```

常用 smoke 命令：

```bash
./build/Debug/reg_fsm_benchmark.exe --case=noop --messages=10000
./build/Debug/reg_fsm_benchmark.exe --case=multi_fsm --messages=10000 --fsm-count=32
./build/Debug/reg_fsm_benchmark.exe --case=concurrent --messages=10000 --producers=4
./build/Debug/reg_fsm_benchmark.exe --case=timer --timers=1000 --timer-delay-ms=1
```

输出按 case 分块展示，每行保留 `key=value` 字段，方便阅读、复制和保存为 baseline。

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

默认日志输出以 FSM 流程摘要为主：

```text
[15:04:36.850][INFO][RegFsm] fsm=1 event=MSG_INIT state=IDLE->WORKING next=MSG_CONNECT
[15:04:36.858][WARN][Cfactory_mgr] DispatchMsg unknown serviceId=4 event=MSG_INIT at=Cfactory_mgr.cpp:229 thread=2
```

详细设计说明见 [docs/design.md](docs/design.md)。
