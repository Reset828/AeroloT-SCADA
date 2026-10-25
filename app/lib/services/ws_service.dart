import 'dart:async';
import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:web_socket_channel/web_socket_channel.dart';

enum ConnState { disconnected, connecting, connected, reconnecting }

class WSService {
  WebSocketChannel? _channel;
  ConnState _state = ConnState.disconnected;
  String _serverUrl = 'ws://192.168.1.100:18080';

  final _msgCtrl = StreamController<Map<String, dynamic>>.broadcast();
  final _stateCtrl = StreamController<ConnState>.broadcast();

  Timer? _reconnectTimer;
  Timer? _heartbeatTimer;
  int _retryCount = 0;
  static const int _maxRetries = 3;
  static const int _baseDelayMs = 1000;
  static const int _maxDelayMs = 60000;
  
  static const int _heartbeatIntervalMs = 5000;
  static const int _heartbeatTimeoutMs = 10000;
  DateTime? _lastHeartbeatResponse;
  bool _waitingForHeartbeat = false;

  Stream<Map<String, dynamic>> get msgStream => _msgCtrl.stream;
  Stream<ConnState> get stateStream => _stateCtrl.stream;
  ConnState get state => _state;
  bool get isConnected => _state == ConnState.connected;

  void setServerUrl(String url) => _serverUrl = url;

  Future<void> connect() async {
    if (_state == ConnState.connecting || _state == ConnState.connected) {
      return;
    }

    _updateState(ConnState.connecting);
    debugPrint('WebSocket正在连接: $_serverUrl');

    try {
      _channel = WebSocketChannel.connect(Uri.parse(_serverUrl));

      _channel!.stream.listen(
        _onMsg,
        onError: _onErr,
        onDone: _onDone,
        cancelOnError: false,
      );

      _updateState(ConnState.connected);
      _retryCount = 0;
      _startHeartbeat();
      debugPrint('WebSocket连接成功');
    } catch (e) {
      debugPrint('WS连接异常: $e');
      _scheduleReconnect();
    }
  }

  void disconnect() {
    _stopHeartbeat();
    _reconnectTimer?.cancel();
    _reconnectTimer = null;
    _channel?.sink.close();
    _channel = null;
    _updateState(ConnState.disconnected);
  }

  void sendCmd(Map<String, dynamic> cmd) {
    if (_state == ConnState.connected && _channel != null) {
      _channel!.sink.add(jsonEncode(cmd));
    }
  }

  void _startHeartbeat() {
    _stopHeartbeat();
    _lastHeartbeatResponse = DateTime.now();
    _waitingForHeartbeat = false;
    
    _heartbeatTimer = Timer.periodic(
      Duration(milliseconds: _heartbeatIntervalMs),
      (_) => _sendHeartbeat(),
    );
  }

  void _stopHeartbeat() {
    _heartbeatTimer?.cancel();
    _heartbeatTimer = null;
  }

  void _sendHeartbeat() {
    if (_state != ConnState.connected || _channel == null) {
      return;
    }

    final now = DateTime.now();
    
    if (_waitingForHeartbeat) {
      final elapsed = now.difference(_lastHeartbeatResponse ?? now).inMilliseconds;
      if (elapsed > _heartbeatTimeoutMs) {
        debugPrint('心跳超时，服务器可能已关闭');
        _onDone();
        return;
      }
    }

    _waitingForHeartbeat = true;
    sendCmd({'type': 'heartbeat', 'timestamp': now.millisecondsSinceEpoch});
  }

  void _onMsg(dynamic data) {
    try {
      final msg = jsonDecode(data as String) as Map<String, dynamic>;
      
      if (msg['type'] == 'heartbeat_response') {
        _lastHeartbeatResponse = DateTime.now();
        _waitingForHeartbeat = false;
        return;
      }
      
      _msgCtrl.add(msg);
    } catch (e) {
      debugPrint('消息解析失败: $e');
    }
  }

  void _onErr(dynamic err) {
    debugPrint('WS错误: $err');
    _stopHeartbeat();
    _channel = null;
    _updateState(ConnState.disconnected);
    _scheduleReconnect();
  }

  void _onDone() {
    debugPrint('WS连接关闭');
    _stopHeartbeat();
    _channel = null;
    _updateState(ConnState.disconnected);
    _scheduleReconnect();
  }

  void _scheduleReconnect() {
    _reconnectTimer?.cancel();
    
    if (_retryCount >= _maxRetries) {
      debugPrint('超过最大重试次数($_maxRetries)，停止重连');
      _updateState(ConnState.disconnected);
      return;
    }

    _updateState(ConnState.reconnecting);

    int delay = _baseDelayMs * (1 << _retryCount);
    if (delay > _maxDelayMs) {
      delay = _maxDelayMs;
    }
    debugPrint('${delay}ms后重连 (第${_retryCount + 1}/$_maxRetries次)');

    _reconnectTimer = Timer(Duration(milliseconds: delay), () {
      _retryCount++;
      connect();
    });
  }

  void _updateState(ConnState st) {
    if (_state != st) {
      _state = st;
      _stateCtrl.add(st);
      debugPrint('WebSocket状态变更: $st');
    }
  }

  void dispose() {
    disconnect();
    _msgCtrl.close();
    _stateCtrl.close();
  }
}
