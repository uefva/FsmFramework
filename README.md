# FsmFramework

一个 C++ 事件驱动有限状态机框架原型。

当前版本已经从单个 FSM demo 演进为 Manager-Factory-FSM 三层结构：

```text
Cfactory_mgr
  -> Cfactory / RegFactory
      -> Cfsm / RegFsm
```

## 已实现能力

- 多层框架结构：`Cfactory_mgr`、`Cfactory`、`Cfsm`
- 线程安全消息泵：`SendMsg`、`Run`、`Stop`、`RunUntilEmpty`
- 自管理后台线程：`Start`、`Stop`、`Join`
- Factory 路由：按 `serviceId` 找到目标工厂
- FSM 路由：按 `fsmId` 找到目标状态机实例
- FSM 生命周期管理：创建、分发、结束后回收
- 定时器事件：`StartTimer` 到期后投递普通 `CMsg`
- 表驱动状态转移：`RegFsm` 使用转移表替代大段 `switch`
- 低耦合 FSM 接口：业务 FSM 通过 `Cfsm::SendMsg` / `StartTimer` 投递事件
- 状态进入/退出钩子：`OnEnterState`、`OnExitState`
- 工厂 FSM 列表锁保护
- 消息暂存队列：`Cfsm::_save`、`Cfsm::_hold`
- CMake 构建入口
- 基础测试入口：`reg_fsm_tests`

## 构建

```bash
cmake -S . -B build
cmake --build build
```

## 运行 demo

```bash
./build/reg_fsm_demo
```

Windows 下可执行文件路径可能是：

```bash
./build/Debug/reg_fsm_demo.exe
```

## 运行测试

```bash
./build/reg_fsm_tests
```

Windows 下可执行文件路径可能是：

```bash
./build/Debug/reg_fsm_tests.exe
```

## 当前示例流程

`RegFsm` 当前注册流程如下：

```text
MSG_INIT
  -> MSG_CONNECT
  -> MSG_REQ
  -> MSG_RESP
  -> MSG_TIMEOUT
  -> MSG_CLOSE
  -> KILL_FSM
```

其中 `MSG_RESP -> MSG_TIMEOUT` 通过定时器触发，定时器到期后向 `Cfactory_mgr` 消息泵投递 `MSG_TIMEOUT`。

## 文档

详细设计说明见：

```text
docs/design.md
```
