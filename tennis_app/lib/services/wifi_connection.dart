import 'dart:async';

import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:web_socket_channel/web_socket_channel.dart';
import 'package:connectivity_plus/connectivity_plus.dart';
import 'package:shared_preferences/shared_preferences.dart';

// Connection state
class WifiConnectionState {
  final bool isConnecting;
  final bool isConnected;
  final String? error;
  final String? robotError; // ERROR, messages sent by ESP32 (non-fatal)
  final String? robotWarning; // WARN, messages sent by ESP32
  final List<String> rxLog;
  final int? batteryPercent;
  final double robotX;
  final double robotY;
  final bool anchorLOnline;
  final bool anchorROnline;
  final int ballsCount;
  final String robotState;
  final int maxBalls;
  final double rangeL;
  final double rangeR;

  const WifiConnectionState({
    this.isConnecting = false,
    this.isConnected = false,
    this.error,
    this.robotError,
    this.robotWarning,
    this.rxLog = const [],
    this.batteryPercent,
    this.robotX = 0,
    this.robotY = 0,
    this.anchorLOnline = false,
    this.anchorROnline = false,
    this.ballsCount = 0,
    this.robotState = 'MANUAL',
    this.maxBalls = 20,
    this.rangeL = 0,
    this.rangeR = 0,
  });

  WifiConnectionState copyWith({
    bool? isConnecting,
    bool? isConnected,
    String? error,
    String? robotError,
    String? robotWarning,
    List<String>? rxLog,
    int? batteryPercent,
    double? robotX,
    double? robotY,
    bool? anchorLOnline,
    bool? anchorROnline,
    int? ballsCount,
    String? robotState,
    int? maxBalls,
    double? rangeL,
    double? rangeR,
  }) {
    return WifiConnectionState(
      isConnecting: isConnecting ?? this.isConnecting,
      isConnected: isConnected ?? this.isConnected,
      error: error,
      robotError: robotError ?? this.robotError,
      robotWarning: robotWarning ?? this.robotWarning,
      rxLog: rxLog ?? this.rxLog,
      batteryPercent: batteryPercent ?? this.batteryPercent,
      robotX: robotX ?? this.robotX,
      robotY: robotY ?? this.robotY,
      anchorLOnline: anchorLOnline ?? this.anchorLOnline,
      anchorROnline: anchorROnline ?? this.anchorROnline,
      ballsCount: ballsCount ?? this.ballsCount,
      robotState: robotState ?? this.robotState,
      maxBalls: maxBalls ?? this.maxBalls,
      rangeL: rangeL ?? this.rangeL,
      rangeR: rangeR ?? this.rangeR,
    );
  }
}

// WiFi ping check setting provider
class WifiPingCheckNotifier extends StateNotifier<bool> {
  static const String _key = 'wifi_ping_check_enabled';

  WifiPingCheckNotifier() : super(true) {
    _load();
  }

  Future<void> _load() async {
    final prefs = await SharedPreferences.getInstance();
    state = prefs.getBool(_key) ?? true;
  }

  Future<void> setEnabled(bool enabled) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setBool(_key, enabled);
    state = enabled;
  }
}

final wifiPingCheckProvider = StateNotifierProvider<WifiPingCheckNotifier, bool>(
  (ref) => WifiPingCheckNotifier(),
);

// Terminal state
class TerminalState {
  final List<String> lines;

  const TerminalState({this.lines = const []});

  TerminalState copyWith({List<String>? lines}) {
    return TerminalState(lines: lines ?? this.lines);
  }
}

class TerminalController extends StateNotifier<TerminalState> {
  TerminalController() : super(const TerminalState());

  void addLine(String line) {
    final newLines = [...state.lines, line];
    if (newLines.length > 200) {
      newLines.removeAt(0);
    }
    state = TerminalState(lines: newLines);
  }

  void clear() {
    state = const TerminalState(lines: []);
  }
}

final terminalProvider = StateNotifierProvider<TerminalController, TerminalState>(
  (ref) => TerminalController(),
);

// Main WiFi connection notifier
class WifiConnectionNotifier extends StateNotifier<WifiConnectionState> {
  static const String _host = "192.168.4.1";
  static const int _port = 81;
  static const Duration _connectionTimeout = Duration(seconds: 10);
  static const Duration _testTimeout = Duration(seconds: 3);

  WebSocketChannel? _channel;
  StreamSubscription? _subscription;
  StreamSubscription? _connectivitySubscription;
  Timer? _pingTimer;

  final Ref _ref;
  final void Function(String) _log;

  WifiConnectionNotifier(this._ref, this._log) : super(const WifiConnectionState());

