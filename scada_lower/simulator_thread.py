"""
设备模拟器更新线程
每200ms更新所有设备模拟器的状态，并将数据写入ModbusDataStore
"""

from PyQt5.QtCore import QThread, pyqtSignal
import time


class SimulatorUpdateThread(QThread):
    """模拟器更新线程"""

    updated = pyqtSignal()

    def __init__(self, data_store, simulators, parent=None):
        super().__init__(parent)
        self.data_store = data_store
        self.simulators = simulators
        self.running = False
        self.update_interval = 0.2  # 200ms

    def run(self):
        """线程主循环"""
        self.running = True
        while self.running:
            for slave_id, simulator in self.simulators.items():
                simulator.update()
                data = simulator.get_data()
                self._write_to_datastore(slave_id, data)
            
            self.updated.emit()
            time.sleep(self.update_interval)

    def _write_to_datastore(self, slave_id, data):
        """将模拟器数据写入ModbusDataStore"""
        # 寄存器地址映射:
        # 0: 压力 (bar * 100) / 进口压力
        # 1: 温度 (℃ * 10) / 出口压力
        # 2: 运行状态 (0=停机, 1=运行, 2=故障)
        # 3: 加载/卸载状态 / 露点温度
        # 4: 电流 (A * 10) / 再生状态
        # 5: 故障码
        
        # 写入保持寄存器
        if 'pressure' in data:
            self.data_store.set_holding_register(slave_id, 0, data['pressure'])
        if 'inlet_pressure' in data:
            self.data_store.set_holding_register(slave_id, 0, data['inlet_pressure'])
            
        if 'temperature' in data:
            self.data_store.set_holding_register(slave_id, 1, data['temperature'])
        if 'outlet_pressure' in data:
            self.data_store.set_holding_register(slave_id, 1, data['outlet_pressure'])
            
        if 'run_state' in data:
            self.data_store.set_holding_register(slave_id, 2, data['run_state'])
            
        if 'load_state' in data:
            self.data_store.set_holding_register(slave_id, 3, data['load_state'])
        if 'dew_point' in data:
            self.data_store.set_holding_register(slave_id, 3, data['dew_point'])
            
        if 'current_amp' in data:
            self.data_store.set_holding_register(slave_id, 4, data['current_amp'])
        if 'regen_state' in data:
            self.data_store.set_holding_register(slave_id, 4, data['regen_state'])
            
        if 'fault_code' in data:
            self.data_store.set_holding_register(slave_id, 5, data['fault_code'])
        
        # 写入输入寄存器（只读镜像）
        if 'pressure' in data:
            self.data_store.set_input_register(slave_id, 0, data['pressure'])
        if 'inlet_pressure' in data:
            self.data_store.set_input_register(slave_id, 0, data['inlet_pressure'])
            
        if 'temperature' in data:
            self.data_store.set_input_register(slave_id, 1, data['temperature'])
        if 'outlet_pressure' in data:
            self.data_store.set_input_register(slave_id, 1, data['outlet_pressure'])
            
        if 'run_state' in data:
            self.data_store.set_input_register(slave_id, 2, data['run_state'])

    def stop(self):
        """停止线程"""
        self.running = False
