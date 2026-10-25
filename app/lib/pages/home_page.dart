import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/device_provider.dart';
import '../providers/alarm_provider.dart';

class HomePage extends StatelessWidget {
  const HomePage({super.key});

  @override
  Widget build(BuildContext context) {
    final deviceProvider = context.watch<DeviceProvider>();
    final alarmProvider = context.watch<AlarmProvider>();

    return Scaffold(
      appBar: AppBar(
        title: const Text('AeroloT SCADA 监控'),
        actions: [
          IconButton(
            icon: Badge(
              label: Text('${alarmProvider.unacknowledgedCount}'),
              isLabelVisible: alarmProvider.unacknowledgedCount > 0,
              backgroundColor: Colors.red,
              child: const Icon(Icons.notifications),
            ),
            onPressed: () => _showAlarms(context),
          ),
        ],
      ),
      body: _buildBody(context, deviceProvider),
    );
  }

  Widget _buildBody(BuildContext context, DeviceProvider deviceProvider) {
    if (!deviceProvider.isOnline) {
      return Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            const Icon(
              Icons.cloud_off,
              size: 64,
              color: Colors.red,
            ),
            const SizedBox(height: 16),
            const Text(
              '上位机已断开连接',
              style: TextStyle(
                fontSize: 18,
                color: Colors.red,
                fontWeight: FontWeight.bold,
              ),
            ),
            const SizedBox(height: 8),
            const Text(
              '请检查上位机是否运行',
              style: TextStyle(
                fontSize: 14,
                color: Colors.grey,
              ),
            ),
          ],
        ),
      );
    }

    if (!deviceProvider.scadaConnected) {
      return Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            const Icon(
              Icons.sync_problem,
              size: 64,
              color: Colors.orange,
            ),
            const SizedBox(height: 16),
            Text(
              deviceProvider.scadaMessage.isNotEmpty 
                  ? deviceProvider.scadaMessage 
                  : '上位机未连接到下位机',
              style: const TextStyle(
                fontSize: 18,
                color: Colors.orange,
              ),
            ),
            const SizedBox(height: 8),
            const Text(
              '请等待上位机连接到下位机',
              style: TextStyle(
                fontSize: 14,
                color: Colors.grey,
              ),
            ),
          ],
        ),
      );
    }

    if (deviceProvider.devices.isEmpty) {
      return const Center(
        child: Text(
          '暂无设备数据',
          style: TextStyle(fontSize: 16, color: Colors.grey),
        ),
      );
    }

    return ListView.builder(
      padding: const EdgeInsets.all(8),
      itemCount: deviceProvider.devices.length,
      itemBuilder: (context, index) {
        final device = deviceProvider.devices[index];
        return _DeviceCard(device: device);
      },
    );
  }

  void _showAlarms(BuildContext context) {
    final alarmProvider = context.read<AlarmProvider>();
    showModalBottomSheet(
      context: context,
      builder: (_) => Container(
        constraints: BoxConstraints(
          maxHeight: MediaQuery.of(context).size.height * 0.6,
        ),
        child: alarmProvider.alarms.isEmpty
            ? const Center(
                child: Text('暂无报警信息', style: TextStyle(color: Colors.grey)),
              )
            : ListView.builder(
                itemCount: alarmProvider.alarms.length,
                itemBuilder: (_, i) {
                  final alarm = alarmProvider.alarms[i];
                  final isCritical = alarm.severity == 'critical';
                  return Container(
                    color: isCritical ? Colors.red.shade50 : Colors.orange.shade50,
                    child: ListTile(
                      leading: Icon(
                        Icons.warning,
                        color: isCritical ? Colors.red : Colors.orange,
                      ),
                      title: Text(
                        alarm.message,
                        style: TextStyle(
                          color: isCritical ? Colors.red : Colors.orange.shade800,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                      subtitle: Text(
                        '设备${alarm.deviceId} - ${alarm.timestamp.toString().substring(0, 19)}',
                        style: TextStyle(
                          color: isCritical ? Colors.red.shade700 : Colors.orange.shade700,
                        ),
                      ),
                      trailing: alarm.isAcknowledged
                          ? const Icon(Icons.check, color: Colors.green)
                          : TextButton(
                              onPressed: () => alarmProvider.acknowledge(alarm.id),
                              child: const Text('确认'),
                            ),
                    ),
                  );
                },
              ),
      ),
    );
  }
}

class _DeviceCard extends StatelessWidget {
  final dynamic device;

  const _DeviceCard({required this.device});

  @override
  Widget build(BuildContext context) {
    final alarmProvider = context.watch<AlarmProvider>();
    final hasAlarm = alarmProvider.alarms.any(
      (a) => a.deviceId == device.id && !a.isAcknowledged
    );
    
    return Card(
      color: hasAlarm ? Colors.red.shade50 : null,
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Expanded(
                  child: Text(
                    device.name,
                    style: TextStyle(
                      fontSize: 18,
                      fontWeight: FontWeight.bold,
                      color: hasAlarm ? Colors.red : null,
                    ),
                  ),
                ),
                Container(
                  padding: const EdgeInsets.symmetric(
                    horizontal: 8,
                    vertical: 4,
                  ),
                  decoration: BoxDecoration(
                    color: device.isRunning 
                        ? Colors.green 
                        : (device.status == 2 ? Colors.red : Colors.grey),
                    borderRadius: BorderRadius.circular(12),
                  ),
                  child: Text(
                    device.statusText,
                    style: const TextStyle(color: Colors.white, fontSize: 12),
                  ),
                ),
              ],
            ),
            if (hasAlarm) ...[
              const SizedBox(height: 8),
              Container(
                padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
                decoration: BoxDecoration(
                  color: Colors.red,
                  borderRadius: BorderRadius.circular(4),
                ),
                child: const Text(
                  '报警',
                  style: TextStyle(color: Colors.white, fontSize: 12),
                ),
              ),
            ],
            const SizedBox(height: 12),
            _buildMetrics(),
          ],
        ),
      ),
    );
  }

  Widget _buildMetrics() {
    final type = device.type as String?;
    
    if (type == 'compressor') {
      return Row(
        children: [
          _Metric(label: '压力', value: '${device.pressure.toStringAsFixed(2)}', unit: 'bar'),
          _Metric(label: '温度', value: '${device.temperature.toStringAsFixed(1)}', unit: '℃'),
          _Metric(label: '电流', value: '${device.current.toStringAsFixed(1)}', unit: 'A'),
        ],
      );
    } else if (type == 'tank') {
      return Row(
        children: [
          _Metric(label: '罐压', value: '${device.pressure.toStringAsFixed(2)}', unit: 'bar'),
          _Metric(label: '温度', value: '${device.temperature.toStringAsFixed(1)}', unit: '℃'),
        ],
      );
    } else if (type == 'dryer') {
      return Row(
        children: [
          _Metric(label: '进口压力', value: '${device.pressure.toStringAsFixed(2)}', unit: 'bar'),
          _Metric(label: '出口压力', value: '${device.outletPressure?.toStringAsFixed(2) ?? '--'}', unit: 'bar'),
          _Metric(label: '露点', value: '${device.dewPoint?.toStringAsFixed(1) ?? '--'}', unit: '℃'),
        ],
      );
    }
    
    return Row(
      children: [
        _Metric(label: '压力', value: '${device.pressure}', unit: 'bar'),
        _Metric(label: '温度', value: '${device.temperature}', unit: '℃'),
      ],
    );
  }
}

class _Metric extends StatelessWidget {
  final String label;
  final String value;
  final String unit;

  const _Metric({required this.label, required this.value, required this.unit});

  @override
  Widget build(BuildContext context) {
    return Expanded(
      child: Column(
        children: [
          Text(label, style: const TextStyle(color: Colors.grey, fontSize: 12)),
          Text(
            '$value $unit',
            style: const TextStyle(fontSize: 16, fontWeight: FontWeight.w500),
          ),
        ],
      ),
    );
  }
}