  void _addLog(String message) {
    _log(message);
    final newLog = [...state.rxLog, message];
    if (newLog.length > 200) newLog.removeAt(0);
    state = state.copyWith(rxLog: newLog);
  }

  Future<bool> _testConnection() async {
    _addLog('CONNECT: test ws://$_host:$_port/ws');
    try {
      final socket = WebSocketChannel.connect(
        Uri.parse('ws://$_host:$_port/ws'),
      );
      
      final completer = Completer<bool>();
      Timer? timer;
      StreamSubscription? sub;

      timer = Timer(_testTimeout, () {
        sub?.cancel();
        socket.sink.close();
        _addLog('CONNECT: test timeout');
        if (!completer.isCompleted) completer.complete(false);
      });

      sub = socket.stream.listen(
        (message) {
          timer?.cancel();
          sub?.cancel();
          socket.sink.close();
          _addLog('CONNECT: test ok (rx: $message)');
          if (!completer.isCompleted) completer.complete(true);
        },
        onError: (error) {
          timer?.cancel();
          sub?.cancel();
          socket.sink.close();
          _addLog('CONNECT: test error: $error');
          if (!completer.isCompleted) completer.complete(false);
        },
      );

      return completer.future;
    } catch (e) {
      return false;
    }
  }

  Future<void> connect({bool skipCheck = false}) async {
    if (state.isConnecting || state.isConnected) return;

    _addLog('CONNECT: start (skipCheck=$skipCheck)');

    state = state.copyWith(isConnecting: true, error: null);

    final pingCheckEnabled = _ref.read(wifiPingCheckProvider);

    // Blind mode: if Wi-Fi check is disabled in settings, connect without any WebSocket actions.
    // This allows sending commands "вслепую" while still logging TX/RX into terminal.
    if (!pingCheckEnabled) {
      _addLog('CONNECT: blind mode (ping check disabled)');
      state = state.copyWith(isConnecting: false, isConnected: true, error: null);
      return;
    }

    // If skip check or ping check disabled, skip ONLY the pre-test, but still do real connection
    final doPreTest = !(skipCheck || !pingCheckEnabled);

    // Test connection first
    if (doPreTest) {
      final testOk = await _testConnection();
      if (!testOk) {
        _addLog('CONNECT: failed on pre-test');
        state = state.copyWith(
          isConnecting: false,
          error: 'Не удалось подключиться к ESP32',
        );
        return;
      }
    } else {
      _addLog('CONNECT: pre-test skipped');
    }

    // Main connection
    try {
      _addLog('CONNECT: opening ws://$_host:$_port/ws');
      _channel = WebSocketChannel.connect(
        Uri.parse('ws://$_host:$_port/ws'),
      );

      final completer = Completer<bool>();
      Timer? timer;

      timer = Timer(_connectionTimeout, () {
        if (!completer.isCompleted) {
          _addLog('CONNECT: timeout waiting for STATE,CONNECTED');
          completer.complete(false);
          _channel?.sink.close();
          _channel = null;
        }
      });

      _subscription = _channel!.stream.listen(
        (message) {
          _handleMessage(message);
          if (!completer.isCompleted && message == 'STATE,CONNECTED') {
            _addLog('CONNECT: connected');
            completer.complete(true);
          }
        },
        onError: (error) {
          timer?.cancel();
          if (!completer.isCompleted) completer.complete(false);
          _handleError('WebSocket error: $error');
        },
        onDone: () {
          timer?.cancel();
          if (!completer.isCompleted) completer.complete(false);
          _handleDisconnect();
        },
      );

      final success = await completer.future;
      timer.cancel();

      if (success) {
        state = state.copyWith(isConnecting: false, isConnected: true);
        _startConnectivityMonitor();
        _startPingTimer();
      } else {
        _addLog('CONNECT: failed');
        state = state.copyWith(
          isConnecting: false,
          error: 'Таймаут подключения',
        );
        _channel?.sink.close();
        _channel = null;
      }
    } catch (e) {
      _addLog('CONNECT: exception: $e');
      state = state.copyWith(
        isConnecting: false,
        error: 'Ошибка подключения: $e',
      );
    }
  }

