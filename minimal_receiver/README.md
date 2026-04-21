# minimal_receiver

`minimal_receiver/` 是一个完全独立构建的最小 VRPN 客户端目录。它只做一件事：连接某个 VRPN Tracker，把收到的位置和姿态持续打印到终端，方便你在 Linux 上先把“能连上、能收到、格式对不对”这件事确认下来。

这里的 **VRPN** 是一种常见的动捕/传感器网络协议。这里的 **Tracker** 是 VRPN 里表示某个被跟踪对象的数据流名字，例如 `sunraynext_uav0`。本目录不会发送 MAVLink，也不会依赖仓库里的 `Receiver/` 目录。

## 目录结构

```text
minimal_receiver/
├── CMakeLists.txt
├── README.md
├── cmake/
│   └── FindVRPN.cmake
├── include/
│   └── minimal_receiver/
│       └── TrackerClient.h
└── src/
    ├── TrackerClient.cpp
    └── main.cpp
```

## 这个程序会做什么

程序启动后会：

1. 连接 `--tracker@--host:--port`
2. 持续轮询 VRPN 数据
3. 把四元数转换为 `roll/pitch/yaw`
4. 在终端打印：
   - 时间戳 `ts`
   - 位置 `pos=(x, y, z)`
   - 姿态 `rpy=(roll, pitch, yaw)`
   - 累计消息数 `count`
   - 最近约 1 秒窗口估算频率 `hz`

## Linux 环境准备

下面以 Ubuntu / Debian 系为主说明。其他发行版也可以，只要能提供：

- C++17 编译器
- CMake 3.15+
- VRPN 头文件和库文件

### 1. 安装基础编译工具

```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config
```

### 2. 安装 VRPN

优先尝试系统包：

```bash
sudo apt install -y libvrpn-dev
```

如果你的发行版没有这个包，或者版本过老，可以改用源码安装。安装到 `/usr/local` 后，本目录自带的 `FindVRPN.cmake` 会自动搜索 `/usr/local/include` 和 `/usr/local/lib`。

源码安装示例：

```bash
git clone --recursive https://github.com/vrpn/vrpn.git
cd vrpn
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
sudo cmake --install build
```

如果你机器上已经通过 ROS 或其他方式把 VRPN 装到了这些路径，本目录也会自动尝试查找：

- `/opt/ros/jazzy/include`
- `/opt/ros/jazzy/lib`
- `/usr/include`
- `/usr/lib`
- `/usr/lib/x86_64-linux-gnu`
- `/usr/lib/aarch64-linux-gnu`
- `/usr/local/include`
- `/usr/local/lib`

### 3. 确认 VRPN 已安装

至少确认头文件和库文件存在一组：

```bash
ls /usr/include/vrpn_Connection.h
ls /usr/lib/x86_64-linux-gnu/libvrpn.so
```

或者：

```bash
ls /usr/local/include/vrpn_Connection.h
ls /usr/local/lib/libvrpn.so
```

如果你的系统把 `quat` 单独拆成库，也一并确认：

```bash
ls /usr/lib/x86_64-linux-gnu/libquat.so
```

## 编译

在仓库根目录执行：

```bash
cd minimal_receiver
cmake -B build -S .
cmake --build build -j"$(nproc)"
```

编译成功后，可执行文件位于：

```bash
./build/vrpn_pose_monitor
```

## 命令行参数

```text
Usage: vrpn_pose_monitor --tracker <name> [options]
Options:
  --tracker <name>   Tracker name, e.g. sunraynext_uav0
  --host <addr>      VRPN host (default 127.0.0.1)
  --port <port>      VRPN port (default 3883)
  --sample-ms <ms>   Poll interval in milliseconds (default 2)
  --help             Show this help
```

参数说明：

- `--tracker`：必须提供。要订阅的 Tracker 名字。
- `--host`：VRPN 服务端 IP 或主机名。`localhost` 和 `::1` 会被自动规整成 `127.0.0.1`。
- `--port`：VRPN 服务端端口，默认 `3883`。
- `--sample-ms`：每次轮询之间的休眠时间，默认 `2ms`。如果只是做链路确认，保持默认即可。

## 运行方式

### 场景 A：直接对接你自己的 VRPN 服务端

如果你已经有一个 VRPN Server，知道它的：

