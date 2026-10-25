import 'dart:async';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import 'services/ws_service.dart';
import 'providers/device_provider.dart';
import 'providers/alarm_provider.dart';
import 'pages/connect_page.dart';

void main() {
  runApp(const ScadaApp());
}

class ScadaApp extends StatelessWidget {
  const ScadaApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (_) => DeviceProvider()),
        ChangeNotifierProvider(create: (_) => AlarmProvider()),
        Provider<WSService>(create: (_) => WSService(), lazy: false),
      ],
      child: MaterialApp(
        title: 'AeroloT SCADA',
        theme: ThemeData(
          colorScheme: ColorScheme.fromSeed(seedColor: Colors.blue),
          useMaterial3: true,
        ),
        home: const MainNavigator(),
      ),
    );
  }
}

class MainNavigator extends StatefulWidget {
  const MainNavigator({super.key});

  @override
  State<MainNavigator> createState() => _MainNavigatorState();
}

class _MainNavigatorState extends State<MainNavigator> {
  final GlobalKey<NavigatorState> _navigatorKey = GlobalKey<NavigatorState>();
  StreamSubscription? _msgSub;
  StreamSubscription? _stateSub;
  bool _initialized = false;

  @override
  void dispose() {
    _msgSub?.cancel();
    _stateSub?.cancel();
    super.dispose();
  }

  void _setupSubscriptions() {
    if (_initialized) return;
    _initialized = true;

    final ws = context.read<WSService>();
    final deviceProvider = context.read<DeviceProvider>();
    final alarmProvider = context.read<AlarmProvider>();

    deviceProvider.setOnDeviceUpdate((device) {
    });

    _msgSub = ws.msgStream.listen((msg) {
      final type = msg['type'] as String?;
      debugPrint('收到消息: type=$type');
      
      if (type == 'realtime_data') {
        deviceProvider.updateFromMsg(msg);
      } else if (type == 'connection_state') {
        deviceProvider.updateFromMsg(msg);
      } else if (type == 'alarm') {
        alarmProvider.addAlarmFromMsg(msg);
      }
    });

    _stateSub = ws.stateStream.listen((state) {
      debugPrint('连接状态变化: $state');
      deviceProvider.setOnline(state == ConnState.connected);
      
      if (state == ConnState.disconnected) {
        deviceProvider.setDisconnected();
        _navigatorKey.currentState?.pushAndRemoveUntil(
          MaterialPageRoute(builder: (_) => const ConnectPage()),
          (route) => false,
        );
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    WidgetsBinding.instance.addPostFrameCallback((_) {
      _setupSubscriptions();
    });

    return Navigator(
      key: _navigatorKey,
      onGenerateRoute: (settings) {
        return MaterialPageRoute(
          builder: (context) => const ConnectPage(),
          settings: settings,
        );
      },
    );
  }
}
