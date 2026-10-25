import 'package:flutter/foundation.dart';
import '../models/device.dart';
import '../services/ws_service.dart' show WSService, ConnState;

class DeviceProvider extends ChangeNotifier {
  List<Device> _devices = [];
  bool _isOnline = false;
  bool _scadaConnected = false;
  String _scadaMessage = '';
  Function(Device)? _onDeviceUpdate;

  List<Device> get devices => _devices;
  bool get isOnline => _isOnline;
  bool get scadaConnected => _scadaConnected;
  String get scadaMessage => _scadaMessage;
  int get runningCount => _devices.where((d) => d.isRunning).length;
  int get alarmCount => _devices.where((d) => d.hasAlarm).length;

  void setOnDeviceUpdate(Function(Device) callback) {
    _onDeviceUpdate = callback;
  }

  void initDemo() {
    _devices = Device.demoDevices();
    notifyListeners();
  }

  void clearDevices() {
    _devices = [];
    notifyListeners();
  }

  int _parseDeviceId(String deviceId) {
    if (deviceId.startsWith('COM-')) {
      return int.tryParse(deviceId.split('-')[1]) ?? 0;
    } else if (deviceId.startsWith('TANK-')) {
      return 4;
    } else if (deviceId.startsWith('DRY-')) {
      return 5;
    }
    return int.tryParse(deviceId) ?? 0;
  }

  void updateFromMsg(Map<String, dynamic> data) {
    final type = data['type'] as String?;
    debugPrint('DeviceProvider: 收到消息 type=$type');

    if (type == 'connection_state') {
      _scadaConnected = data['connected'] as bool? ?? false;
      _scadaMessage = data['message'] as String? ?? '';
      debugPrint('DeviceProvider: 连接状态更新 connected=$_scadaConnected message=$_scadaMessage');
      
      if (!_scadaConnected) {
        _devices = [];
      } else if (_devices.isEmpty) {
        _devices = Device.demoDevices();
      }
      notifyListeners();
      return;
    }

    if (type == 'realtime_data') {
      if (!_scadaConnected) {
        _scadaConnected = true;
        _scadaMessage = '已连接到下位机';
      }
      
      if (_devices.isEmpty) {
        _devices = Device.demoDevices();
        debugPrint('DeviceProvider: 初始化设备列表');
      }
      
      final deviceData = data['device'] as Map<String, dynamic>?;
      if (deviceData != null) {
        final deviceIdStr = deviceData['id'] as String;
        final deviceNum = _parseDeviceId(deviceIdStr);
        final idx = _devices.indexWhere((d) => d.id == deviceNum);
        
        debugPrint('DeviceProvider: 设备ID=$deviceIdStr 解析后=$deviceNum 索引=$idx');
        debugPrint('DeviceProvider: 数据 pressure=${deviceData['pressure']} temperature=${deviceData['temperature']} status=${deviceData['status']}');
        
        if (idx >= 0) {
          _devices[idx].pressure =
              (deviceData['pressure'] as num?)?.toDouble() ?? 0;
          _devices[idx].temperature =
              (deviceData['temperature'] as num?)?.toDouble() ?? 0;
          _devices[idx].status = deviceData['status'] as int? ?? 0;
          
          final deviceType = deviceData['type'] as String?;
          if (deviceType == 'compressor') {
            _devices[idx].current =
                (deviceData['current'] as num?)?.toDouble() ?? 0;
            _devices[idx].alarmCode = deviceData['alarmCode'] as int? ?? 0;
          } else if (deviceType == 'dryer') {
            _devices[idx].outletPressure =
                (deviceData['outletPressure'] as num?)?.toDouble() ?? 0;
            _devices[idx].dewPoint =
                (deviceData['dewPoint'] as num?)?.toDouble() ?? 0;
            _devices[idx].regenStatus = deviceData['regenStatus'] as int? ?? 0;
          }
          
          _devices[idx].lastUpdate = DateTime.now();
          
          debugPrint('DeviceProvider: 更新设备 ${_devices[idx].name} pressure=${_devices[idx].pressure} temperature=${_devices[idx].temperature}');
          
          if (_onDeviceUpdate != null) {
            _onDeviceUpdate!(_devices[idx]);
          }
          
          notifyListeners();
        } else {
          debugPrint('DeviceProvider: 未找到设备索引 deviceNum=$deviceNum');
        }
      } else {
        debugPrint('DeviceProvider: deviceData为空');
      }
    } else if (type == 'data') {
      final devs = data['devices'] as List<dynamic>?;
      if (devs != null) {
        for (final d in devs) {
          final devData = d as Map<String, dynamic>;
          final id = devData['id'] as int;
          final idx = _devices.indexWhere((d) => d.id == id);

          if (idx >= 0) {
            _devices[idx].pressure =
                (devData['pressure'] as num?)?.toDouble() ?? 0;
            _devices[idx].temperature =
                (devData['temperature'] as num?)?.toDouble() ?? 0;
            _devices[idx].frequency =
                (devData['frequency'] as num?)?.toDouble() ?? 0;
            _devices[idx].current =
                (devData['current'] as num?)?.toDouble() ?? 0;
            _devices[idx].status = devData['status'] as int? ?? 0;
            _devices[idx].alarmCode = devData['alarmCode'] as int? ?? 0;
            _devices[idx].lastUpdate = DateTime.now();
            
            if (_onDeviceUpdate != null) {
              _onDeviceUpdate!(_devices[idx]);
            }
          }
        }
        notifyListeners();
      }
    } else if (type == 'device_status') {
      final id = data['deviceId'] as int;
      final status = data['status'] as int;
      final idx = _devices.indexWhere((d) => d.id == id);
      if (idx >= 0) {
        _devices[idx].status = status;
        _devices[idx].lastUpdate = DateTime.now();
        notifyListeners();
      }
    }
  }

  void setOnline(bool online) {
    _isOnline = online;
    notifyListeners();
  }
  
  void setDisconnected() {
    _isOnline = false;
    _scadaConnected = false;
    _scadaMessage = '上位机已断开连接';
    _devices = [];
    notifyListeners();
  }
}
