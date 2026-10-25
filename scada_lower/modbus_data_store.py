"""
Modbus数据存储 - pymodbus 3.x 兼容版
每个从站独立的 ModbusBaseDeviceContext 实现
"""

from pymodbus.datastore import ModbusBaseSlaveContext


def to_uint16(value):
    """将有符号整数转换为无符号16位整数（补码）"""
    if value < 0:
        return value & 0xFFFF
    return value


class SlaveContext(ModbusBaseSlaveContext):
    """
    单个从站的数据上下文
    兼容 pymodbus 3.x API: getValues(func_code, address, count)
    """

    def __init__(self, slave_id=1):
        super().__init__()
        self.slave_id = slave_id
        self._coils = [False] * 100
        self._discrete_inputs = [False] * 100
        self._holding_registers = [0] * 100
        self._input_registers = [0] * 100
        
        # 初始化默认值
        if slave_id == 4:  # 储气罐
            self._holding_registers[0] = 800   # 8.0 bar * 100
            self._holding_registers[1] = 350  # 35.0℃ * 10
            self._holding_registers[2] = 1   # 运行状态
        elif slave_id == 5:  # 干燥机
            self._holding_registers[0] = 800   # 进口压力 8.0 bar
            self._holding_registers[1] = 780   # 出口压力 7.8 bar
            self._holding_registers[2] = 1   # 运行状态
            self._holding_registers[3] = to_uint16(-400)  # 露点 -40.0℃ (补码)
            self._holding_registers[4] = 0   # 再生状态

    def validate(self, func_code, address, count=1):
        """校验请求范围，保持项目现有的 0 起始地址约定。"""
        if address < 0 or count < 1:
            return False

        store_type = self.decode(func_code)
        stores = {
            'c': self._coils,
            'd': self._discrete_inputs,
            'h': self._holding_registers,
            'i': self._input_registers,
        }
        store = stores.get(store_type)
        return store is not None and address + count <= len(store)

    def getValues(self, func_code, address, count=1):
        """
        获取寄存器值
        :param func_code: Modbus功能码 (1=线圈,2=离散输入,3=保持寄存器,4=输入寄存器)
        :param address: 起始地址 (0-based)
        :param count: 读取数量
        """
        store_type = self.decode(func_code)
        try:
            if store_type == 'c':  # 线圈 (FC 1,5,15)
                return self._coils[address:address + count]
            elif store_type == 'd':  # 离散输入 (FC 2)
                return self._discrete_inputs[address:address + count]
            elif store_type == 'h':  # 保持寄存器 (FC 3,6,16,22,23)
                result = self._holding_registers[address:address + count]
                print(f"[DEBUG] 从站{self.slave_id} 读取保持寄存器: 地址{address}, 数量{count}, 值{result}")
                return result
            elif store_type == 'i':  # 输入寄存器 (FC 4)
                return self._input_registers[address:address + count]
        except IndexError:
            print(f"[ERROR] 从站{self.slave_id} 读取越界: 地址{address}, 数量{count}")
            return [0] * count
        return [0] * count

    def setValues(self, func_code, address, values):
        """
        设置寄存器值
        :param func_code: Modbus功能码
        :param address: 起始地址 (0-based)
        :param values: 要写入的值列表
        """
        store_type = self.decode(func_code)
        try:
            if store_type == 'c':  # 线圈
                for i, v in enumerate(values):
                    self._coils[address + i] = bool(v)
            elif store_type == 'd':  # 离散输入
                for i, v in enumerate(values):
                    self._discrete_inputs[address + i] = bool(v)
            elif store_type == 'h':  # 保持寄存器
                for i, v in enumerate(values):
                    self._holding_registers[address + i] = int(v)
            elif store_type == 'i':  # 输入寄存器
                for i, v in enumerate(values):
                    self._input_registers[address + i] = int(v)
        except IndexError:
            pass

    def reset(self):
        """重置数据存储"""
        self._coils = [False] * 100
        self._discrete_inputs = [False] * 100
        self._holding_registers = [0] * 100
        self._input_registers = [0] * 100


class ModbusDataStore:
    """
    Modbus数据存储管理器
    管理所有从站的数据，提供统一的读写接口
    """

    def __init__(self):
        # 为5个从站各创建独立的 SlaveContext
        self._contexts = {}
        for slave_id in range(1, 6):
            self._contexts[slave_id] = SlaveContext(slave_id)

    def get_server_context_dict(self):
        """
        获取用于 ModbusServerContext 的设备字典
        格式: {slave_id: SlaveContext, ...}
        """
        return self._contexts

    # ========== 保持寄存器操作 ==========
    def set_holding_register(self, slave_id, address, value):
        """设置保持寄存器值"""
        if slave_id in self._contexts:
            ctx = self._contexts[slave_id]
            if 0 <= address < len(ctx._holding_registers):
                ctx._holding_registers[address] = to_uint16(int(value))

    def get_holding_registers(self, slave_id):
        """获取所有保持寄存器值"""
        if slave_id in self._contexts:
            return self._contexts[slave_id]._holding_registers
        return [0] * 100

    # ========== 输入寄存器操作 ==========
    def set_input_register(self, slave_id, address, value):
        """设置输入寄存器值"""
        if slave_id in self._contexts:
            ctx = self._contexts[slave_id]
            if 0 <= address < len(ctx._input_registers):
                ctx._input_registers[address] = int(value)

    def get_input_registers(self, slave_id):
        """获取所有输入寄存器值"""
        if slave_id in self._contexts:
            return self._contexts[slave_id]._input_registers
        return [0] * 100

    # ========== 线圈操作 ==========
    def set_coil(self, slave_id, address, value):
        """设置线圈值"""
        if slave_id in self._contexts:
            ctx = self._contexts[slave_id]
            if 0 <= address < len(ctx._coils):
                ctx._coils[address] = bool(value)

    def get_coils(self, slave_id):
        """获取所有线圈值"""
        if slave_id in self._contexts:
            return self._contexts[slave_id]._coils
        return [False] * 100

    # ========== 离散输入操作 ==========
    def get_discrete_inputs(self, slave_id):
        """获取所有离散输入值"""
        if slave_id in self._contexts:
            return self._contexts[slave_id]._discrete_inputs
        return [False] * 100
