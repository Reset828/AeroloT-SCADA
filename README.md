# AeroloT SCADA

面向空压站的物联网集控 SCADA 示例项目，包含 Qt 6/C++ 上位机、Python 下位机模拟器和 Flutter 移动端。系统面向 3 台空压机、1 台储气罐和 1 台干燥机，提供 Modbus TCP 数据采集、告警、历史数据与移动端实时数据镜像。

## 项目结构

```text
.
├── scada_upper/   Qt 6 上位机：Modbus TCP 客户端、数据存储、告警与 WebSocket 服务端
├── scada_lower/   PyQt5 下位机模拟器：5 个 Modbus 从站及故障注入
└── app/           Flutter 移动端：连接上位机 WebSocket 并展示数据
```

## 功能概览

- 上位机采用 C++17、Qt 6 和 CMake，分模块实现 Modbus 通信、设备数据映射、SQLite 数据存储、告警处理和桌面界面。
- Modbus 采集采用异步线程与周期扫描，支持连接状态管理及重连。
- SQLite 使用 WAL 模式保存历史数据、告警及通信记录。
- WebSocket 服务默认监听 `18080` 端口，为移动端提供实时数据和心跳通信。
- 下位机模拟器提供 5 个设备从站、实时工艺数据和超限、抖动、稳定故障等测试场景。
- Flutter App 可连接 WebSocket 服务，适用于 Android、iOS、Windows、Linux、macOS 和 Web 等 Flutter 支持的平台（具体构建需相应平台工具链）。

## 环境要求

- Qt 上位机：CMake 3.16+、C++17 编译器，以及包含 `Core`、`SerialBus`、`Sql`、`Network`、`WebSockets`、`Widgets`、`PrintSupport`、`Multimedia` 和 `Test`（构建测试时）组件的 Qt 6。
- 下位机模拟器：Python 3、PyQt5、pymodbus 3.x。
- 移动端：Flutter SDK，以及目标平台所需的 Android Studio/Android SDK、Xcode 等工具。

## 运行

### 1. 启动下位机模拟器

在项目根目录运行：

```bash
python -m venv .venv
```

Windows PowerShell：

```powershell
.venv\Scripts\Activate.ps1
pip install PyQt5 "pymodbus>=3.0"
python scada_lower/main.py
```

macOS/Linux：

```bash
source .venv/bin/activate
pip install PyQt5 "pymodbus>=3.0"
python scada_lower/main.py
```

启动后在模拟器界面点击“启动服务器”。默认端口为 `502`；若系统权限或端口占用导致启动失败，可在界面选择其他端口。上位机当前默认连接 `127.0.0.1:1502`，因此本机联调时请将模拟器端口设置为 `1502`。跨设备使用时需相应调整连接地址和网络防火墙配置。

### 2. 构建并启动 Qt 上位机

将 `<Qt安装目录>` 替换为本机 Qt 安装中对应编译器套件的目录。例如 Windows MSVC 可使用 `C:/Qt/6.x.x/msvc2022_64`：

```bash
cmake -S scada_upper -B scada_upper/build -DCMAKE_PREFIX_PATH="<Qt安装目录>" -DBUILD_TESTS=ON
cmake --build scada_upper/build --config Debug
```

应用目标名为 `AeroloT_SCADA`。Windows 多配置生成器通常输出到 `scada_upper/build/Debug/AeroloT_SCADA.exe`；其他生成器和配置的输出位置可能不同。启动应用后通过界面连接下位机。上位机默认连接 `127.0.0.1:1502`，WebSocket 服务端口为 `18080`。

运行自动化测试：

```bash
ctest --test-dir scada_upper/build -C Debug --output-on-failure
```

### 3. 启动 Flutter 移动端

```bash
cd app
flutter pub get
flutter run
```

移动端 WebSocket 地址需指向运行上位机的主机，默认配置地址为 `ws://192.168.1.100:18080`，请按实际网络修改。Android 模拟器访问宿主机时通常使用 `10.0.2.2`；通过 USB 调试连接实体 Android 设备时，可使用：

```bash
adb reverse tcp:18080 tcp:18080
```

然后将 App 服务地址设为 `ws://127.0.0.1:18080`。更多连接排查说明见 [app/CONNECT_GUIDE.txt](app/CONNECT_GUIDE.txt)。

## 注意事项

- 本项目主要用于演示和联调。连接真实 PLC 前，请核对现场网络、安全策略、Modbus 寄存器映射及设备控制风险。
- WebSocket 当前使用非 TLS 的 `ws://`，不应直接暴露到不可信网络。
- 不要提交本机配置、构建输出、数据库、运行日志或包含现场敏感信息的文件。
