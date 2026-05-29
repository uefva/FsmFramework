# FsmFramework 设计文档

## 1. 文档目的

本文档用于说明 `FsmFramework` 项目的当前代码结构、核心设计思路、状态机运行流程、主要类职责、接口约定、当前限制以及后续演进方向。

当前项目已经具备一个有限状态机框架原型的基本结构：

- 有统一的状态机基类；
- 有独立的消息对象；
- 有注册和认证两套具体业务状态机；
- 有 manager/factory/fsm 三层调度结构；
- 有示例入口演示消息路由、状态转移和错误路径。

本文档既描述“现在代码是怎么工作的”，也给出“后续如果要把它变成更通用框架，可以怎么演进”的建议。

## 2. 项目概述

`FsmFramework` 是一个 C++ 事件驱动有限状态机框架原型。当前代码已经具备 `Cfactory_mgr -> Cfactory -> Cfsm` 三层调度结构，并实现了注册流程和认证流程两条业务链路。

示例程序会注册两个工厂：

- `FAC_REG_FAC_ID -> RegFactory -> RegFsm`
- `FAC_AUTH_FAC_ID -> AuthFactory -> AuthFsm`

外部向 `Cfactory_mgr` 投递 `CMsg`，manager 按 `serviceId` 找到目标工厂，factory 按 `fsmId` 找到或创建 FSM。FSM 进入 `KILL_FSM` 后由 factory 统一回收。

当前核心代码位于 `reg_fsm` 目录：

```text
FsmFramework/
├── README.md
└── reg_fsm/
    ├── main.cpp
    ├── inc/
    │   ├── CMsg.h
    │   ├── AuthFactory.h
    │   ├── AuthFsm.h
    │   ├── Cfactory.h
    │   ├── Cfactory_mgr.h
    │   ├── Cfsm.h
    │   ├── RegFactory.h
    │   ├── RegFsm.h
    │   └── common.h
    └── src/
        ├── AuthFactory.cpp
        ├── AuthFsm.cpp
        ├── CMsg.cpp
        ├── Cfactory.cpp
        ├── Cfactory_mgr.cpp
        ├── Cfsm.cpp
        ├── RegFactory.cpp
        └── RegFsm.cpp
tests/
└── framework_tests.cpp
```

## 3. 设计目标

当前代码体现出的设计目标可以概括为：

1. 提供一个可复用的状态机基类 `Cfsm`。
2. 通过继承方式扩展不同业务状态机。
3. 用统一消息对象 `CMsg` 驱动状态机处理。
4. 将消息处理拆分为前处理、处理中、后处理三个阶段。
5. 通过 `Cfactory_mgr` 和 `Cfactory` 解耦消息入口、业务路由和 FSM 生命周期。
6. 让具体状态机通过转移表定义状态和事件之间的关系。

## 4. 非目标

当前版本已经实现了状态机工厂、管理器、线程安全消息泵、定时器事件、CMake 构建入口和基础测试入口。

暂未完整实现的能力包括：

1. 完整日志系统。
2. 真实消息缓冲区资源池。
3. 状态进入和退出回调中的真实业务逻辑。
4. 大规模压测与性能指标。
5. 基于 GoogleTest 等测试框架的系统化单元测试。
6. 定时器线程的条件变量唤醒优化。

## 5. 核心概念

### 5.1 状态机

状态机负责维护当前状态，并根据输入消息执行业务逻辑，然后决定是否改变状态或生成下一条消息。

当前项目中：

- 抽象状态机由 `Cfsm` 表示；
- 注册流程状态机由 `RegFsm` 表示；
- 认证流程状态机由 `AuthFsm` 表示。

### 5.2 状态

状态由 `Tstate` 枚举表示，定义在 `reg_fsm/inc/common.h`。

```cpp
enum Tstate
{
    IDLE = 0,
    WORKING,
    KILL_FSM
};
```

状态含义：

| 状态 | 含义 |
|---|---|
| `IDLE` | 状态机已创建，尚未进入业务处理流程。 |
| `WORKING` | 状态机正在执行业务流程。 |
| `KILL_FSM` | 状态机生命周期结束，等待 factory 回收。 |

### 5.3 消息

消息由 `CMsg` 类表示，定义在 `reg_fsm/inc/CMsg.h`。

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

当前字段说明：

| 字段 | 类型 | 说明 |
|---|---|---|
| `type` | `MsgType` | 当前消息类型。默认值是 `MSG_INIT`。 |
| `serviceId` | `unsigned int` | 目标服务或工厂 ID，用于路由到 `Cfactory`。 |
| `fsmId` | `unsigned int` | 目标 FSM 实例 ID，用于路由到具体 `Cfsm`。 |
| `sessionId` | `unsigned int` | 业务会话 ID，预留给业务层使用。 |
| `msg` | `std::vector<char>` | 消息载荷，当前示例中暂未使用。 |

