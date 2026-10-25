"""
Modbus从站模拟器主窗口
简洁整齐的UI界面，5个设备监控
"""

import sys
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QTabWidget, QLabel, QPushButton, QTableWidget, QTableWidgetItem,
    QGroupBox, QMessageBox, QHeaderView, QSpinBox, QComboBox
)
from PyQt5.QtCore import Qt, QTimer
from PyQt5.QtGui import QFont, QColor

from modbus_data_store import ModbusDataStore
from modbus_server import ModbusServerManager
from compressor_simulator import CompressorSimulator
from tank_simulator import TankSimulator
from dryer_simulator import DryerSimulator
from simulator_thread import SimulatorUpdateThread


class DeviceTabWidget(QWidget):
    """设备标签页控件"""

    def __init__(self, slave_id, device_name, data_store, main_window, parent=None):
        super().__init__(parent)
        self.slave_id = slave_id
        self.device_name = device_name
        self.data_store = data_store
        self.main_window = main_window
        self.regen_combo = None
        self.init_ui()

    def init_ui(self):
        """初始化UI"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 10, 10, 10)
        layout.setSpacing(10)

        # 标题
        title_label = QLabel(f"{self.device_name} (从站 {self.slave_id})")
        title_font = QFont()
        title_font.setPointSize(14)
        title_font.setBold(True)
        title_label.setFont(title_font)
        title_label.setAlignment(Qt.AlignCenter)
        layout.addWidget(title_label)

        # 数据表格
        data_group = QGroupBox("实时数据")
        data_layout = QVBoxLayout()
        
        self.data_table = QTableWidget(8, 3)
        self.data_table.setHorizontalHeaderLabels(["参数", "数值", "单位"])
        self.data_table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.data_table.setAlternatingRowColors(True)
        self.data_table.setEditTriggers(QTableWidget.NoEditTriggers)
        data_layout.addWidget(self.data_table)
        
        data_group.setLayout(data_layout)
        layout.addWidget(data_group)

        # 控制面板
        control_group = QGroupBox("故障注入控制")
        control_layout = QVBoxLayout()
        
        # 第一行按钮
        btn_layout1 = QHBoxLayout()
        
        self.btn_overlimit = QPushButton("⚠ 强制超限")
        self.btn_overlimit.setStyleSheet("background-color: #E74C3C; color: white; font-weight: bold; padding: 8px;")
        self.btn_overlimit.clicked.connect(self.force_overlimit)
        
        self.btn_jitter = QPushButton("〰 制造抖动")
        self.btn_jitter.setStyleSheet("background-color: #F39C12; color: white; font-weight: bold; padding: 8px;")
        self.btn_jitter.clicked.connect(self.force_jitter)
        
        btn_layout1.addWidget(self.btn_overlimit)
        btn_layout1.addWidget(self.btn_jitter)
        
        # 第二行按钮
        btn_layout2 = QHBoxLayout()
        
        self.btn_stable_fault = QPushButton("🔴 稳定故障")
        self.btn_stable_fault.setStyleSheet("background-color: #C0392B; color: white; font-weight: bold; padding: 8px;")
        self.btn_stable_fault.clicked.connect(self.force_stable_fault)
        
        self.btn_restore = QPushButton("✅ 恢复正常")
        self.btn_restore.setStyleSheet("background-color: #27AE60; color: white; font-weight: bold; padding: 8px;")
        self.btn_restore.clicked.connect(self.restore_normal)
        
        btn_layout2.addWidget(self.btn_stable_fault)
        btn_layout2.addWidget(self.btn_restore)
        
        control_layout.addLayout(btn_layout1)
        control_layout.addLayout(btn_layout2)
        
        # 工作模式选择（仅干燥机）
        if self.slave_id == 5:
            regen_layout = QHBoxLayout()
            regen_layout.addWidget(QLabel("工作模式:"))
            
            self.regen_combo = QComboBox()
            self.regen_combo.addItems(["除湿中", "再生中"])
            self.regen_combo.currentIndexChanged.connect(self.on_regen_mode_changed)
            regen_layout.addWidget(self.regen_combo)
            regen_layout.addStretch()
            control_layout.addLayout(regen_layout)
        
        # 状态显示
        self.status_label = QLabel("状态: 正常运行")
        self.status_label.setStyleSheet("color: green; font-weight: bold; padding: 5px; font-size: 12px;")
        control_layout.addWidget(self.status_label)
        
        control_group.setLayout(control_layout)
        layout.addWidget(control_group)

        # 刷新按钮
        refresh_btn = QPushButton("🔄 刷新数据")
        refresh_btn.clicked.connect(self.refresh_data)
        refresh_btn.setStyleSheet("padding: 8px; font-weight: bold;")
        layout.addWidget(refresh_btn)

        # 初始化数据
        self.refresh_data()

    def _to_signed(self, value):
        """将无符号16位整数转换为有符号整数"""
        if value >= 32768:
            return value - 65536
        return value

    def refresh_data(self):
        """刷新显示数据"""
        hr = self.data_store.get_holding_registers(self.slave_id)
        
        # 根据设备类型显示不同参数
        if self.slave_id <= 3:  # 空压机
            params = [
                ("排气压力", hr[0] / 100.0, "bar"),
                ("排气温度", hr[1] / 10.0, "℃"),
                ("运行状态", hr[2], ""),
                ("加载/卸载", "加载" if hr[3] == 1 else "卸载", ""),
                ("运行电流", hr[4] / 10.0, "A"),
                ("故障码", hr[5], ""),
            ]
        elif self.slave_id == 4:  # 储气罐
            params = [
                ("罐压", hr[0] / 100.0, "bar"),
                ("温度", hr[1] / 10.0, "℃"),
                ("状态", hr[2], ""),
            ]
        else:  # 干燥机
            dew_point = self._to_signed(hr[3]) / 10.0
            params = [
                ("进口压力", hr[0] / 100.0, "bar"),
                ("出口压力", hr[1] / 100.0, "bar"),
                ("露点", dew_point, "℃"),
                ("运行状态", hr[2], ""),
                ("再生状态", "再生中" if hr[4] == 1 else "除湿中", ""),
            ]
        
        for i, (name, value, unit) in enumerate(params):
            self.data_table.setItem(i, 0, QTableWidgetItem(name))
            self.data_table.setItem(i, 1, QTableWidgetItem(str(value)))
            self.data_table.setItem(i, 2, QTableWidgetItem(unit))
            
            if self.slave_id == 5 and self.regen_combo is not None:
                regen_state = 1 if "再生中" in str(value) else 0
                if self.regen_combo.currentIndex() != regen_state:
                    self.regen_combo.blockSignals(True)
                    self.regen_combo.setCurrentIndex(regen_state)
                    self.regen_combo.blockSignals(False)
            
            # 超限高亮
            if isinstance(value, (int, float)):
                if self._is_alarm(name, value):
                    self.data_table.item(i, 1).setBackground(QColor(255, 200, 200))
                else:
                    self.data_table.item(i, 1).setBackground(QColor(255, 255, 255))

    def _is_alarm(self, param_name, value):
        """判断是否超限"""
        alarms = {
            "排气压力": lambda v: v > 10.0 or v < 4.0,
            "排气温度": lambda v: v > 90,
            "罐压": lambda v: v > 10.0,
            "露点": lambda v: v > -20,
        }
        check = alarms.get(param_name)
        return check(value) if check else False

    def _get_simulator(self):
        """获取当前设备的模拟器"""
        if self.main_window and hasattr(self.main_window, 'simulators'):
            return self.main_window.simulators.get(self.slave_id)
        return None

    def force_overlimit(self):
        """强制超限"""
        sim = self._get_simulator()
        if sim is None:
            return
        
        if self.slave_id <= 3:  # 空压机
            sim.force_overpressure()
            self.status_label.setText("状态: 压力超限")
        elif self.slave_id == 4:  # 储气罐
            sim.force_overpressure()
            self.status_label.setText("状态: 压力超限")
        else:  # 干燥机
            sim.force_overlimit()
            self.status_label.setText("状态: 露点超限")
        
        self.status_label.setStyleSheet("color: red; font-weight: bold; padding: 5px;")
        self.refresh_data()

    def force_jitter(self):
        """制造抖动"""
        sim = self._get_simulator()
        if sim is None:
            return
        
        sim.force_jitter()
        self.status_label.setText("状态: 数据抖动")
        self.status_label.setStyleSheet("color: orange; font-weight: bold; padding: 5px;")
        self.refresh_data()

    def force_stable_fault(self):
        """稳定故障"""
        sim = self._get_simulator()
        if sim is None:
            return
        
        sim.force_stable_overlimit()
        
        self.status_label.setText("状态: 稳定故障")
        self.status_label.setStyleSheet("color: darkred; font-weight: bold; padding: 5px;")
        self.refresh_data()

    def restore_normal(self):
        """恢复正常"""
        sim = self._get_simulator()
        if sim is None:
            return
        
        sim.restore_normal()
        self.status_label.setText("状态: 正常运行")
        self.status_label.setStyleSheet("color: green; font-weight: bold; padding: 5px;")
        
        if self.slave_id == 5 and hasattr(self, 'regen_combo'):
            self.regen_combo.setCurrentIndex(0)
        
        self.refresh_data()
    
    def on_regen_mode_changed(self, index):
        """切换再生/除湿模式"""
        if self.slave_id != 5:
            return
        
        sim = self._get_simulator()
        if sim is None:
            return
        
        sim.regen_state = index
        self.refresh_data()


class MainWindow(QMainWindow):
    """主窗口类"""

    def __init__(self):
        super().__init__()
        
        # 初始化数据存储和服务器
        self.data_store = ModbusDataStore()
        self.server_manager = ModbusServerManager(self.data_store)
        
        # 服务器状态
        self.server_running = False
        
        # 初始化设备模拟器
        self._init_simulators()
        
        # 初始化UI
        self.init_ui()
        
        # 定时器
        self.update_timer = QTimer()
        self.update_timer.timeout.connect(self.update_display)
        self.update_timer.start(500)

    def _init_simulators(self):
        """初始化所有设备模拟器"""
        self.simulators = {}
        
        # 从站1-3: 空压机
        for i in range(1, 4):
            self.simulators[i] = CompressorSimulator(i, target_pressure=7.5)
        
        # 从站4: 储气罐
        self.simulators[4] = TankSimulator(4, initial_pressure=8.0)
        
        # 从站5: 干燥机
        self.simulators[5] = DryerSimulator(5)
        
        # 创建模拟器更新线程
        self.update_thread = SimulatorUpdateThread(self.data_store, self.simulators)

    def init_ui(self):
        """初始化UI"""
        self.setWindowTitle("空压站 SCADA - Modbus从站模拟器")
        self.setGeometry(100, 100, 900, 700)
        
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        
        main_layout = QVBoxLayout(central_widget)
        main_layout.setContentsMargins(10, 10, 10, 10)
        main_layout.setSpacing(10)
        
        # 顶部控制栏
        control_layout = QHBoxLayout()
        
        # 服务器控制
        self.start_btn = QPushButton("▶ 启动服务器")
        self.start_btn.clicked.connect(self.start_server)
        self.start_btn.setStyleSheet("background-color: #27AE60; color: white; font-weight: bold; padding: 8px;")
        
        self.stop_btn = QPushButton("⏹ 停止服务器")
        self.stop_btn.clicked.connect(self.stop_server)
        self.stop_btn.setEnabled(False)
        self.stop_btn.setStyleSheet("background-color: #E74C3C; color: white; font-weight: bold; padding: 8px;")
        
        self.disconnect_btn = QPushButton("🔌 断开连接")
        self.disconnect_btn.clicked.connect(self.disconnect_server)
        self.disconnect_btn.setEnabled(False)
        self.disconnect_btn.setStyleSheet("background-color: #F39C12; color: white; font-weight: bold; padding: 8px;")
        
        self.relisten_btn = QPushButton("🔄 重新监听")
        self.relisten_btn.clicked.connect(self.relisten_server)
        self.relisten_btn.setStyleSheet("background-color: #3498DB; color: white; font-weight: bold; padding: 8px;")
        
        control_layout.addWidget(self.start_btn)
        control_layout.addWidget(self.stop_btn)
        control_layout.addWidget(self.disconnect_btn)
        control_layout.addWidget(self.relisten_btn)
        
        # 端口号
        port_label = QLabel("端口:")
        self.port_input = QSpinBox()
        self.port_input.setRange(1024, 65535)
        self.port_input.setValue(1502)  # 默认1502，避免502需要管理员权限
        self.port_input.setFixedWidth(80)
        control_layout.addWidget(port_label)
        control_layout.addWidget(self.port_input)
        
        # 状态显示
        self.status_label = QLabel("未启动")
        self.status_label.setStyleSheet("color: red; font-weight: bold; padding: 5px; font-size: 12px;")
        control_layout.addWidget(self.status_label)
        
        main_layout.addLayout(control_layout)
        
        # 设备标签页
        self.tabs = QTabWidget()
        
        devices = [
            (1, "空压机 1"),
            (2, "空压机 2"),
            (3, "空压机 3"),
            (4, "储气罐"),
            (5, "干燥机"),
        ]
        
        for slave_id, name in devices:
            tab = DeviceTabWidget(slave_id, name, self.data_store, self)
            self.tabs.addTab(tab, f"{name}")
        
        main_layout.addWidget(self.tabs)

    def start_server(self):
        """启动服务器"""
        try:
            port = self.port_input.value()
            self.server_manager.start_server(port=port)
            self.server_running = True
            
            self.start_btn.setEnabled(False)
            self.stop_btn.setEnabled(True)
            self.disconnect_btn.setEnabled(True)
            self.status_label.setText(f"运行中 (端口 {port})")
            self.status_label.setStyleSheet("color: green; font-weight: bold; padding: 5px;")
            
            # 启动模拟器
            if not self.update_thread.isRunning():
                self.update_thread.start()
                
        except Exception as e:
            QMessageBox.critical(self, "启动失败", f"无法启动服务器: {str(e)}")

    def stop_server(self):
        """停止服务器"""
        try:
            self.server_manager.stop_server()
            self.server_running = False
            
            self.start_btn.setEnabled(True)
            self.stop_btn.setEnabled(False)
            self.disconnect_btn.setEnabled(False)
            self.status_label.setText("已停止")
            self.status_label.setStyleSheet("color: gray; font-weight: bold; padding: 5px;")
            
            # 停止模拟器
            if self.update_thread.isRunning():
                self.update_thread.stop()
                self.update_thread.wait()
                
        except Exception as e:
            QMessageBox.warning(self, "停止失败", f"停止服务器时出错: {str(e)}")

    def disconnect_server(self):
        """断开连接（模拟网络中断）"""
        try:
            self.server_manager.stop_server()
            self.status_label.setText("已断开")
            self.status_label.setStyleSheet("color: orange; font-weight: bold; padding: 5px;")
            self.disconnect_btn.setEnabled(False)
        except Exception as e:
            QMessageBox.warning(self, "断开失败", str(e))

    def relisten_server(self):
        """重新监听端口"""
        try:
            port = self.port_input.value()
            self.server_manager.start_server(port=port)
            self.server_running = True
            
            self.start_btn.setEnabled(False)
            self.stop_btn.setEnabled(True)
            self.disconnect_btn.setEnabled(True)
            self.status_label.setText(f"重新监听中 (端口 {port})")
            self.status_label.setStyleSheet("color: green; font-weight: bold; padding: 5px;")
            
        except Exception as e:
            QMessageBox.critical(self, "重新监听失败", str(e))

    def update_display(self):
        """更新显示"""
        current_tab = self.tabs.currentIndex()
        if current_tab >= 0:
            widget = self.tabs.widget(current_tab)
            if hasattr(widget, 'refresh_data'):
                widget.refresh_data()

    def closeEvent(self, event):
        """窗口关闭事件"""
        if self.server_running:
            self.stop_server()
        event.accept()


def main():
    """主函数"""
    app = QApplication(sys.argv)
    app.setStyle('Fusion')
    
    window = MainWindow()
    window.show()
    
    sys.exit(app.exec_())


if __name__ == '__main__':
    main()
