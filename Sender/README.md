# Sender（VRPN 发送端）

`Sender` 是一个轻量级的 C++17 VRPN Server，用来在一台机器上模拟一个或多个 Tracker，并通过局域网向外提供 VRPN 数据。

当前已经支持两类运动模式：

- 默认模式：确定性的圆周运动
- 随机模式：在指定半径内进行有界随机游走

## 依赖

### macOS

```bash
brew install vrpn cmake
```

### Ubuntu 24 / Debian 系

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build
sudo apt install -y ros-jazzy-vrpn
```

说明：

- 在 Ubuntu 24 上，`libvrpn-dev` 可能不可用。
- 当前仓库已经兼容从 `/opt/ros/jazzy` 查找 VRPN 头文件和静态库。

## 编译

```bash
cd Sender
cmake -B build -S .
cmake --build build -j4
```

生成的二进制为：

```bash
build/fake_vrpn_uav_server
```

## 启动

```bash
./build/fake_vrpn_uav_server [options]
```

### 常用参数

| 参数 | 作用 |
| ---- | ---- |
| `-b`, `--bind <addr>` | VRPN 绑定地址，默认 `:3883` |
| `-n`, `--num-trackers <N>` | 生成多少个 tracker，默认 32 |
| `-r`, `--rate <Hz>` | 发送频率，默认 50Hz |
| `--tracker-prefix <s>` | tracker 名前缀，默认 `uav`，因此会生成 `uav0`、`uav1` ... |
| `--random-walk` | 启用随机游走模式 |
| `--random-radius <m>` | 随机运动半径，默认 1.0m |
| `-q`, `--quiet` | 静默模式，抑制周期日志 |
| `--status-interval <s>` | 状态日志输出周期 |
| `--status-mode append|inline` | 状态输出方式 |
| `--status-tracker <idx>` | 日志里显示哪个 tracker 的位姿 |
| `--status-no-pose` | 状态日志里不输出位姿 |
| `--auto-restart` | 连接失败时自动重启 |
| `--restart-delay <s>` | 自动重启延迟 |

## SunrayNext 场景：发送 `sunraynext_uav0`

如果你的目标是局域网内发送一个名为 `sunraynext_uav0` 的 VRPN Tracker，内容在半径 1m 的圆内随机运动，频率 120Hz，那么直接运行：

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

这会产生：

- Tracker 名称：`sunraynext_uav0`
- VRPN 端口：`3883`
- 发布频率：`120Hz`
- 位置：`x, y` 在半径 `1m` 的圆内随机游走
- 高度：`z = 1.0`
- 姿态：根据平面运动方向生成 yaw 四元数

## 仓库内附带启动脚本

已经提供现成脚本：

```bash
./start_sunraynext_sender.sh
```

它等价于上面的 SunrayNext 场景启动命令。

## 客户端订阅方式

假设 Sender 所在主机 IP 是：

```text
192.168.10.32
```

那么客户端可订阅：

```text
sunraynext_uav0@192.168.10.32:3883
```

如果在同机测试，也可以直接订阅：

```text
sunraynext_uav0@127.0.0.1:3883
```

## 后台运行

### 后台启动

```bash
nohup ./start_sunraynext_sender.sh > /tmp/sunraynext_vrpn_sender.log 2>&1 < /dev/null &
```

### 查看日志

```bash
tail -f /tmp/sunraynext_vrpn_sender.log
```

### 停止进程

```bash
pkill -f fake_vrpn_uav_server
```

## 常见问题

### 1. 端口绑定失败

检查 `3883` 是否被占用：

```bash
ss -ltnp | grep 3883
```

如已被占用，可以停止旧进程，或者改用其他端口：

```bash
./build/fake_vrpn_uav_server --bind :4000 ...
```

### 2. CMake 找不到 VRPN

确认这几个文件存在：

```bash
ls /opt/ros/jazzy/include/vrpn_Connection.h
ls /opt/ros/jazzy/lib/libvrpn.a
ls /opt/ros/jazzy/lib/libquat.a
```

### 3. 如何查看完整参数帮助

```bash
./build/fake_vrpn_uav_server --help
```