当前流程主要依赖 `type` 驱动。`msg` 字段可以为后续协议数据、业务参数、请求体或响应体预留。

### 5.4 消息类型

消息类型由 `MsgType` 枚举表示。

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

消息含义：

| 消息 | 含义 |
|---|---|
| `MSG_INIT` | 初始化业务流程。 |
| `MSG_CONNECT` | 建立连接或准备连接。 |
| `MSG_REQ` | 发送或处理请求。 |
| `MSG_RESP` | 处理响应。 |
| `MSG_TIMEOUT` | 处理超时流程，当前由注册流程使用。 |
| `MSG_CLOSE` | 关闭流程并结束状态机。 |

### 5.5 处理结果

处理结果由 `EerrNo` 表示，定义在 `common.h`。

```cpp
enum EerrNo
{
    INIT = 0,
    SUCCESS,
    ERROR,
};
```

含义：

| 枚举值 | 含义 |
|---|---|
| `INIT` | 初始状态或尚未处理。 |
| `SUCCESS` | 处理成功。 |
| `ERROR` | 处理失败。 |

## 6. 模块职责

### 6.1 `common.h`

`common.h` 负责放置公共类型和常量定义。

当前内容包括：

- 基础类型别名，例如 `U32`；
- 服务相关宏，例如 `CHAT_SERVICE_KEY_BASE`、`MAX_CHAT_SERVICE_KEY`；
- 工厂 ID 常量，例如 `FAC_REG_FAC_ID`、`FAC_AUTH_FAC_ID`、`FAC_NUM_IN_MGR_MAX`；
- 消息类型枚举 `Tmsg_type`；
- 状态枚举 `Tstate`；
- 错误码枚举 `EerrNo`。

注意：当前 `Tmsg_type`、`CHAT_SERVICE_KEY_BASE`、`MAX_CHAT_SERVICE_KEY` 等定义在示例流程中还没有实际使用。

### 6.2 `CMsg`

`CMsg` 是消息载体，负责承载状态机处理所需的输入信息。

当前设计很简单：

- 默认消息类型为 `MSG_INIT`；
- 保留 `std::vector<char>` 类型的消息体。

它目前没有成员函数，也没有做封装校验。后续如果消息结构复杂，可以增加以下能力：

- 构造函数；
- 消息类型检查；
- payload 读写接口；
- 序列化和反序列化；
- 消息来源、目标、序列号、时间戳等元数据。

### 6.3 `Cfsm`

`Cfsm` 是状态机基类，定义在 `reg_fsm/inc/Cfsm.h`，实现在 `reg_fsm/src/Cfsm.cpp`。

核心成员：

```cpp
private:
    unsigned int _fsmId;
    Tstate _state;
    EerrNo _prc;
    Cfactory* _factory;

protected:
    std::list<CMsg> _save;
    std::list<CMsg> _hold;
```

成员含义：

| 成员 | 含义 |
|---|---|
| `_fsmId` | 当前 FSM 在工厂中的实例 ID。 |
| `_state` | 当前状态机状态。 |
| `_prc` | 最近一次处理结果。 |
| `_factory` | 所属工厂指针，用于访问管理器和投递后续消息。 |
| `_save` | 暂存消息队列。 |
| `_hold` | 挂起消息队列。 |

核心接口：

| 接口 | 说明 |
|---|---|
| `GetState()` | 获取当前状态。 |
| `SetState(Tstate state)` | 设置当前状态。 |
| `_changeState(Tstate state)` | 内部状态切换接口。 |
| `PrePrcMsg(CMsg& pBuf)` | 消息处理前置阶段。 |
| `ProcessMsg(CMsg& pMsg)` | 消息处理主阶段。 |
| `PostPrcMsg(CMsg& pBuf)` | 消息处理后置阶段。 |
| `Create()` | 状态机创建初始化。 |
| `Destroy()` | 状态机销毁处理。 |
| `Destory()` | 兼容旧代码的销毁接口包装。 |
| `SaveMsg()` / `HoldMsg()` | 保存或挂起消息。 |
| `Print(bool detailFlag)` | 打印状态机信息。 |

当前实现特点：

- 构造函数只初始化基础字段，`Cfactory::AddFsm()` 创建实例后调用 `Create()`；
- `Create()` 将 `_prc` 设置为 `INIT`；
- `ProcessMsg()` 默认打印 `"Cfsm::ProcessMsg"` 并返回 `SUCCESS`；
- `PrePrcMsg()`、`PostPrcMsg()` 默认打印对应日志。

### 6.4 `RegFsm`

`RegFsm` 是具体业务状态机，定义在 `reg_fsm/inc/RegFsm.h`，实现在 `reg_fsm/src/RegFsm.cpp`。

它继承自 `Cfsm`：

```cpp
class RegFsm : public Cfsm
```