  void _handleMessage(String message) {
    _addLog('RX: $message');

    if (message.startsWith('BAT_PCT,')) {
      final pct = int.tryParse(message.split(',')[1]);
      if (pct != null) {
        state = state.copyWith(batteryPercent: pct);
      }
    }
    if (message.startsWith('POS,')) {
      final parts = message.split(',');
      if (parts.length == 3) {
        final x = double.tryParse(parts[1]) ?? 0;
        final y = double.tryParse(parts[2]) ?? 0;
        state = state.copyWith(robotX: x, robotY: y);
      }
    }
    if (message.startsWith('ANCHOR,')) {
      final parts = message.split(',');
      if (parts.length == 3) {
        final isOnline = parts[2] == 'online';
        if (parts[1] == 'L') state = state.copyWith(anchorLOnline: isOnline);
        if (parts[1] == 'R') state = state.copyWith(anchorROnline: isOnline);
      }
    }
    if (message.startsWith('BALLS_COUNT,')) {
      final n = int.tryParse(message.split(',')[1]) ?? 0;
      state = state.copyWith(ballsCount: n);
    }
    if (message.startsWith('STATE,')) {
      final s = message.substring(6);
      state = state.copyWith(robotState: s);
    }
    if (message.startsWith('RANGE,')) {
      final parts = message.split(',');
      if (parts.length == 3) {
        final dist = double.tryParse(parts[2]) ?? 0;
        if (parts[1] == 'L') state = state.copyWith(rangeL: dist);
        if (parts[1] == 'R') state = state.copyWith(rangeR: dist);
      }
    }
    if (message.startsWith('CALIBRATED,')) {
      final parts = message.split(',');
      if (parts.length == 3) {
        final x = double.tryParse(parts[1]) ?? 0;
        final y = double.tryParse(parts[2]) ?? 0;
        state = state.copyWith(robotX: x, robotY: y);
      }
    }
    if (message.startsWith('ERROR,')) {
      // Non-fatal error from ESP32 (e.g. AUTO attempted before calibration)
      final msg = message.substring(6);
      state = state.copyWith(robotError: msg);
    }
    if (message.startsWith('WARN,')) {
      final msg = message.substring(5);
      state = state.copyWith(robotWarning: msg);
    }
  }

  void _handleError(String error) {
    _addLog('ERROR: $error');
    state = state.copyWith(error: error, isConnected: false);
    _channel?.sink.close();
    _channel = null;
    _pingTimer?.cancel();
  }

  void _handleDisconnect() {
    _addLog('DISCONNECT: done');
    state = state.copyWith(isConnected: false);
    _channel = null;
    _pingTimer?.cancel();
  }

  void _startConnectivityMonitor() {
    _connectivitySubscription = Connectivity()
        .onConnectivityChanged
        .listen((results) {
      final hasWifi = results.contains(ConnectivityResult.wifi);
      if (!hasWifi && state.isConnected) {
        disconnect(error: 'Wi-Fi отключен');
      }
    });
  }

  void _startPingTimer() {
    _pingTimer?.cancel();
    _pingTimer = Timer.periodic(const Duration(seconds: 30), (_) {
      if (state.isConnected) {
        sendRaw('PING');
      }
    });
  }

  void disconnect({String? error}) {
    _addLog('DISCONNECT: ${error ?? 'manual'}');
    _pingTimer?.cancel();
    _connectivitySubscription?.cancel();
    _subscription?.cancel();
    _channel?.sink.close();
    _channel = null;
    _subscription = null;

    state = WifiConnectionState(
      error: error,
      rxLog: state.rxLog,
    );
  }

  void _send(String message) {
    if (!state.isConnected) return;
    _channel?.sink.add(message);
    _addLog('TX: $message');
  }

  // Movement commands
  void sendMove(int left, int right) {
    left = left.clamp(-100, 100);
    right = right.clamp(-100, 100);
    _send('M,$left,$right');
  }

  void sendStop() {
    _send('STOP');
  }

  // Attachment commands
  void sendAttachment(bool enabled) {
    _send(enabled ? 'ATTACHMENT_ON' : 'ATTACHMENT_OFF');
  }

  // Mount commands
  void sendMount(bool enabled) {
    _send(enabled ? 'MOUNT_ON' : 'MOUNT_OFF');
  }

  // Raw command
  void sendRaw(String command) {
    _send(command);
  }

  // Set max balls
  void sendSetBalls(int n) {
    sendRaw('SET_BALLS:$n');
    state = state.copyWith(maxBalls: n);
  }

  // Send zone centers to ESP32.
  // [centers] is a list of (x, y) in robot working-area metres:
  //   X = working-area length axis
  //   Y = working-area width axis
  // Format sent: "ZONES:x1,y1;x2,y2;..."
  void sendZones(List<({double x, double y})> centers) {
    if (centers.isEmpty) return;
    final parts = centers
        .map((c) => '${c.x.toStringAsFixed(2)},${c.y.toStringAsFixed(2)}')
        .join(';');
    sendRaw('ZONES:$parts');
  }

  @override
  void dispose() {
    disconnect();
    super.dispose();
  }
}

final wifiConnectionProvider = StateNotifierProvider<WifiConnectionNotifier, WifiConnectionState>(
  (ref) {
    final terminal = ref.read(terminalProvider.notifier);
    return WifiConnectionNotifier(ref, terminal.addLine);
  },
);
