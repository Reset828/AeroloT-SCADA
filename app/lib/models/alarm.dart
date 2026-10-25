import 'package:flutter/foundation.dart';

class Alarm {
  final int id;
  final int deviceId;
  final String message;
  final String severity;
  final DateTime timestamp;
  bool isAcknowledged;

  Alarm({
    required this.id,
    required this.deviceId,
    required this.message,
    required this.severity,
    required this.timestamp,
    this.isAcknowledged = false,
  });

  factory Alarm.fromJson(Map<String, dynamic> json) {
    int devId = 0;
    final deviceIdRaw = json['deviceId'];
    if (deviceIdRaw is int) {
      devId = deviceIdRaw;
    } else if (deviceIdRaw is String) {
      final match = RegExp(r'\d+').firstMatch(deviceIdRaw);
      if (match != null) {
        devId = int.parse(match.group(0)!);
      }
    }
    
    String severity = 'warning';
    if (json.containsKey('alarmType')) {
      severity = json['alarmType'] as String? ?? 'warning';
    } else if (json.containsKey('severity')) {
      severity = json['severity'] as String? ?? 'warning';
    }
    
    return Alarm(
      id: json['id'] as int? ?? DateTime.now().millisecondsSinceEpoch,
      deviceId: devId,
      message: json['message'] as String? ?? '',
      severity: severity,
      timestamp: DateTime.fromMillisecondsSinceEpoch(
        json['timestamp'] as int? ?? DateTime.now().millisecondsSinceEpoch,
      ),
    );
  }
}