主要职责：

1. 实现注册服务相关流程。
2. 根据当前 `CMsg::type` 决定下一步。
3. 在合适时机修改状态机状态。
4. 复用基类的前处理、主处理和后处理默认逻辑。

当前 `RegFsm::ProcessMsg` 使用表驱动方式处理消息类型：

```cpp
struct RegTransition
{
    Tstate from;
    MsgType event;
    Tstate to;
    bool hasNext;
    MsgType nextEvent;
    unsigned int delayMs;
    const char* log;
};
```

### 6.5 `AuthFsm`

`AuthFsm` 是认证业务状态机，定义在 `reg_fsm/inc/AuthFsm.h`，实现在 `reg_fsm/src/AuthFsm.cpp`。

它同样继承自 `Cfsm`：

```cpp
class AuthFsm : public Cfsm
```

主要职责：

1. 实现认证服务相关流程。
2. 使用独立的 `AuthTransition` 转移表描述认证流程。
3. 通过 `Cfsm::SendMsg` 投递后续事件。
4. 在 `MSG_CLOSE` 后进入 `KILL_FSM`，由 `AuthFactory` 回收。

### 6.6 `RegFactory` 和 `AuthFactory`

`RegFactory` 和 `AuthFactory` 都继承自 `Cfactory`。

两者职责一致，但创建的 FSM 类型不同：

| 工厂 | serviceId | 创建的 FSM | 说明 |
|---|---:|---|---|
| `RegFactory` | `FAC_REG_FAC_ID` | `RegFsm` | 注册业务流程 |
| `AuthFactory` | `FAC_AUTH_FAC_ID` | `AuthFsm` | 认证业务流程 |

factory 的共同处理规则：

1. 如果消息携带 `fsmId`，优先查找已有 FSM。
2. 如果未找到 FSM，只有 `MSG_INIT` 可以创建新 FSM。
3. 找到或创建 FSM 后调用 `DispatchToFsm`。
4. FSM 进入 `KILL_FSM` 后调用 `KillFsm` 回收实例。

## 7. 状态转移设计

### 7.1 注册流程消息推进表

当前注册流程中，消息按照固定顺序推进：

| 当前消息 | 处理行为 | 下一消息 |
|---|---|---|
| `MSG_INIT` | 初始化注册服务 | `MSG_CONNECT` |
| `MSG_CONNECT` | 处理连接阶段 | `MSG_REQ` |
| `MSG_REQ` | 处理请求阶段 | `MSG_RESP` |
| `MSG_RESP` | 处理响应阶段 | `MSG_TIMEOUT` |
| `MSG_TIMEOUT` | 处理超时阶段 | `MSG_CLOSE` |
| `MSG_CLOSE` | 关闭注册服务 | 不再设置下一消息 |

### 7.2 认证流程消息推进表

当前认证流程中，消息按照固定顺序推进：

| 当前消息 | 处理行为 | 下一消息 |
|---|---|---|
| `MSG_INIT` | 准备认证上下文 | `MSG_CONNECT` |
| `MSG_CONNECT` | 连接认证服务 | `MSG_REQ` |
| `MSG_REQ` | 校验认证信息 | `MSG_RESP` |
| `MSG_RESP` | 认证成功 | `MSG_CLOSE` |
| `MSG_CLOSE` | 关闭认证流程 | 不再设置下一消息 |

### 7.3 状态变化表

当前状态变化比较简单：

| 当前状态 | 触发消息 | 下一状态 |
|---|---|---|
| `IDLE` | `MSG_INIT` | `WORKING` |
| `WORKING` | `MSG_CONNECT` | `WORKING` |
| `WORKING` | `MSG_REQ` | `WORKING` |
| `WORKING` | `MSG_RESP` | `WORKING` |
| `WORKING` | `MSG_TIMEOUT` | `WORKING` |
| `WORKING` | `MSG_CLOSE` | `KILL_FSM` |

### 7.4 当前实现中的关键点

`RegFsm::ProcessMsg` 和 `AuthFsm::ProcessMsg` 的关键逻辑一致：

1. 如果当前状态已经是 `KILL_FSM`，返回 `ERROR`，避免重复处理。
2. 调用基类 `Cfsm::ProcessMsg(pMsg)`。
3. 根据“当前状态 + 当前消息”查找对应业务的转移表。
4. 找不到合法转移时返回 `ERROR`。
5. 找到转移后打印日志并切换到目标状态。
6. 如果转移表配置了下一事件，则通过 `Cfsm::SendMsg` 或 `Cfsm::StartTimer` 投递。
7. 在 `MSG_CLOSE` 时设置状态为 `KILL_FSM`，随后由 `Cfactory` 回收该 FSM。

这意味着当前状态机不再直接修改同一个 `CMsg::type` 来驱动流程，而是通过管理器消息泵投递下一条消息。普通事件和定时器事件走同一条分发链路。

