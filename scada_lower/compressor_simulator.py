"""
空压机模拟器
模拟空压机的物理运行特性
"""

import random
import time


class CompressorSimulator:
    """空压机模拟器类"""

    def __init__(self, slave_id, target_pressure=7.5):
        self.slave_id = slave_id
        self.target_pressure = target_pressure
        
        # 压力相关
        self.current_pressure = 7.0  # 当前压力 (bar)
        self.pressure_trend = 0.01   # 压力变化趋势
        
        # 温度相关
        self.current_temperature = 75.0  # 当前温度 (℃)
        
        # 运行状态: 0=停机, 1=运行, 2=故障
        self.run_state = 1  # 默认运行
        self.load_state = 1  # 0=卸载, 1=加载
        
        # 电流
        self.current_amp = 15.0  # 运行电流 (A)
        
        # 故障码
        self.fault_code = 0
        
        # 故障模式标志
        self.overpressure_mode = False  # 压力超限模式
        self.forced_fault = False       # 稳定故障模式
        
        self.last_update_time = time.time()

    def update(self):
        """更新模拟器状态，每200ms调用"""
        if self.forced_fault:
            return
        
        current_time = time.time()
        delta_time = current_time - self.last_update_time
        self.last_update_time = current_time
        
        if self.run_state == 0:  # 停机
            self.current_pressure -= 0.02
            self.current_temperature -= 0.3
            self.current_amp = 0
            self.load_state = 0
        elif self.run_state == 1:  # 运行
            if self.overpressure_mode:
                self.current_pressure = 10.5 + random.uniform(-0.3, 0.3)
                self.current_temperature += 0.05 + random.uniform(-0.02, 0.02)
                self.current_amp = 18.0 + random.uniform(-0.5, 0.5)
            else:
                if self.load_state == 1:
                    self.current_pressure += self.pressure_trend + random.uniform(-0.01, 0.01)
                else:
                    self.current_pressure -= 0.01 + random.uniform(-0.005, 0.005)
                
                if self.current_pressure >= 8.0:
                    self.load_state = 0
                elif self.current_pressure <= 6.0:
                    self.load_state = 1
                
                if self.load_state == 1:
                    self.current_temperature += 0.05 + random.uniform(-0.02, 0.02)
                else:
                    self.current_temperature -= 0.05 + random.uniform(-0.02, 0.02)
                
                self.current_temperature = max(70.0, min(80.0, self.current_temperature))
                
                self.current_amp = 12.0 if self.load_state == 0 else 18.0
                self.current_amp += random.uniform(-0.5, 0.5)
        elif self.run_state == 2:  # 故障
            self.current_amp = 0
            self.load_state = 0
        
        self.current_pressure = max(0, min(12, self.current_pressure))

    def force_overpressure(self):
        """强制压力超限"""
        self.overpressure_mode = True
        self.forced_fault = False
        self.current_pressure = 10.5

    def force_jitter(self):
        """制造抖动数据"""
        self.overpressure_mode = False
        self.forced_fault = False
        self.current_pressure = 7.8 + random.uniform(-0.3, 0.3)
        self.current_temperature = 89 + random.uniform(-2, 2)

    def force_stable_overlimit(self):
        """制造稳定故障"""
        self.overpressure_mode = False
        self.forced_fault = True
        self.current_pressure = 10.5
        self.current_temperature = 95.0
        self.run_state = 2
        self.fault_code = 1

    def restore_normal(self):
        """恢复正常值"""
        self.overpressure_mode = False
        self.forced_fault = False
        self.current_pressure = 7.0
        self.current_temperature = 75.0
        self.run_state = 1
        self.fault_code = 0
        self.load_state = 1

    def set_run_state(self, state):
        """设置运行状态 (0=停机, 1=运行, 2=故障)"""
        self.run_state = state
        if state == 2:
            self.fault_code = 1
        else:
            self.fault_code = 0

    def get_data(self):
        """获取当前数据"""
        return {
            'pressure': int(self.current_pressure * 100),
            'temperature': int(self.current_temperature * 10),
            'run_state': self.run_state,
            'load_state': self.load_state,
            'current_amp': int(self.current_amp * 10),
            'fault_code': self.fault_code
        }
