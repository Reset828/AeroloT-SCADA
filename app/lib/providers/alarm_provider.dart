import 'package:flutter/foundation.dart';
import '../models/alarm.dart';

class AlarmProvider extends ChangeNotifier {
  final List<Alarm> _alarms = [];
  int _nextId = 1;

  List<Alarm> get alarms => List.unmodifiable(_alarms);
  List<Alarm> get unacknowledged =>
      _alarms.where((a) => !a.isAcknowledged).toList();
  int get unacknowledgedCount => unacknowledged.length;

  /// 添加从上位机 Webhook 推送过来的报警
  void addAlarmFromMsg(Map<String, dynamic> data) {
    final alarm = Alarm(
      id: _nextId++,
      deviceId: _parseDeviceId(data['deviceId']),
      message: data['message'] as String? ?? '',
      severity: data['alarmType'] as String? ?? 'warning',
      timestamp: DateTime.fromMillisecondsSinceEpoch(
        data['timestamp'] as int? ?? DateTime.now().millisecondsSinceEpoch,
      ),
    );
    addAlarm(alarm);
  }

  /// 添加报警记录
  void addAlarm(Alarm alarm) {
    _alarms.insert(0, alarm);
    notifyListeners();
  }

  /// 确认报警
  void acknowledge(int alarmId) {
    final idx = _alarms.indexWhere((a) => a.id == alarmId);
    if (idx >= 0) {
      _alarms[idx].isAcknowledged = true;
      notifyListeners();
    }
  }

  /// 清空报警列表
  void clear() {
    _alarms.clear();
    notifyListeners();
  }

  /// 解析设备ID
  int _parseDeviceId(dynamic deviceId) {
    if (deviceId is int) return deviceId;
    if (deviceId is String) {
      final match = RegExp(r'\d+').firstMatch(deviceId);
      if (match != null) return int.parse(match.group(0)!);
    }
    return 0;
  }
}