## 8. 框架图与流程图

本节汇总当前框架的关键架构图和流程图，方便新接手的开发者快速理解整体协作方式。

### 8.1 总体架构图

```mermaid
flowchart TD
    App["main.cpp / Application"] --> Mgr["Cfactory_mgr"]

    Mgr --> Pump["Thread-safe message pump"]
    Mgr --> Timer["Timer threads"]
    Mgr --> RegFac["RegFactory"]
    Mgr --> AuthFac["AuthFactory"]

    RegFac --> RegFsm1["RegFsm instance"]
    AuthFac --> AuthFsm1["AuthFsm instance"]

    RegFsm1 --> Mgr
    AuthFsm1 --> Mgr
    Timer --> Pump
    Pump --> Mgr
```

### 8.2 当前启动流程

```mermaid
flowchart TD
    A["main()"] --> B["Register RegFactory(FAC_REG_FAC_ID)"]
    B --> C["Register AuthFactory(FAC_AUTH_FAC_ID)"]
    C --> D["mgr.Start()"]
    D --> E["Start worker thread"]
    E --> F["Run message pump"]

    F --> G["FsmMgrTest(FAC_REG_FAC_ID)"]
    G --> H["Send MSG_INIT to RegFactory"]

    F --> I["FsmMgrTest(FAC_AUTH_FAC_ID)"]
    I --> J["Send MSG_INIT to AuthFactory"]

    F --> K["FsmMgrTest(unknown factory id)"]
    K --> L["DispatchMsg returns ERROR"]

    L --> M["mgr.Stop()"]
```

### 8.3 消息分发流程

```mermaid
flowchart TD
    A["SendMsg(CMsg)"] --> B["Push message into _pump"]
    B --> C["Notify _pump_cv"]
    C --> D["Cfactory_mgr::Run wakes up"]
    D --> E["Pop one CMsg"]
    E --> F["DispatchMsg(msg)"]
    F --> G{"FindFactory(msg.serviceId)"}

    G -- "Reg factory" --> H["RegFactory::FacMsgPrc"]
    G -- "Auth factory" --> I["AuthFactory::FacMsgPrc"]
    G -- "Not found" --> J["Return ERROR"]

    H --> K["Find or create RegFsm"]
    I --> L["Find or create AuthFsm"]

    K --> M["DispatchToFsm"]
    L --> M

    M --> N["PrePrcMsg"]
    N --> O["ProcessMsg"]
    O --> P["PostPrcMsg"]
    P --> Q{"state == KILL_FSM?"}
    Q -- "Yes" --> R["KillFsm"]
    Q -- "No" --> S["Wait for next message"]
```

### 8.4 Factory 生命周期图

```mermaid
flowchart TD
    A["Factory receives CMsg"] --> B{"msg.fsmId != 0?"}

    B -- "Yes" --> C["FindFsm(fsmId)"]
    B -- "No" --> D["fsm = nullptr"]

    C --> E{"FSM found?"}
    D --> E

    E -- "No + msg.type == MSG_INIT" --> F["AddFsm()"]
    E -- "No + other msg" --> G["Return ERROR"]
    E -- "Yes" --> H["DispatchToFsm"]

    F --> H
    H --> I["FSM handles message"]
    I --> J{"FSM state == KILL_FSM?"}
    J -- "Yes" --> K["KillFsm"]
    J -- "No" --> L["Keep FSM alive"]
```

### 8.5 RegFsm 状态转换图

```mermaid
stateDiagram-v2
    [*] --> IDLE
    IDLE --> WORKING: MSG_INIT / send MSG_CONNECT
    WORKING --> WORKING: MSG_CONNECT / send MSG_REQ
    WORKING --> WORKING: MSG_REQ / send MSG_RESP
    WORKING --> WORKING: MSG_RESP / start MSG_TIMEOUT timer
    WORKING --> WORKING: MSG_TIMEOUT / send MSG_CLOSE
    WORKING --> KILL_FSM: MSG_CLOSE
    KILL_FSM --> [*]: factory recycles FSM
```

### 8.6 AuthFsm 状态转换图

```mermaid
stateDiagram-v2
    [*] --> IDLE
    IDLE --> WORKING: MSG_INIT / send MSG_CONNECT
    WORKING --> WORKING: MSG_CONNECT / send MSG_REQ
    WORKING --> WORKING: MSG_REQ / send MSG_RESP
    WORKING --> KILL_FSM: MSG_RESP / send MSG_CLOSE
    KILL_FSM --> [*]: factory recycles FSM
```

### 8.7 定时器流程图

