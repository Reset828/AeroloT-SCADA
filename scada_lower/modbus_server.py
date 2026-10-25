"""
Modbus TCP服务器管理类
兼容 pymodbus 3.x
"""

from pymodbus.server import StartTcpServer
from pymodbus.datastore import ModbusServerContext
from pymodbus.device import ModbusDeviceIdentification
import logging
import threading


class ModbusServerManager:
    """Modbus TCP服务器管理器"""

    def __init__(self, data_store):
        self.data_store = data_store
        self.server_thread = None
        self.running = False
        
        logging.basicConfig(level=logging.WARNING)
        self.logger = logging.getLogger('pymodbus')

    def start_server(self, host='0.0.0.0', port=1502):
        """启动Modbus TCP服务器"""
        if self.running:
            self.logger.warning("服务器已在运行中")
            return

        try:
            # 获取各从站的独立context字典
            devices = self.data_store.get_server_context_dict()
            
            # single=False，每个从站使用独立的 SlaveContext
            server_context = ModbusServerContext(
                slaves=devices,
                single=False
            )

            identity = ModbusDeviceIdentification()
            identity.VendorName = 'AeroloT'
            identity.ProductCode = 'SCADA'
            identity.ProductName = 'Modbus Simulator'
            identity.ModelName = 'AeroloT-SCADA'
            identity.MajorMinorRevision = '1.0'

            self.running = True
            self.logger.info(f"正在启动Modbus TCP服务器: {host}:{port}")
            
            def run_server():
                try:
                    StartTcpServer(
                        context=server_context,
                        identity=identity,
                        address=(host, port),
                    )
                except Exception as e:
                    self.logger.error(f"服务器运行错误: {e}")
                    self.running = False
            
            self.server_thread = threading.Thread(target=run_server, daemon=True)
            self.server_thread.start()
            
            self.logger.info(f"Modbus TCP服务器已启动，监听端口: {port}")
            
        except Exception as e:
            self.running = False
            self.logger.error(f"启动服务器失败: {str(e)}")
            raise

    def stop_server(self):
        """停止Modbus TCP服务器"""
        if not self.running:
            self.logger.warning("服务器未在运行")
            return
        try:
            self.running = False
            self.logger.info("Modbus TCP服务器已停止")
        except Exception as e:
            self.logger.error(f"停止服务器失败: {str(e)}")

    def is_running(self):
        """检查服务器是否正在运行"""
        return self.running
