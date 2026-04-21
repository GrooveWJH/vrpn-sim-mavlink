# vrpn-sim-mavlink

![macOS](https://img.shields.io/badge/macOS-supported-success?logo=apple)
![Linux](https://img.shields.io/badge/Linux-supported-success?logo=linux)

这是一个用于模拟 VRPN 动捕数据，并将指定 Tracker 姿态转发为 MAVLink `VISION_POSITION_ESTIMATE` 的小型工具链。项目分为两部分：

```text
.
├── Sender     # C++17 VRPN 发送端，负责模拟假想 Tracker
└── Receiver   # C++ VRPN → MAVLink 桥接器，负责把指定 Tracker 发给飞控
```

它适合这些场景：

- 在没有真实动捕系统时，先验证 VRPN → 飞控外部视觉链路
- 在局域网内模拟多机 / 单机的 Tracker 数据源
- 给 PX4 / ArduPilot / 自定义接收程序喂稳定的测试姿态数据

## 项目结构

- `Sender/`：生成 VRPN Tracker 数据
- `Receiver/`：从 VRPN 读取指定 Tracker，并通过串口或 UDP 发出 MAVLink

## 当前推荐使用方式

如果你只是想尽快在 Linux 上把链路跑通，建议优先从 `Sender` 开始：

1. 在一台局域网机器上运行 `Sender`
2. 让它持续发送某个 Tracker，例如 `sunraynext_uav0`
3. 在另一台机器或同机上运行 `Receiver` / 自己的 VRPN 客户端进行订阅

## Linux（Ubuntu 24 / ROS Jazzy 环境）快速指南

### 1. 安装基础依赖

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build
```

### 2. 安装 VRPN

在 Ubuntu 24 上，`libvrpn-dev` 可能不存在。当前仓库已经兼容 ROS Jazzy 自带的 VRPN 安装方式，可直接安装：

```bash
sudo apt install -y ros-jazzy-vrpn
```

安装后，头文件和静态库通常位于：

- `/opt/ros/jazzy/include`
- `/opt/ros/jazzy/lib`

仓库中的 `Sender/cmake/FindVRPN.cmake` 已经补充了对这一路径的查找。

## Sender：模拟 VRPN Tracker

`Sender` 是一个 C++17 写的轻量 VRPN Server。默认会生成 `uav0`、`uav1` ... 这样的 Tracker。现在额外支持：

- 自定义前缀 `--tracker-prefix`
- 随机运动模式 `--random-walk`
- 随机运动半径约束 `--random-radius`

### 编译 Sender

```bash
cd Sender
cmake -B build -S .
cmake --build build -j4
```

### Linux 上的推荐启动方式

仓库已经附带一个启动脚本：

```bash
cd Sender
./start_sunraynext_sender.sh
```

这个脚本会启动一个满足以下条件的发送端：

- 监听端口：`3883`
- Tracker 数量：`1`
- Tracker 名称：`sunraynext_uav0`
- 发送频率：`120 Hz`
- 运动模式：XY 平面半径 `1m` 内的有界随机游走
- 高度：固定 `z = 1.0`

等价命令如下：

```bash
./build/fake_vrpn_uav_server \
  --bind :3883 \
  --num-trackers 1 \
  --tracker-prefix sunraynext_uav \
  --rate 120 \
  --random-walk \
  --random-radius 1.0 \
  --status-mode inline \
  --status-interval 1
```

### 发送内容说明

该 sender 会对局域网提供一个名为：

- `sunraynext_uav0`

的 VRPN Tracker。

它的特性是：

- `x, y` 在半径 `1m` 圆内运动
- `z = 1.0`
- 姿态四元数会根据平面运动方向自动生成 yaw
- 发送频率为 `120Hz`

如果局域网内另一台机器的 IP 为 `<sender-ip>`，客户端可以按下面的地址订阅：

- `sunraynext_uav0@<sender-ip>:3883`

## Receiver：把 VRPN 转发成 MAVLink

`Receiver` 用于连接某个指定 Tracker，然后把它转成 MAVLink `VISION_POSITION_ESTIMATE`，通过串口或 UDP 发给飞控。

典型启动方式：

```bash
cd Receiver
cmake -B build -S .
cmake --build build -j4
./build/vrpn_receiver \
  --tracker sunraynext_uav0 \
  --host 127.0.0.1 \
  --port 3883 \
  --link serial \
  --device /dev/ttyUSB0 \
  --baud 57600 \
  --log-poses
```

如果要走 UDP：

```bash
./build/vrpn_receiver \
  --tracker sunraynext_uav0 \
  --host 127.0.0.1 \
  --port 3883 \
  --link udp \
  --udp-target 127.0.0.1:14550
```

## 常见问题

### 1. CMake 找不到 VRPN

先确认你是否安装了：

```bash
sudo apt install -y ros-jazzy-vrpn
```

然后确认这些文件存在：

```bash
ls /opt/ros/jazzy/include/vrpn_Connection.h
ls /opt/ros/jazzy/lib/libvrpn.a
ls /opt/ros/jazzy/lib/libquat.a
```

### 2. 3883 端口被占用

如果启动时看到端口绑定失败，可以检查：

```bash
ss -ltnp | grep 3883
```

然后关闭旧进程，或改用其他端口：

```bash
./build/fake_vrpn_uav_server --bind :4000 ...
```

### 3. 如何后台运行 Sender

```bash
cd Sender
nohup ./start_sunraynext_sender.sh > /tmp/sunraynext_vrpn_sender.log 2>&1 < /dev/null &
```

查看日志：

```bash
tail -f /tmp/sunraynext_vrpn_sender.log
```

停止进程：

```bash
pkill -f fake_vrpn_uav_server
```

## 说明

当前仓库已经补充了 Ubuntu 24 + ROS Jazzy 场景下的 Sender 配置路径，以及 `sunraynext_uav0` 的 Linux 启动脚本。后续如果继续扩展，可以再把：

- systemd 服务文件
- 多 tracker 随机运动配置
- 局域网自动发现
- 更丰富的轨迹模型

继续补进来。