```mermaid
flowchart TD
    A["RegFsm handles MSG_RESP"] --> B["StartNextTimer(MSG_TIMEOUT, delayMs)"]
    B --> C["Cfsm::StartTimer"]
    C --> D["Cfactory_mgr::StartTimer"]
    D --> E["Create CTimerCtrl"]
    E --> F["Start timer thread"]
    F --> G["sleep_for(timeoutMs)"]
    G --> H{"timer active?"}
    H -- "Yes" --> I["SendMsg(MSG_TIMEOUT)"]
    H -- "No" --> J["Do nothing"]
    I --> K["Message pump dispatches timeout message"]
```

### 8.8 类关系图

```mermaid
classDiagram
    class Cfactory_mgr {
        -vector~Cfactory*~ _fac_list
        -queue~CMsg~ _pump
        -vector~CTimerCtrl~ _timer_list
        -thread _worker_thread
        +RegisterFactory(factory)
        +SendMsg(msg)
        +Start()
        +Stop()
        +Join()
        +StartTimer(timeoutMs, msg)
    }

    class Cfactory {
        -unsigned int _facId
        -Cfactory_mgr* _facMgr
        -vector~Cfsm*~ _fsm_list
        +CreateFsm()
        +FacMsgPrc(msg)
        +DispatchToFsm(fsm, msg)
        +KillFsm(fsmId)
    }

    class Cfsm {
        -unsigned int _fsmId
        -Tstate _state
        -Cfactory* _factory
        +ProcessMsg(msg)
        +SetState(state)
        +SendMsg(msg)
        +StartTimer(timeoutMs, msg)
    }

    class RegFactory
    class AuthFactory
    class RegFsm
    class AuthFsm
    class CMsg {
        +MsgType type
        +unsigned int serviceId
        +unsigned int fsmId
        +unsigned int sessionId
        +vector~char~ msg
    }

    Cfactory_mgr --> Cfactory
    Cfactory <|-- RegFactory
    Cfactory <|-- AuthFactory
    Cfactory --> Cfsm
    Cfsm <|-- RegFsm
    Cfsm <|-- AuthFsm
    Cfactory_mgr --> CMsg
    Cfsm --> CMsg
```

框架速记：

1. 外部只投递 `CMsg`。
2. `Cfactory_mgr` 按 `serviceId` 查找目标 factory。
3. factory 按 `fsmId` 查找已有 FSM，或在 `MSG_INIT` 时创建新 FSM。
4. FSM 根据“当前状态 + 当前事件”查表处理，并投递后续事件。
5. FSM 进入 `KILL_FSM` 后由 factory 统一回收。

## 9. 接口设计说明

### 9.1 `PrePrcMsg`

前处理接口。

当前行为：

- `RegFsm::PrePrcMsg` 调用 `Cfsm::PrePrcMsg`；
- 基类打印 `"Cfsm::PrePrcMsg"`。

适合放置的逻辑：

- 消息合法性预检查；
- 上下文准备；
- 日志记录；
- 统计计数；
- 消息解码。

### 9.2 `ProcessMsg`

主处理接口。

当前行为：

- 业务 FSM 的 `ProcessMsg` 先检查当前状态；
- 调用 `Cfsm::ProcessMsg`；
- 根据“当前状态 + `CMsg::type`”查表执行转移；
- 返回 `SUCCESS` 或 `ERROR`。

适合放置的逻辑：

- 核心业务处理；
- 状态转移；
- 生成下一步事件；
- 错误处理；
- 调用外部服务或协议处理函数。

### 9.3 `PostPrcMsg`

后处理接口。

当前行为：

- 业务 FSM 的 `PostPrcMsg` 调用 `Cfsm::PostPrcMsg`；
- 基类打印 `"Cfsm::PostPrcMsg"`。

适合放置的逻辑：

- 释放临时资源；
- 记录处理结果；
- 发送处理完成通知；
- 上报统计信息；
- 清理消息缓存。

### 9.4 `Create`

状态机初始化接口。

当前行为：

- 设置 `_state = IDLE`；
- 设置 `_prc = INIT`；
- 返回 `SUCCESS`。

后续可以扩展：

- 初始化状态机上下文；
- 注册定时器；
- 分配资源；
- 初始化内部队列。

### 9.5 `Destroy`

状态机销毁接口。

当前行为：

- 打印 `"Cfsm::Destroy"`；
- 返回 `SUCCESS`。

注意：当前代码保留了 `Destory()` 作为旧接口兼容包装，新的代码应优先调用 `Destroy()`。

后续可以扩展：

- 停止定时器；
- 释放资源；
- 关闭连接；
- 清空队列；
- 输出最终状态。

## 10. 当前代码中的设计问题

### 10.1 纯虚函数提供默认实现

`Cfsm.h` 中这几个接口被声明为纯虚函数：

```cpp
virtual void PrePrcMsg(CMsg& pBuf) = 0;
virtual EerrNo ProcessMsg(CMsg& pMsg) = 0;
virtual void PostPrcMsg(CMsg& pBuf) = 0;
```

但 `Cfsm.cpp` 又提供了实现。

