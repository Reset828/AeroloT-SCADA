import 'dart:async';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/ws_service.dart' show WSService, ConnState;
import '../providers/device_provider.dart';
import 'home_page.dart';

class ConnectPage extends StatefulWidget {
  const ConnectPage({super.key});

  @override
  State<ConnectPage> createState() => _ConnectPageState();
}

class _ConnectPageState extends State<ConnectPage> {
  final _ipCtrl = TextEditingController(text: '10.30.94.40');
  StreamSubscription? _stateSub;
  bool _isConnecting = false;
  String? _errorMsg;

  @override
  void dispose() {
    _stateSub?.cancel();
    _ipCtrl.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('SCADA 连接')),
      body: Padding(
        padding: const EdgeInsets.all(24.0),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            const Icon(Icons.wifi, size: 64, color: Colors.blue),
            const SizedBox(height: 32),
            TextField(
              controller: _ipCtrl,
              decoration: const InputDecoration(
                labelText: '服务器IP',
                border: OutlineInputBorder(),
                prefixIcon: Icon(Icons.computer),
              ),
              enabled: !_isConnecting,
            ),
            const SizedBox(height: 16),
            const Text('端口: 18080', style: TextStyle(color: Colors.grey)),
            if (_errorMsg != null) ...[
              const SizedBox(height: 16),
              Text(_errorMsg!, style: const TextStyle(color: Colors.red)),
            ],
            const SizedBox(height: 32),
            SizedBox(
              width: double.infinity,
              height: 48,
              child: ElevatedButton.icon(
                onPressed: _isConnecting ? null : _connect,
                icon: _isConnecting 
                    ? const SizedBox(
                        width: 20,
                        height: 20,
                        child: CircularProgressIndicator(strokeWidth: 2),
                      )
                    : const Icon(Icons.link),
                label: Text(_isConnecting ? '连接中...' : '连接'),
              ),
            ),
          ],
        ),
      ),
    );
  }

  void _connect() {
    final ip = _ipCtrl.text.trim();
    final url = 'ws://$ip:18080';

    setState(() {
      _isConnecting = true;
      _errorMsg = null;
    });

    final ws = context.read<WSService>();
    final deviceProvider = context.read<DeviceProvider>();

    bool navigated = false;
    _stateSub = ws.stateStream.listen((state) {
      deviceProvider.setOnline(state == ConnState.connected);
      
      if (state == ConnState.connected && !navigated) {
        navigated = true;
        Navigator.pushReplacement(
          context,
          MaterialPageRoute(builder: (_) => const HomePage()),
        );
      } else if (state == ConnState.disconnected && !navigated) {
        setState(() {
          _isConnecting = false;
          _errorMsg = '连接失败，请检查服务器地址';
        });
      }
    });

    ws.setServerUrl(url);
    ws.connect();
  }
}
