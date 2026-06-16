# Circuit Simulator DSL

一个用于数字电路仿真的 C++ DSL（领域特定语言）实现。该实现支持电源、保护电阻、开关和灯泡，并能够判断电路中灯泡的亮灭状态。

## 核心功能

- **元件定义**: 支持定义电源(Power)、电阻(Resistor)、开关(Switch)、灯泡(Bulb)
- **导线连接**: 使用 `link x,y` 连接元件引脚
- **开关控制**: 使用 `set switch_name 0/1` 控制开关状态
- **电路仿真**: `run` 命令计算并显示所有灯泡状态(0=熄灭，1=点亮)
- **动态修改**: 支持添加/删除导线、清空电路、实时查看电路状态

## 灯泡亮灭规则

1. **短路规则**: 如果灯泡的两个引脚被导线或闭合的开关直接连接（等势节点），则灯泡被短路，不亮(0)
2. **导通规则**: 如果灯泡位于电源正负极之间的任何导通路径上，且不被短路，则灯泡点亮(1)

## DSL 语法规范

### 1. 元件定义
```
power identifier
resistor identifier
switch identifier  
bulb identifier
```

支持批量定义，用逗号分隔：
```
bulb b1,b2,b3
switch s1,s2
```

**标识符规范**: `[a-zA-Z_][a-zA-Z0-9_]*`

### 2. 导线连接
```
link component_name.pin1,component_name.pin2
```

引脚命名约定：
- 电源: `+` / `-` （或 `p`, `n`, `1`, `2`）
- 其他元件: `1` / `2` （或 `a`, `b`, `left`, `right`）

示例：
```
link p.+,r1.1
link r1.2,s1.1
link s1.2,b1.1
link b1.2,p.-
```

### 3. 开关控制
```
set switch_identifier 0    # 断开（默认）
set switch_identifier 1    # 闭合
```

### 4. 批量开关组控制
使用 `group` 定义一个开关组，`set` 命令可以用一个整数的二进制位批量设置组内开关状态。

```
group group_name s1,s2,s3   # 定义开关组，成员按顺序 s1（bit 0）、s2（bit 1）、s3（bit 2）
set group_name 0b101        # s1 和 s3 闭合，s2 断开
set group_name 0x5          # 同上（十六进制）
set group_name 5            # 同上（十进制）
```

### 5. 电路操作
```
run                         # 仿真并显示灯泡状态
print                       # 显示当前所有元件、开关组和导线连接
clear                       # 清空整个电路（所有元件和连接）
clear components            # 只清空元件（保留连接）
clear links                 # 只清空导线连接（保留元件）
del endpoint1,endpoint2     # 删除指定的导线连接
exit                        # 退出程序
```

### 6. 错误处理
交互模式下，任何命令出错都会在终端显示 `Error: ...` 信息，但程序不会退出，可继续输入后续命令。

## C++ API 接口

### 主要类定义

#### CircuitSimulator
核心仿真器类，位于命名空间 `circuit`。

```cpp
namespace circuit {

class CircuitSimulator {
public:
    // 元件定义
    bool addComponent(const std::string& type,
                      const std::string& identifier);
    
    // 批量添加元件
    void addComponents(const std::string& type,
                       const std::vector<std::string>& identifiers);
    
    // 导线连接
    bool addLink(const std::string& endpoint1,
                 const std::string& endpoint2);
    
    // 删除导线
    bool removeLink(const std::string& endpoint1,
                    const std::string& endpoint2);
    
    // 开关控制
    bool setSwitchState(const std::string& identifier,
                        bool closed);
    
    // 批量开关组
    bool addSwitchGroup(
        const std::string& groupName,
        const std::vector<std::string>& switchIdentifiers);
    
    bool setSwitchGroupState(
        const std::string& groupName,
        const std::string& value);
    
    // 电路仿真
    std::vector<BulbResult> simulate() const;
    
    // 清空电路
    void clearAll();
    void clearComponents();
    void clearLinks();
    
    // 获取当前状态
    std::string toString() const;
    
    // DSL 执行
    bool executeCommand(const std::string& commandLine,
                       std::vector<std::string>& outputLines,
                       std::vector<std::string>& errorLines);
};

} // namespace circuit
```