这在 C++ 中是允许的，含义是：

- `Cfsm` 仍然是抽象类，不能直接实例化；
- 派生类必须实现这些函数；
- 派生类可以在自己的实现中显式调用基类版本，例如 `Cfsm::ProcessMsg(pMsg)`。

如果这是有意设计，建议在注释中说明。否则容易让维护者困惑。

### 10.2 `Destory` 兼容问题

旧接口名是 `Destory`，正确英文通常应为 `Destroy`。当前代码已经新增 `Destroy()`，并保留 `Destory()` 作为兼容包装。

影响：

- 影响可读性；
- 后续其他开发者容易写错；
- 文档和代码会持续传播这个拼写。

建议尽早统一修正。

### 10.3 定时器实现仍是原型版

当前 `Cfactory_mgr::StartTimer` 使用“一个 timer 一个线程”的方式：

```cpp
std::thread timerThread([this, timeoutMs, timeoutMsg, timerCtrl]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
    if (timerCtrl->active.load())
    {
        SendMsg(timeoutMsg);
    }
});
```

该方式适合当前原型，但不适合大量定时器场景。

主要限制：

- 定时器数量多时会创建大量线程；
- `StopTimer` 不会中断 `sleep_for`，只会阻止到期后投递消息；
- 析构时需要等待 timer 线程结束。

### 10.4 错误码粒度较粗

当前错误码只有：

```cpp
INIT,
SUCCESS,
ERROR
```

这能满足 demo，但不能区分具体错误来源，例如：

- 未找到 factory；
- 未找到 FSM；
- 非法状态转移；
- 定时器创建失败；
- 消息泵已停止。

### 10.5 日志系统仍较简单

当前日志直接使用 `std::cout` 输出。虽然主要日志已经有换行，但还没有统一日志级别、模块名、时间戳、线程 ID 或可关闭的调试开关。

### 10.6 构建系统已补充

项目当前已经新增根目录 `CMakeLists.txt`，可以构建 demo 和测试入口。

后续仍可继续增强：

- 增加更多编译选项；
- 增加安装规则；
- 接入 CI；
- 按库和示例程序拆分 target。

## 11. 推荐演进方案

### 11.1 第一阶段：整理当前代码

目标是让当前 demo 更规范，但不改变整体设计。

建议事项：

1. 持续完善 `CMakeLists.txt`。
2. 优先使用 `Destroy`，逐步清理旧的 `Destory` 兼容接口。
3. 清理未使用宏或补充注释说明。
4. 将 `RegFsm`、`AuthFsm` 的共同转移表处理逻辑抽象出来。
5. 将 `Cfactory` 的路由公共逻辑进一步复用，减少具体 factory 重复代码。

### 11.2 第二阶段：增强可测试性

目标是让状态转移可以被自动验证。

建议事项：

1. 增加单元测试。
2. 将状态转移逻辑拆成更小函数。
3. 避免 `ProcessMsg` 直接承担太多职责。
4. 为非法消息和非法状态增加测试。

可测试点：

- `CMsg` 默认类型是 `MSG_INIT`；
- `RegFsm` 初始状态是 `IDLE`；
- `MSG_INIT` 后状态变成 `WORKING`；
- 完整流程最终进入 `KILL_FSM`；
- 非法消息返回 `ERROR`。

### 11.3 第三阶段：引入显式转移结果

当前接口返回 `EerrNo`，下一消息通过修改 `CMsg` 获得。

可以考虑新增结果类型：

```cpp
struct FsmResult
{
    EerrNo err;
    Tstate nextState;
    MsgType nextMsg;
};
```

这样可以明确表达：

- 本次处理是否成功；
- 下一状态是什么；
- 是否产生下一事件。

### 11.4 第四阶段：增强表驱动状态机

当前 `RegFsm` 已经从 `switch` 演进为表驱动。后续如果流程继续变多，可以把转移表进一步抽象到基类或配置层。

示例结构：

```cpp
struct Transition
{
    Tstate currentState;
    MsgType event;
    Tstate nextState;
    MsgType nextEvent;
};
```

优点：

- 状态转移规则更集中；
- 更容易检查缺失分支；
- 更容易做自动化测试；
- 可以在配置文件或表中描述流程。

### 11.5 第五阶段：框架化能力

如果项目要从当前原型继续发展成更通用的 FSM 框架，可以重点增强：

1. 状态机基类模板化。
2. 通用转移表执行器，减少 `RegFsm`、`AuthFsm` 中的重复代码。
3. 更完整的定时器管理器，避免一个定时器一个线程。
4. 显式状态进入和退出回调语义。
5. 统一日志接口。
6. 统一错误处理策略。
7. 消息缓冲区池和资源池的真实落地。
8. DOT/Mermaid 状态图导出。
9. 性能压测和并发测试。

## 12. 建议的目标架构