- Tracker 名字，例如 `sunraynext_uav0`
- 服务器地址，例如 `192.168.10.32`
- 端口，例如 `3883`

那么直接运行：

```bash
cd minimal_receiver
./build/vrpn_pose_monitor \
  --tracker sunraynext_uav0 \
  --host 192.168.10.32 \
  --port 3883
```

### 场景 B：用本仓库里的 Sender 做本地联调

如果你只是想先在一台 Linux 机器上自测，可以直接用仓库里的 `Sender/` 来造一个 VRPN 数据源。

先编译 Sender：

```bash
cd Sender
cmake -B build -S .
cmake --build build -j"$(nproc)"
```

启动一个单 Tracker 的 VRPN 发送端：

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

这个命令会提供一个名字为 `sunraynext_uav0` 的 Tracker。

然后在另一个终端运行：

```bash
cd minimal_receiver
./build/vrpn_pose_monitor \
  --tracker sunraynext_uav0 \
  --host 127.0.0.1 \
  --port 3883
```

## 运行输出示例

输出看起来会像这样：

```text
ts=1713676201.123456 | pos=(0.3274, -0.1182, 1.0000) | rpy=(0.0000, -0.0000, 1.2483) | count=42 | hz=119.84
ts=1713676201.131773 | pos=(0.3331, -0.1105, 1.0000) | rpy=(0.0000, -0.0000, 1.2744) | count=43 | hz=119.85
```

字段含义：

- `ts`：VRPN 消息自带时间戳，单位秒。
- `pos`：位置，单位通常由你的 VRPN 上游系统决定，常见是米。
- `rpy`：由四元数转换得到的欧拉角，单位弧度。
- `count`：从程序启动开始累计收到的有效消息数。
- `hz`：最近约一秒窗口内估算出来的消息频率。

## 常见问题

### 1. CMake 报错，找不到 VRPN

先确认头文件和库文件是否真的装到了系统里。

可以直接搜索：

```bash
find /usr /usr/local /opt/ros -name 'vrpn_Connection.h' 2>/dev/null
find /usr /usr/local /opt/ros -name 'libvrpn*' 2>/dev/null
find /usr /usr/local /opt/ros -name 'libquat*' 2>/dev/null
```

如果库不在默认路径，可以显式指定：

```bash
cd minimal_receiver
cmake -B build -S . \
  -DVRPN_INCLUDE_DIR=/custom/prefix/include \
  -DVRPN_vrpn_LIBRARY=/custom/prefix/lib/libvrpn.so \
  -DVRPN_quat_LIBRARY=/custom/prefix/lib/libquat.so
```

如果你的系统没有单独的 `libquat.so`，可以先只指定前两个变量再试一次。

### 2. 程序启动了，但一直没有输出

这通常说明连接成功了，但没有收到指定 Tracker 的数据。按顺序检查：

1. `--tracker` 名字是否和服务端完全一致。
2. `--host` 和 `--port` 是否正确。
3. 服务端是否真的在持续发数据。
4. 本机或局域网防火墙是否拦住了端口。

可以先确认端口是否监听：

```bash
ss -ltnp | grep 3883
```

### 3. 只有本机能连，别的机器连不上

通常是服务端只绑定在本地地址，或者防火墙没有放行对应端口。对端机器可以先测试网络连通性：

```bash
ping <server-ip>
nc -vz <server-ip> 3883
```

### 4. 输出频率不稳定

先分清两个频率：

- 上游 VRPN Server 实际发送频率
- 本程序终端里估算出来的 `hz`

`hz` 是接收侧近似估算值，受线程调度、打印开销和系统负载影响。若只是确认链路是否正常，这个值小范围波动是正常的。

### 5. 如何退出

前台运行时直接按：

```bash
Ctrl+C
```

程序会捕获 `SIGINT` / `SIGTERM` 并正常退出。

## 最小工作流总结

如果你只想最快确认 Linux 侧接收是否正常，按下面顺序走就够了：

1. 安装 `build-essential`、`cmake` 和 VRPN。
2. `cd minimal_receiver && cmake -B build -S . && cmake --build build`
3. 准备一个可用的 VRPN Server。
4. 运行 `./build/vrpn_pose_monitor --tracker <name> --host <ip> --port <port>`
5. 看到终端持续打印 `ts / pos / rpy / count / hz`，说明接收链路打通。