#### BulbResult 结构体
```cpp
struct BulbResult {
    std::string identifier;     // 灯泡标识符
    int state;                  // 0=熄灭，1=点亮
    int declarationOrder;       // 定义顺序
};
```

### 使用示例

```cpp
#include "circuit/circuit_simulator.h"

// 创建仿真器
circuit::CircuitSimulator sim;

// 通过 DSL 脚本编程
std::vector<std::string> output, errors;
sim.executeCommand("power p", output, errors);
sim.executeCommand("resistor r1", output, errors);
sim.executeCommand("bulb b1", output, errors);
sim.executeCommand("link p.+,r1.1", output, errors);
sim.executeCommand("link r1.2,b1.1", output, errors);
sim.executeCommand("link b1.2,p.-", output, errors);

// 直接调用 API
sim.addComponent("switch", "s1");
sim.addLink("b1.1", "s1.1");
sim.addLink("s1.2", "b1.2");
sim.setSwitchState("s1", true);  // 闭合开关

// 仿真并获取结果
auto results = sim.simulate();
for (const auto& result : results) {
    std::cout << result.identifier << ": " << result.state << std::endl;
}

// 导出电路状态
std::cout << sim.toString() << std::endl;
```

### 命令行接口

```cpp
#include "circuit/cli.h"

int main(int argc, char* argv[]) {
    return circuit::cli::run(argc, argv);
}
```

## 编译与构建

### 使用 CMake
```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### 构建目标
- `circuit_simulator_lib`: 静态库
- `circuit_cli`: 命令行界面程序

## 示例电路

### 示例 1：基本串联电路
```
power p
resistor r1
bulb b1
link p.+,r1.1
link r1.2,b1.1
link b1.2,p.-
run
```

输出: `b1 1`

### 示例 2：带开关的电路
```
power p
switch s1
bulb b1
link p.+,s1.1
link s1.2,b1.1
link b1.2,p.-
run
set s1 1
run
```

输出:
```
b1 0
b1 1
```

### 示例 3：灯泡短路
```
power p
resistor r1
bulb b1
switch s1
link p.+,r1.1
link r1.2,b1.1
link b1.2,p.-
link b1.1,s1.1
link s1.2,b1.2
run
set s1 1
run
```

输出:
```
b1 1
b1 0
```

### 示例 4：批量开关组
```
power p
bulb b1
switch s1,s2,s3
group g s1,s2,s3
link p.+,b1.1
link b1.2,s1.1
link s1.2,s2.1
link s2.2,s3.1
link s3.2,p.-
set g 0b111
run
set g 0b000
run
```

输出:
```
b1 1
b1 0
```

## 算法原理

### 电路建模
1. 每个元件引脚分配唯一整数 ID
2. 导线和闭合的开关连接形成等势节点（使用并查集合并）
3. 灯泡和电阻构成图中的边

### 亮灭判定
1. **短路检测**: 如果灯泡两端属于同一等势节点，则被短路
2. **导通检测**: 
   - 构建连通图（剔除短路边）
   - 寻找电源正负极
   - 计算双连通分量（biconnected components）
   - 如果灯泡所在的边位于电源正负极之间的路径上，则点亮

### 复杂度
- 时间复杂度：O(N + E)，其中 N 为引脚数，E 为连接数
- 空间复杂度：O(N + E)

## 扩展性

### 支持的元件类型
- `power`: 电源（唯一）
- `resistor`: 电阻（保护电阻）
- `switch`: 开关
- `bulb`: 灯泡（或 `lamp`）

### 引脚别名
电源 (+) : `+`, `p`, `pos`, `positive`, `1`  
电源 (-) : `-`, `n`, `neg`, `negative`, `2`  
其他元件 (1) : `1`, `a`, `l`, `left`  
其他元件 (2) : `2`, `b`, `r`, `right`

## 错误处理

DSL 命令执行时提供详细的错误信息，包括：
- 语法错误
- 未定义的元件
- 重复的标识符
- 无效的引脚名
- 导线连接冲突

## 许可证

开源项目，可自由使用和修改。