如果后续要从 demo 演进为框架，可以参考以下分层：

```mermaid
flowchart TD
    A["应用层 Application"] --> B["具体业务 FSM"]
    B --> C["FSM 基类 / 接口层"]
    C --> D["状态转移引擎"]
    C --> E["消息模型"]
    C --> F["生命周期管理"]
    D --> G["转移表 / 分发器"]
    F --> H["定时器 / 资源管理"]
```

各层职责：

| 层级 | 职责 |
|---|---|
| 应用层 | 创建状态机，投递消息，接收处理结果。 |
| 具体业务 FSM | 实现业务动作和状态转移规则。 |
| FSM 基类 / 接口层 | 统一生命周期和处理接口。 |
| 状态转移引擎 | 负责根据状态和事件找到下一步。 |
| 消息模型 | 定义消息类型、载荷和元数据。 |
| 生命周期管理 | 管理创建、销毁、资源释放。 |

## 13. 扩展示例：新增一个状态机

如果要新增一个业务状态机，例如 `LoginFsm`，可以按当前模式执行：

1. 在 `common.h` 中新增业务 factory ID，例如 `FAC_LOGIN_FAC_ID`。
2. 在 `inc` 下新增 `LoginFsm.h` 和 `LoginFactory.h`。
3. 在 `src` 下新增 `LoginFsm.cpp` 和 `LoginFactory.cpp`。
4. `LoginFsm` 继承 `Cfsm`，实现 `PrePrcMsg`、`ProcessMsg`、`PostPrcMsg`。
5. `LoginFactory` 继承 `Cfactory`，实现创建、查找和路由 `LoginFsm` 的逻辑。
6. 在 `main.cpp` 或应用入口中创建 `LoginFactory`，并调用 `RegisterFactory` 注册到 `Cfactory_mgr`。
7. 在 `CMakeLists.txt` 中加入新增源文件。

示例结构：

```cpp
class LoginFsm : public Cfsm
{
public:
    explicit LoginFsm();

    void PrePrcMsg(CMsg& pBuf) override;
    EerrNo ProcessMsg(CMsg& pMsg) override;
    void PostPrcMsg(CMsg& pBuf) override;
};
```

这种方式简单直接，适合小规模状态机。若状态机数量继续变多，建议抽象公共 factory 路由逻辑和转移表工具，减少新增业务时的重复代码。

## 14. 错误处理建议

当前错误处理只有 `ERROR`，粒度较粗。后续可以考虑细分错误码：

```cpp
enum EerrNo
{
    INIT = 0,
    SUCCESS,
    ERROR,
    INVALID_STATE,
    INVALID_MSG,
    TIMEOUT,
    INTERNAL_ERROR
};
```

这样可以区分：

- 消息非法；
- 状态非法；
- 处理超时；
- 内部错误；
- 外部依赖失败。

## 15. 日志建议

短期建议：

- 保持每条日志独占一行；
- 日志中包含状态、消息、处理结果；
- 错误日志中包含失败原因。

示例：

```text
[RegFsm] state=IDLE event=MSG_INIT result=SUCCESS next_state=WORKING next_event=MSG_CONNECT
```

长期建议：

- 封装日志宏；
- 支持日志级别；
- 支持输出文件名和行号；
- 支持关闭或打开调试日志。

## 16. 构建说明

项目已经添加根目录 `CMakeLists.txt`。

当前目标划分：

```text
reg_fsm_demo     示例程序
reg_fsm_tests    单元测试
```

这样项目可以通过统一命令构建：

```bash
cmake -S . -B build
cmake --build build
```

## 17. 测试建议

当前已有 `tests/framework_tests.cpp`，覆盖了基础消息默认值、非法转移和 manager/factory/fsm 主链路。后续建议继续补充以下测试：

| 测试项 | 预期 |
|---|---|
| `RegFsm` 完整流程 | 最终进入 `KILL_FSM` 并被 `RegFactory` 回收 |
| `AuthFsm` 完整流程 | 最终进入 `KILL_FSM` 并被 `AuthFactory` 回收 |
| 未知 `serviceId` | `Cfactory_mgr::DispatchMsg` 返回 `ERROR` |
| 未知 `fsmId` + 非 `MSG_INIT` | 对应 factory 返回 `ERROR` |
| `StartTimer` + `StopTimer` | timer 被取消后不再投递消息 |
| 多个 factory 同时注册 | 不同 `serviceId` 路由到不同 factory |

如果后续引入 GoogleTest，可以把当前 `assert` 测试迁移为更清晰的测试用例。

## 18. 当前示例预期输出

当前示例会注册 `RegFactory` 和 `AuthFactory`，并发送三条初始消息：

1. `FAC_REG_FAC_ID`：进入注册流程；
2. `FAC_AUTH_FAC_ID`：进入认证流程；
3. `FAC_AUTH_FAC_ID + 1`：进入未知 factory 错误路径。

