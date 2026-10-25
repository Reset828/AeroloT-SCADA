"""
储气罐模拟器
模拟储气罐的压力和温度变化
"""

import random
import time


class TankSimulator:
    """储气罐模拟器类"""

    def __init__(self, slave_id, initial_pressure=8.0):
        self.slave_id = slave_id
        self.current_pressure = initial_pressure
        self.current_temperature = 35.0
        
        # 运行状态: 0=停机, 1=运行, 2=故障
        self.run_state = 1
        
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
        
        if self.overpressure_mode:
            self.current_pressure = 10.5 + random.uniform(-0.3, 0.3)
            self.current_temperature += random.uniform(-0.1, 0.1)
        else:
            self.current_pressure += random.uniform(-0.02, 0.02)
            self.current_temperature += random.uniform(-0.1, 0.1)
        
        self.current_pressure = max(4.0, min(12.0, self.current_pressure))
        self.current_temperature = max(25.0, min(60.0, self.current_temperature))

    def force_overpressure(self):
        """强制压力超限"""
        self.overpressure_mode = True
        self.forced_fault = False
        self.current_pressure = 10.5

    def force_jitter(self):
        """制造抖动数据"""
        self.overpressure_mode = False
        self.forced_fault = False
        self.current_pressure = 9.8 + random.uniform(-0.3, 0.3)

    def force_stable_overlimit(self):
        """制造稳定故障"""
        self.overpressure_mode = False
        self.forced_fault = True
        self.current_pressure = 10.5
        self.current_temperature = 55.0
        self.run_state = 2

    def restore_normal(self):
        """恢复正常值"""
        self.overpressure_mode = False
        self.forced_fault = False
        self.current_pressure = 8.0
        self.current_temperature = 35.0
        self.run_state = 1

    def set_run_state(self, state):
        """设置运行状态 (0=停机, 1=运行, 2=故障)"""
        self.run_state = state

    def get_data(self):
        """获取当前数据"""
        return {
            'pressure': int(self.current_pressure * 100),
            'temperature': int(self.current_temperature * 10),
            'status': self.run_state
        }
