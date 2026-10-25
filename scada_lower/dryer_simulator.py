"""
干燥机模拟器
模拟干燥机的运行状态、露点、进出口压力
"""

import random
import time


class DryerSimulator:
    """干燥机模拟器类"""

    def __init__(self, slave_id):
        self.slave_id = slave_id
        
        # 进出口压力
        self.inlet_pressure = 8.0
        self.outlet_pressure = 7.8
        
        # 露点温度
        self.dew_point = -40.0
        
        # 运行状态: 0=停止, 1=运行, 2=故障
        self.run_state = 1
        
        # 再生状态: 0=除湿中, 1=再生中
        self.regen_state = 0
        
        # 故障模式标志
        self.overlimit_mode = False   # 露点超限模式
        self.forced_fault = False     # 稳定故障模式
        
        self.last_update_time = time.time()
        self.cycle_time = 0

    def update(self):
        """更新模拟器状态，每200ms调用"""
        if self.forced_fault:
            return
            
        current_time = time.time()
        delta_time = current_time - self.last_update_time
        self.last_update_time = current_time
        
        if self.run_state == 0:  # 停止
            return
        
        # 状态切换（每30秒切换一次再生状态）
        self.cycle_time += delta_time
        if self.cycle_time >= 30:
            self.regen_state = 1 - self.regen_state
            self.cycle_time = 0
        
        # 进出口压力小幅波动
        self.inlet_pressure += random.uniform(-0.02, 0.02)
        self.outlet_pressure = self.inlet_pressure - 0.2 + random.uniform(-0.05, 0.05)
        
        # 露点温度
        if self.overlimit_mode:
            self.dew_point = -15.0 + random.uniform(-2, 2)
        else:
            if self.regen_state == 0:  # 除湿中
                self.dew_point = -40.0 + random.uniform(-1, 1)
            else:  # 再生中
                self.dew_point = -30.0 + random.uniform(-1, 1)
            
            self.dew_point = max(-40.0, min(-20.0, self.dew_point))
        
        # 限制范围
        self.inlet_pressure = max(6.0, min(10.0, self.inlet_pressure))
        self.outlet_pressure = max(5.8, min(9.8, self.outlet_pressure))

    def force_overlimit(self):
        """强制露点超限"""
        self.overlimit_mode = True
        self.forced_fault = False
        self.dew_point = -15.0

    def force_jitter(self):
        """制造抖动数据"""
        self.overlimit_mode = False
        self.forced_fault = False
        self.dew_point = -25 + random.uniform(-5, 5)

    def force_stable_overlimit(self):
        """制造稳定故障"""
        self.overlimit_mode = False
        self.forced_fault = True
        self.dew_point = -10.0
        self.run_state = 2

    def restore_normal(self):
        """恢复正常值"""
        self.overlimit_mode = False
        self.forced_fault = False
        self.inlet_pressure = 8.0
        self.outlet_pressure = 7.8
        self.dew_point = -40.0
        self.run_state = 1
        self.regen_state = 0

    def set_run_state(self, state):
        """设置运行状态"""
        self.run_state = state

    def get_data(self):
        """获取当前数据"""
        return {
            'inlet_pressure': int(self.inlet_pressure * 100),
            'outlet_pressure': int(self.outlet_pressure * 100),
            'dew_point': int(self.dew_point * 10),
            'run_state': self.run_state,
            'regen_state': self.regen_state
        }
