import 'package:flutter/foundation.dart';

class Device {
  final int id;
  final String name;
  final String type;
  double pressure;
  double temperature;
  double frequency;
  double current;
  int status;
  int alarmCode;
  int runtimeHours;
  DateTime lastUpdate;
  
  double? outletPressure;
  double? dewPoint;
  int? regenStatus;

  Device({
    required this.id,
    required this.name,
    required this.type,
    this.pressure = 0,
    this.temperature = 0,
    this.frequency = 0,
    this.current = 0,
    this.status = 0,
    this.alarmCode = 0,
    this.runtimeHours = 0,
    this.outletPressure,
    this.dewPoint,
    this.regenStatus,
    DateTime? lastUpdate,
  }) : lastUpdate = lastUpdate ?? DateTime.now();

  String get statusText {
    switch (status) {
      case 1:
        return '运行';
      case 2:
        return '故障';
      default:
        return '停止';
    }
  }

  bool get isRunning => status == 1;
  bool get hasAlarm => alarmCode != 0 || status == 2;

  factory Device.fromJson(Map<String, dynamic> json) {
    return Device(
      id: json['id'] as int,
      name: json['name'] as String? ?? '设备${json['id']}',
      type: json['type'] as String? ?? 'compressor',
      pressure: (json['pressure'] as num?)?.toDouble() ?? 0,
      temperature: (json['temperature'] as num?)?.toDouble() ?? 0,
      frequency: (json['frequency'] as num?)?.toDouble() ?? 0,
      current: (json['current'] as num?)?.toDouble() ?? 0,
      status: json['status'] as int? ?? 0,
      alarmCode: json['alarmCode'] as int? ?? 0,
      runtimeHours: (json['runtimeHours'] as num?)?.toInt() ?? 0,
      outletPressure: (json['outletPressure'] as num?)?.toDouble(),
      dewPoint: (json['dewPoint'] as num?)?.toDouble(),
      regenStatus: json['regenStatus'] as int?,
    );
  }

  static List<Device> demoDevices() {
    return [
      Device(
        id: 1,
        name: '空压机1',
        type: 'compressor',
        pressure: 8.2,
        temperature: 45,
        current: 15.5,
        status: 1,
      ),
      Device(
        id: 2,
        name: '空压机2',
        type: 'compressor',
        pressure: 7.8,
        temperature: 42,
        current: 14.8,
        status: 1,
      ),
      Device(
        id: 3,
        name: '空压机3',
        type: 'compressor',
        pressure: 7.5,
        temperature: 40,
        current: 14.2,
        status: 1,
      ),
      Device(
        id: 4,
        name: '储气罐',
        type: 'tank',
        pressure: 7.5,
        temperature: 28,
        status: 1,
      ),
      Device(
        id: 5,
        name: '干燥机',
        type: 'dryer',
        pressure: 7.2,
        outletPressure: 7.0,
        temperature: 35,
        dewPoint: -20,
        regenStatus: 1,
        status: 1,
      ),
    ];
  }
}