输出中会看到类似日志：

```text
Cfactory_mgr::RegisterFactory facId=1
Cfactory_mgr::RegisterFactory facId=3
[REG][MSG_INIT]: start reg service
[AUTH][MSG_INIT]: prepare auth context
Cfactory_mgr::DispatchMsg unknown serviceId=4
[REG][MSG_TIMEOUT]: timeout reg service
Cfactory::KillFsm facId=1 fsmId=...
Cfactory::KillFsm facId=3 fsmId=...
```

具体顺序可能受后台消息泵和定时器线程影响，但整体会体现注册流程、认证流程和错误路径三类行为。

## 19. 总结

`FsmFramework` 当前是一个事件驱动 FSM 框架原型。它已经具备以下基础：

- `Cfactory_mgr -> Cfactory -> Cfsm` 三层结构；
- `RegFactory/RegFsm` 注册业务流程；
- `AuthFactory/AuthFsm` 认证业务流程；
- 基于 `serviceId` 和 `fsmId` 的消息路由；
- 后台消息泵和简单定时器；
- 表驱动状态转移；
- FSM 创建、分发和回收。

当前最值得优先处理的问题是：

1. 逐步清理 `Destory` 兼容接口；
2. 引入更细粒度错误码；
3. 将转移表处理逻辑抽象为可复用组件；
4. 将定时器从“一 timer 一线程”升级为统一 TimerManager；
5. 补充更强的测试断言；
6. 引入统一日志模块。

如果项目目标是学习状态机，当前结构已经足够直观。如果目标是做可复用框架，建议下一步从错误码、日志、测试和定时器管理这几件事继续推进。

## 20. 当前框架实现进展

当前代码已经在原始 demo 基础上实现了第一版框架化改造，核心能力包括：

1. `Cfactory_mgr` 作为顶层管理器，负责工厂注册、消息泵、消息分发、停止控制和定时器事件。
2. `Cfactory` 作为状态机工厂基类，负责 FSM 创建、查找、消息派发和生命周期回收。
3. `RegFactory` 作为注册业务工厂，负责把注册类消息路由到 `RegFsm`。
4. `AuthFactory` 作为认证业务工厂，负责把认证类消息路由到 `AuthFsm`。
5. `Cfsm` 作为状态机基类，维护 `fsmId`、状态、所属工厂、`_save` 和 `_hold` 消息暂存队列。
6. `RegFsm` 和 `AuthFsm` 都使用表驱动状态转移。
7. `RegFsm` 的 `MSG_RESP -> MSG_TIMEOUT` 已通过 `Cfactory_mgr::StartTimer` 定时器事件触发。
8. `main.cpp` 已通过 `Cfactory_mgr` 注册两个 factory，并向两个不同 `serviceId` 投递初始消息。
9. 项目新增 `CMakeLists.txt` 和 `tests/framework_tests.cpp`，用于构建 demo 和基础测试。
10. `Cfactory_mgr` 已新增 `Start/Stop/Join`，调用方不需要分散管理工作线程。
11. `Cfsm` 已新增受保护的 `SendMsg/StartTimer/StopTimer`，业务 FSM 不再直接依赖 manager。
12. `Cfactory` 已为 FSM 列表增加锁保护，降低并发访问风险。
13. `Cfsm::SetState` 已支持 `OnExitState/OnEnterState` 状态切换钩子。

当前运行链路如下：

```text
main
  -> Cfactory_mgr::RegisterFactory
  -> Cfactory_mgr::Start
  -> Cfactory_mgr::Run
  -> Cfactory_mgr::SendMsg(MSG_INIT)
  -> RegFactory::FacMsgPrc / AuthFactory::FacMsgPrc
  -> Cfactory::AddFsm
  -> RegFsm::ProcessMsg / AuthFsm::ProcessMsg
  -> Cfsm::SendMsg / StartTimer
  -> MSG_CLOSE 后 Cfactory::KillFsm
```

## 21. 后续可继续增强的方向

虽然当前版本已经具备框架雏形，但如果要进一步提升简历竞争力和工程完整度，建议继续补充：

1. 更细粒度错误码，例如 `INVALID_STATE`、`INVALID_MSG`、`TIMER_ERROR`。
2. 定时器线程的条件变量唤醒机制，避免长时间 sleep 阻塞析构等待。
3. 更完整的单元测试框架，例如 GoogleTest。
4. 压测用例，例如大量 FSM 实例和大量消息投递。
5. 消息缓冲区池 `Cmsgbuf`，替代直接拷贝 `std::vector<char>`。
6. 状态进入和退出回调中的真实业务处理。
7. 状态图导出能力，用转移表生成 Mermaid 或 DOT。
8. 将 `RegFsm` 和 `AuthFsm` 的公共转移表查找逻辑抽象为通用组件。
