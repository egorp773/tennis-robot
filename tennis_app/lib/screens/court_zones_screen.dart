import 'dart:math' as math;
import 'dart:ui' as ui;

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../models/court_models.dart';
import '../painters/court_painter.dart';
import '../services/wifi_connection.dart';

const double _courtL = 23.77;
const double _courtW = 10.97;
const double _workMarginX = 3.00;
const double _workMarginY = 3.00;
const double _workL = _courtL + 2 * _workMarginX;
const double _workW = _courtW + 2 * _workMarginY;
const double _courtOriginX = _workMarginX;
const double _courtOriginY = _workMarginY;

// Zone centers are in robot working-area coordinates.
// Screen vertical maps to work X (length), screen horizontal maps to work Y (width).
// Playable court coordinates are shifted by (_courtOriginX, _courtOriginY).
const Map<String, ({double x, double y})> _zoneCenters = {
  // в”Ђв”Ђ Inside court в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
  'BACK_A':       (x:  5.74, y:  8.49),
  'SERV_L_A':     (x: 11.69, y:  5.74),
  'SERV_R_A':     (x: 11.69, y: 11.23),
  'SERV_L_B':     (x: 18.09, y:  5.74),
  'SERV_R_B':     (x: 18.09, y: 11.23),
  'BACK_B':       (x: 24.03, y:  8.49),
  // в”Ђв”Ђ Р—Р° РєРѕСЂС‚РѕРј в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
  'OUT_BACK_A':   (x:  0.00, y:  8.49),
  'OUT_SIDE_L_A': (x:  8.89, y:  0.00),
  'OUT_SIDE_R_A': (x:  8.89, y: 16.97),
  'OUT_SIDE_L_B': (x: 20.88, y:  0.00),
  'OUT_SIDE_R_B': (x: 20.88, y: 16.97),
  'OUT_BACK_B':   (x: 29.77, y:  8.49),
};

class CourtZonesScreen extends ConsumerStatefulWidget {
  const CourtZonesScreen({super.key});

  @override
  ConsumerState<CourtZonesScreen> createState() => _CourtZonesScreenState();
}

class _CourtZonesScreenState extends ConsumerState<CourtZonesScreen> {
  static const Color bg = Color(0xFF0E1318);
  static const Color panel = Color(0xFF10171E);
  static const Color accent = Color(0xFF9CCB4A);

  // Outer map coordinates are the robot working area. The playable court is inset.
  static const double courtL = _workL;
  static const double courtW = _workW;

  // Robot pose in normalized coords (0..1) relative to playable rect
  Offset _robot01 = const Offset(0.90, 0.92);
  double _robotAngle = -math.pi / 2; // up (screen)
  bool _robotSelected = true;

  // SET_BALLS text controller
  final TextEditingController _setBallsController = TextEditingController();

  // Developer mode
  bool _devModeEnabled = false;
  int _devTapCount = 0;
  DateTime? _lastDevTap;
  bool _showCoordinates = false;
  bool _showTrail = true;
  final TextEditingController _devCmdController = TextEditingController();

  // Multi zone selection
  final Set<String> _selectedZoneIds = {};

  // Status indicators for left/right squares
  final bool _leftConnected = true; // Left shows connected (green)
  final bool _leftError = false;
  final bool _rightConnected = false; // Right shows disconnected (white)
  final bool _rightError = false;

  // Robot settings
  int _ballCount = 0;
  int _ballMaxCapacity = 60;
  int _speedMode = 1; // 0: slow, 1: medium, 2: fast
  bool _autoMode = true;
  final int _batteryLevel = 87;
  final bool _isCharging = false;

  // Device names
  String _robotName = 'TennisBot';
  String _beaconLName = 'РњР°СЏРє L';
  String _beaconRName = 'РњР°СЏРє R';

  // Calibration state
  bool _showCalibrationMode = false;
  bool _showDeliveryPointMode = false;
  bool _calibrationCompleted = false;

  // Beacons (UWB DWM1000)
  final bool _beacon1Connected = true;
  final bool _beacon2Connected = true;

  // Delivery point
  Offset _deliveryPoint = const Offset(0.5, 0.95); // Bottom center by default
  bool _deliveryPointSkipped = false;
  bool _mapOnlyMode = false;

  // Gesture state
  DragMode _dragMode = DragMode.none;
  Offset _moveDeltaPx = Offset.zero;
  double _rotateOffset = 0.0;
  bool _isDraggingDeliveryPoint = false;

  Offset _joyOffset = Offset.zero;
  bool _joyActive = false;

  int _bottomTabIndex = 0;
  AppLang _lang = AppLang.ru;
  bool _rollersEnabled = false;

  String _tr(String ru, String en) => _lang == AppLang.ru ? ru : en;

  String _speedLabel(int mode) {
    switch (mode) {
      case 0:
        return _tr('РњРµРґР»РµРЅРЅРѕ', 'Slow');
      case 1:
        return _tr('РЎСЂРµРґРЅРµ', 'Medium');
      case 2:
        return _tr('Р‘С‹СЃС‚СЂРѕ', 'Fast');
      default:
        return '';
    }
  }

  IconData _speedIcon(int mode) {
    switch (mode) {
      case 0:
        return Icons.speed;
      case 1:
        return Icons.speed;
      case 2:
        return Icons.fast_forward;
      default:
        return Icons.speed;
    }
  }

  // Visual sizes
  static const double _robotRadius = 38;
  static const double _handleRadius = 22;    // Р±РѕР»СЊС€РѕР№ РєСЂСѓР¶РѕРє вЂ” Р»РµРіРєРѕ РїРѕРїР°СЃС‚СЊ
  static const double _handleDistance = 75;  // РґР°Р»РµРєРѕ Р·Р° РїСЂРµРґРµР»Р°РјРё С‚РµР»Р° (radius=38)

  // Robot image
  ui.Image? _robotImage;

  @override
  void initState() {
    super.initState();
    _loadRobotImage();
  }

  @override
  void dispose() {
    _setBallsController.dispose();
    _devCmdController.dispose();
    super.dispose();
  }

  void _loadRobotImage() async {
    final ByteData data = await rootBundle.load('assets/images/robot.png');
    final Uint8List bytes = data.buffer.asUint8List();
    final ui.Codec codec = await ui.instantiateImageCodec(bytes);
    final ui.FrameInfo fi = await codec.getNextFrame();
    setState(() {
      _robotImage = fi.image;
    });
  }

  @override
  Widget build(BuildContext context) {
    // Show a SnackBar when ESP32 sends a non-fatal ERROR (e.g. NOT_CALIBRATED)
    // Track UWB position history for trail
    ref.listen<WifiConnectionState>(wifiConnectionProvider, (prev, next) {
      if (next.isConnected && (next.robotX != prev?.robotX || next.robotY != prev?.robotY)) {
        if (next.robotX != 0 || next.robotY != 0) {
          setState(() {
            _positionTrail.add(Offset(next.robotY / _workW, next.robotX / _workL));
            if (_positionTrail.length > 30) _positionTrail.removeAt(0);
          });
        }
      }
      if (!next.isConnected) {
        setState(() => _positionTrail.clear());
      }
    });

    ref.listen<WifiConnectionState>(wifiConnectionProvider, (prev, next) {
      if (next.robotError != null && next.robotError != prev?.robotError) {
        final msg = next.robotError == 'NOT_CALIBRATED'
            ? _tr('РЎРЅР°С‡Р°Р»Р° РІС‹РїРѕР»РЅРё РєР°Р»РёР±СЂРѕРІРєСѓ', 'Calibrate first')
            : next.robotError!;
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(msg), backgroundColor: Colors.red[700]),
        );
      }
    });

    ref.listen<WifiConnectionState>(wifiConnectionProvider, (prev, next) {
      if (next.robotWarning != null && next.robotWarning != prev?.robotWarning) {
        final msg = next.robotWarning!;
        if (msg.startsWith('LOW_BATTERY,')) {
          final pct = msg.split(',').last;
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
              content: Text(_lang == AppLang.ru
                  ? 'РќРёР·РєРёР№ Р·Р°СЂСЏРґ Р±Р°С‚Р°СЂРµРё: $pct%. Р РѕР±РѕС‚ Р·Р°РїСѓС‰РµРЅ.'
                  : 'Low battery: $pct%. Robot started.'),
              backgroundColor: Colors.orange[800],
              duration: const Duration(seconds: 5),
            ),
          );
        } else if (msg == 'PI_TIMEOUT') {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
              content: Text(_lang == AppLang.ru
                  ? 'РќРµС‚ СЃРІСЏР·Рё СЃ РєР°РјРµСЂРѕР№ (Pi). РџРѕРёСЃРє РјСЏС‡РµР№ РїСЂРёРѕСЃС‚Р°РЅРѕРІР»РµРЅ.'
                  : 'Camera (Pi) not responding. Ball search paused.'),
              backgroundColor: Colors.orange[800],
            ),
          );
        }
      }
    });

    return Scaffold(
      backgroundColor: bg,
      body: SafeArea(
        child: IndexedStack(
          index: _bottomTabIndex,
          children: [
            _buildMapTab(),
            _buildSettingsTab(),
          ],
        ),
      ),
      bottomNavigationBar: (_showCalibrationMode || _showDeliveryPointMode)
        ? null 
        : BottomNavigationBar(
            currentIndex: _bottomTabIndex,
            onTap: (i) => setState(() => _bottomTabIndex = i),
            backgroundColor: panel,
            selectedItemColor: accent,
            unselectedItemColor: const Color(0xFFA9B3BC),
            type: BottomNavigationBarType.fixed,
            iconSize: 18,
            selectedFontSize: 10,
            unselectedFontSize: 10,
            items: [
              BottomNavigationBarItem(icon: const Icon(Icons.map_outlined), label: _tr('РљР°СЂС‚Р°', 'Map')),
              BottomNavigationBarItem(icon: const Icon(Icons.tune), label: _tr('РќР°СЃС‚СЂРѕР№РєРё', 'Settings')),
            ],
          ),
    );
  }

  Widget _buildMapTab() {
    if (_showDeliveryPointMode) {
      return _buildDeliveryPointMode();
    }
    if (_showCalibrationMode) {
      return _buildZoneSelectionMode();
    }
    // Map-only mode after calibration: show map with controls
    if (_mapOnlyMode) {
      return _buildMapOnlyMode();
    }
    return Column(
      children: [
        Expanded(child: _buildMapCanvas()),
        _buildCalibrationBar(),
        _buildBottomJoystickDock(),
      ],
    );
  }

  Widget _buildZoneSelectionMode() {
    return SafeArea(
      child: Column(
        children: [
          // Header text
          Container(
            padding: const EdgeInsets.fromLTRB(20, 39, 20, 0),
            child: Text(
              _tr(
                'Р’С‹Р±РµСЂРёС‚Рµ Р·РѕРЅС‹, РІ РєРѕС‚РѕСЂС‹С… СЂРѕР±РѕС‚ Р±СѓРґРµС‚ СЃРѕР±РёСЂР°С‚СЊ РјСЏС‡Рё',
                'Select zones where the robot will collect balls',
              ),
              textAlign: TextAlign.center,
              style: const TextStyle(
                color: Colors.white,
                fontSize: 16,
                fontWeight: FontWeight.w600,
                height: 1.3,
              ),
            ),
          ),
          // Full screen map - no scale transform in calibration mode
          Expanded(
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 16),
              child: _buildMapCanvasForCalibration(),
            ),
          ),
          // Done button
          Container(
            padding: const EdgeInsets.all(16),
            child: SizedBox(
              width: double.infinity,
              child: ElevatedButton(
                onPressed: () {
                  setState(() {
                    _showCalibrationMode = false;
                    _showDeliveryPointMode = true;
                  });
                },
                style: ElevatedButton.styleFrom(
                  backgroundColor: accent,
                  foregroundColor: Colors.black,
                  padding: const EdgeInsets.symmetric(vertical: 14),
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(12),
                  ),
                  elevation: 0,
                ),
                child: Text(
                  _tr('Р“РѕС‚РѕРІРѕ', 'Done'),
                  style: const TextStyle(
                    fontWeight: FontWeight.w700,
                    fontSize: 15,
                  ),
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildCalibrationBar() {
    final wifiState = ref.watch(wifiConnectionProvider);
    final wifiConnection = ref.read(wifiConnectionProvider.notifier);

    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      color: panel,
      child: Row(
        children: [
          // Left spacer for symmetry
          const SizedBox(width: 48),
          const SizedBox(width: 12),
          // Main button - Connect or Calibration
          Expanded(
            child: wifiState.isConnected
                ? _buildCalibrationButton()
                : ElevatedButton(
                    onPressed: wifiState.isConnecting
                        ? null
                        : () async {
                            await wifiConnection.connect();
                          },
                    style: ElevatedButton.styleFrom(
                      backgroundColor: accent,
                      foregroundColor: Colors.black,
                      padding: const EdgeInsets.symmetric(vertical: 18, horizontal: 24),
                      shape: RoundedRectangleBorder(
                        borderRadius: BorderRadius.circular(12),
                      ),
                      elevation: 0,
                    ),
                    child: wifiState.isConnecting
                        ? const SizedBox(
                            width: 20,
                            height: 20,
                            child: CircularProgressIndicator(strokeWidth: 2, color: Colors.black),
                          )
                        : Text(
                            _tr('РџРѕРґРєР»СЋС‡РёС‚СЊ', 'Connect'),
                            style: const TextStyle(
                              fontSize: 15,
                              fontWeight: FontWeight.w700,
                            ),
                          ),
                  ),
          ),
          const SizedBox(width: 12),
          // Help button - only show when connected
          if (wifiState.isConnected) _buildHelpButton() else const SizedBox(width: 48),
        ],
      ),
    );
  }

  Widget _buildCalibrationButton() {
    final isCalibrating = _showCalibrationMode;
    return ElevatedButton(
      onPressed: () => _onCalibrationPressed(),
      style: ElevatedButton.styleFrom(
        backgroundColor: isCalibrating ? const Color(0xFFFF6B6B) : accent,
        foregroundColor: Colors.black,
        padding: const EdgeInsets.symmetric(vertical: 18, horizontal: 24),
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(12),
        ),
        elevation: 0,
      ),
      child: Text(
        _tr('РљР°Р»РёР±СЂРѕРІРєР°', 'Calibration'),
        style: const TextStyle(
          fontSize: 15,
          fontWeight: FontWeight.w700,
        ),
      ),
    );
  }

  Widget _buildHelpButton() {
    return ElevatedButton(
      onPressed: () => _showHelpDialog(),
      style: ElevatedButton.styleFrom(
        backgroundColor: accent,
        foregroundColor: Colors.black,
        padding: const EdgeInsets.symmetric(vertical: 12, horizontal: 16),
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(12),
        ),
        elevation: 0,
        minimumSize: const Size(48, 36),
      ),
      child: const Text(
        '?',
        style: TextStyle(
          fontSize: 16,
          fontWeight: FontWeight.w700,
        ),
      ),
    );
  }

  void _onCalibrationPressed() {
    setState(() {
      _showCalibrationMode = !_showCalibrationMode;
    });
  }

  void _setRollers(bool enabled, WifiConnectionNotifier wifiConnection) {
    wifiConnection.sendAttachment(enabled);
    setState(() => _rollersEnabled = enabled);
  }

  Widget _buildRollersButton(
    WifiConnectionNotifier wifiConnection, {
    bool compact = false,
  }) {
    final enabled = _rollersEnabled;
    return SizedBox(
      width: double.infinity,
      child: ElevatedButton.icon(
        onPressed: () => _setRollers(!enabled, wifiConnection),
        icon: Icon(
          enabled ? Icons.motion_photos_on : Icons.motion_photos_off,
          size: compact ? 18 : 22,
        ),
        label: Text(
          _tr(
            enabled
                ? (compact ? 'Ролики: выкл' : 'Выключить ролики')
                : (compact ? 'Ролики: вкл' : 'Включить ролики'),
            enabled
                ? (compact ? 'Rollers: off' : 'Rollers off')
                : (compact ? 'Rollers: on' : 'Rollers on'),
          ),
        ),
        style: ElevatedButton.styleFrom(
          backgroundColor: enabled ? accent : panel,
          foregroundColor: enabled ? Colors.black : Colors.white,
          padding: EdgeInsets.symmetric(
            vertical: compact ? 12 : 14,
            horizontal: compact ? 10 : 18,
          ),
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(12),
            side: BorderSide(
              color: enabled ? accent : Colors.white.withValues(alpha: 0.14),
            ),
          ),
          elevation: 0,
        ),
      ),
    );
  }

  void _sendCalibration() {
    final wifiConnection = ref.read(wifiConnectionProvider.notifier);

    // Convert normalized screen coords to ESP32 working-area coordinates.
    // Screen dx maps to working-area Y; screen dy maps to working-area X.
    final espX = _robot01.dy * _workL;
    final espY = _robot01.dx * _workW;

    // Convert Flutter robotAngle (canvas.rotate, CW positive, 0=facing up) to ESP32 heading degrees.
    // ESP32 heading: 0=+X direction (screen down), 90=+Y (screen right), atan2(dy,dx) convention.
    // Formula: heading = 180 - degrees(robotAngle), normalized to -180..180
    double headingDeg = 180.0 - _robotAngle * 180.0 / math.pi;
    while (headingDeg > 180) {
      headingDeg -= 360;
    }
    while (headingDeg < -180) {
      headingDeg += 360;
    }

    wifiConnection.sendRaw(
      'CALIBRATE:${headingDeg.toStringAsFixed(1)}:${espX.toStringAsFixed(2)}:${espY.toStringAsFixed(2)}',
    );

    // Send zone targets after calibration
    if (_selectedZoneIds.isNotEmpty) {
      final centers = _selectedZoneIds
          .map((id) => _zoneCenters[id])
          .whereType<({double x, double y})>()
          .where((c) => c.x >= 0 && c.x <= _workL && c.y >= 0 && c.y <= _workW)
          .toList();
      if (centers.isNotEmpty) {
        wifiConnection.sendZones(centers);
      }
    }
  }

  void _showHelpDialog() {
    showModalBottomSheet(
      context: context,
      backgroundColor: Colors.transparent,
      isScrollControlled: true,
      builder: (context) => Container(
        margin: const EdgeInsets.all(12),
        decoration: BoxDecoration(
          color: const Color(0xFF1A2229),
          borderRadius: BorderRadius.circular(24),
          border: Border.all(color: Colors.white.withValues(alpha: 0.08)),
        ),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            // Handle
            Container(
              margin: const EdgeInsets.only(top: 12, bottom: 8),
              width: 40,
              height: 4,
              decoration: BoxDecoration(
                color: Colors.white.withValues(alpha: 0.2),
                borderRadius: BorderRadius.circular(2),
              ),
            ),
            // Title
            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 12),
              child: Row(
                children: [
                  Container(
                    padding: const EdgeInsets.all(8),
                    decoration: BoxDecoration(
                      color: accent.withValues(alpha: 0.15),
                      borderRadius: BorderRadius.circular(10),
                    ),
                    child: Icon(Icons.help_outline, color: accent, size: 20),
                  ),
                  const SizedBox(width: 12),
                  Text(
                    _tr('РџРѕРјРѕС‰СЊ', 'Help'),
                    style: const TextStyle(
                      color: Colors.white,
                      fontWeight: FontWeight.w700,
                      fontSize: 18,
                    ),
                  ),
                ],
              ),
            ),
            // Steps
            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 8),
              child: Column(
                children: [
                  _buildModernStep('1', _tr(
                    'РЎ РїРѕРјРѕС‰СЊСЋ РґР¶РѕР№СЃС‚РёРєР° СѓСЃС‚Р°РЅРѕРІРёС‚Рµ СЂРѕР±РѕС‚Р° РЅР° РїРѕР»Рµ',
                    'Use the joystick to place the robot on the field',
                  )),
                  const SizedBox(height: 10),
                  _buildModernStep('2', _tr(
                    'РЈР±РµРґРёС‚РµСЃСЊ, С‡С‚Рѕ РѕР±Р° РјР°СЏРєР° (L Рё R) РІРєР»СЋС‡РµРЅС‹ Рё РіРѕСЂСЏС‚ Р·РµР»С‘РЅС‹Рј',
                    'Make sure both beacons (L and R) are on and glowing green',
                  )),
                  const SizedBox(height: 10),
                  _buildModernStep('3', _tr(
                    'РќР°Р¶РјРёС‚Рµ РЅР° СЂРѕР±РѕС‚Р° РЅР° СЌРєСЂР°РЅРµ Рё СѓСЃС‚Р°РЅРѕРІРёС‚Рµ РµРіРѕ РІ С‚Р°РєРѕРµ Р¶Рµ РїРѕР»РѕР¶РµРЅРёРµ, РєР°Рє РІ СЂРµР°Р»СЊРЅРѕСЃС‚Рё',
                    'Tap the robot on screen and set it to the same position as in reality',
                  )),
                  const SizedBox(height: 10),
                  _buildModernStep('4', _tr(
                    'РќР°Р¶РјРёС‚Рµ РєРЅРѕРїРєСѓ В«РљР°Р»РёР±СЂРѕРІРєР°В» РґР»СЏ РѕРїСЂРµРґРµР»РµРЅРёСЏ С‚РѕС‡РЅРѕР№ РїРѕР·РёС†РёРё',
                    'Press the "Calibration" button to determine the exact position',
                  )),
                ],
              ),
            ),
            // Button
            Padding(
              padding: const EdgeInsets.all(20),
              child: SizedBox(
                width: double.infinity,
                child: ElevatedButton(
                  onPressed: () => Navigator.of(context).pop(),
                  style: ElevatedButton.styleFrom(
                    backgroundColor: accent,
                    foregroundColor: Colors.black,
                    padding: const EdgeInsets.symmetric(vertical: 14),
                    shape: RoundedRectangleBorder(
                      borderRadius: BorderRadius.circular(12),
                    ),
                    elevation: 0,
                  ),
                  child: Text(
                    _tr('РџРѕРЅСЏС‚РЅРѕ', 'Got it'),
                    style: const TextStyle(
                      fontWeight: FontWeight.w700,
                      fontSize: 15,
                    ),
                  ),
                ),
              ),
            ),
            const SizedBox(height: 8),
          ],
        ),
      ),
    );
  }

  Widget _buildModernStep(String number, String text) {
    return Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: 0.04),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: Colors.white.withValues(alpha: 0.06)),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Container(
            width: 26,
            height: 26,
            decoration: BoxDecoration(
              color: accent,
              borderRadius: BorderRadius.circular(8),
            ),
            child: Center(
              child: Text(
                number,
                style: const TextStyle(
                  color: Colors.black,
                  fontWeight: FontWeight.w800,
                  fontSize: 13,
                ),
              ),
            ),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Text(
              text,
              style: TextStyle(
                fontSize: 14,
                color: Colors.white.withValues(alpha: 0.9),
                height: 1.4,
              ),
            ),
          ),
        ],
      ),
    );
  }

  void _showEditNameDialog({
    required String title,
    required String initialValue,
    required ValueChanged<String> onSave,
  }) {
    final controller = TextEditingController(text: initialValue);
    showModalBottomSheet(
      context: context,
      backgroundColor: Colors.transparent,
      isScrollControlled: true,
      builder: (context) => Align(
        alignment: Alignment.center,
        child: Container(
          margin: const EdgeInsets.all(12),
          padding: EdgeInsets.only(bottom: MediaQuery.of(context).viewInsets.bottom),
          decoration: BoxDecoration(
            color: const Color(0xFF1A2229),
            borderRadius: BorderRadius.circular(24),
            border: Border.all(color: Colors.white.withValues(alpha: 0.08)),
          ),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              // Handle
              Container(
                margin: const EdgeInsets.only(top: 12, bottom: 8),
                width: 40,
                height: 4,
                decoration: BoxDecoration(
                  color: Colors.white.withValues(alpha: 0.2),
                  borderRadius: BorderRadius.circular(2),
                ),
              ),
              // Title
              Padding(
                padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 12),
                child: Row(
                  children: [
                    Container(
                      padding: const EdgeInsets.all(8),
                      decoration: BoxDecoration(
                        color: accent.withValues(alpha: 0.15),
                        borderRadius: BorderRadius.circular(10),
                      ),
                      child: Icon(Icons.edit, color: accent, size: 20),
                    ),
                    const SizedBox(width: 12),
                    Text(
                      title,
                      style: const TextStyle(
                        color: Colors.white,
                        fontWeight: FontWeight.w700,
                        fontSize: 18,
                      ),
                    ),
                  ],
                ),
              ),
              // Input
              Padding(
                padding: const EdgeInsets.symmetric(horizontal: 20),
                child: TextField(
                  controller: controller,
                  autofocus: true,
                  style: const TextStyle(fontSize: 16, color: Colors.white),
                  decoration: InputDecoration(
                    hintText: _tr('Р’РІРµРґРёС‚Рµ РёРјСЏ', 'Enter name'),
                    hintStyle: const TextStyle(color: Color(0xFFA9B3BC)),
                    filled: true,
                    fillColor: bg,
                    border: OutlineInputBorder(
                      borderRadius: BorderRadius.circular(12),
                      borderSide: BorderSide.none,
                    ),
                  ),
                ),
              ),
              const SizedBox(height: 16),
              // Buttons
              Padding(
                padding: const EdgeInsets.fromLTRB(20, 0, 20, 20),
                child: Row(
                  children: [
                    Expanded(
                      child: ElevatedButton(
                        onPressed: () => Navigator.of(context).pop(),
                        style: ElevatedButton.styleFrom(
                          backgroundColor: panel,
                          foregroundColor: Colors.white,
                          padding: const EdgeInsets.symmetric(vertical: 14),
                          shape: RoundedRectangleBorder(
                            borderRadius: BorderRadius.circular(12),
                          ),
                          elevation: 0,
                        ),
                        child: Text(_tr('РћС‚РјРµРЅР°', 'Cancel'), style: const TextStyle(fontWeight: FontWeight.w700, fontSize: 15)),
                      ),
                    ),
                    const SizedBox(width: 12),
                    Expanded(
                      child: ElevatedButton(
                        onPressed: () {
                          if (controller.text.trim().isNotEmpty) {
                            onSave(controller.text.trim());
                          }
                          Navigator.of(context).pop();
                        },
                        style: ElevatedButton.styleFrom(
                          backgroundColor: accent,
                          foregroundColor: Colors.black,
                          padding: const EdgeInsets.symmetric(vertical: 14),
                          shape: RoundedRectangleBorder(
                            borderRadius: BorderRadius.circular(12),
                          ),
                          elevation: 0,
                        ),
                        child: Text(_tr('РЎРѕС…СЂР°РЅРёС‚СЊ', 'Save'), style: const TextStyle(fontWeight: FontWeight.w700, fontSize: 15)),
                      ),
                    ),
                  ],
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildMapCanvasForCalibration() {
    return Center(
      child: Transform.scale(
        scale: 1.2,
        child: AspectRatio(
          aspectRatio: 0.72,
          child: LayoutBuilder(
            builder: (context, c) {
              final size = Size(c.maxWidth, c.maxHeight);
              final playable = _playableRect(size);
              final zonesPx = _buildZonesPx(playable);
              return GestureDetector(
                behavior: HitTestBehavior.opaque,
                onTapDown: (d) {
                  final z = _hitZone(d.localPosition, zonesPx);
                  if (z != null) {
                    setState(() {
                      if (_selectedZoneIds.contains(z.id)) {
                        _selectedZoneIds.remove(z.id);
                      } else {
                        _selectedZoneIds.add(z.id);
                      }
                    });
                    return;
                  }
                  setState(() => _robotSelected = false);
                },
                onPanStart: null,
                onPanUpdate: null,
                onPanEnd: null,
                child: CustomPaint(
                  painter: CourtPainter(
                    panel: panel,
                    accent: accent,
                    playable: playable,
                    zonesPx: zonesPx,
                    selectedZoneIds: _selectedZoneIds,
                    robot01: _robot01,
                    robotAngle: _robotAngle,
                    robotSelected: false,
                    robotRadius: _robotRadius,
                    handleRadius: _handleRadius,
                    handleDistance: _handleDistance,
                    robotImage: _robotImage,
                    leftConnected: _leftConnected,
                    leftError: _leftError,
                    rightConnected: _rightConnected,
                    rightError: _rightError,
                    deliveryPoint: null,
                    beacon1Connected: _beacon1Connected,
                    beacon2Connected: _beacon2Connected,
                    isRotating: false,
                    showHandle: true,
                  ),
                  child: const SizedBox.expand(),
                ),
              );
            },
          ),
        ),
      ),
    );
  }

  Widget _buildDeliveryPointMode() {
    return SafeArea(
      child: Column(
        children: [
          // Header text
          Container(
            padding: const EdgeInsets.fromLTRB(20, 39, 20, 0),
            child: Text(
              _tr(
                'Р’С‹Р±РµСЂРёС‚Рµ РјРµСЃС‚Рѕ, РєСѓРґР° СЂРѕР±РѕС‚ Р±СѓРґРµС‚ РїСЂРёРІРѕР·РёС‚СЊ РјСЏС‡Рё',
                'Select where the robot will deliver balls',
              ),
              textAlign: TextAlign.center,
              style: const TextStyle(
                color: Colors.white,
                fontSize: 16,
                fontWeight: FontWeight.w600,
                height: 1.3,
              ),
            ),
          ),
          // Full screen map
          Expanded(
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 16),
              child: _buildMapCanvasForDeliveryPoint(),
            ),
          ),
          // Buttons row
          Container(
            padding: const EdgeInsets.all(16),
            child: Row(
              children: [
                // Skip button
                Expanded(
                  child: ElevatedButton(
                    onPressed: () {
                      setState(() {
                        _showDeliveryPointMode = false;
                        _calibrationCompleted = true;
                        _deliveryPointSkipped = true;
                        _mapOnlyMode = true;
                      });
                      _sendCalibration();
                    },
                    style: ElevatedButton.styleFrom(
                      backgroundColor: panel,
                      foregroundColor: Colors.white,
                      padding: const EdgeInsets.symmetric(vertical: 14),
                      shape: RoundedRectangleBorder(
                        borderRadius: BorderRadius.circular(12),
                      ),
                      elevation: 0,
                    ),
                    child: Text(
                      _tr('РџСЂРѕРїСѓСЃС‚РёС‚СЊ', 'Skip'),
                      style: const TextStyle(
                        fontWeight: FontWeight.w700,
                        fontSize: 15,
                      ),
                    ),
                  ),
                ),
                const SizedBox(width: 12),
                // Done button
                Expanded(
                  child: ElevatedButton(
                    onPressed: () {
                      setState(() {
                        _showDeliveryPointMode = false;
                        _calibrationCompleted = true;
                        _deliveryPointSkipped = false;
                        _mapOnlyMode = true;
                      });
                      _sendCalibration();
                    },
                    style: ElevatedButton.styleFrom(
                      backgroundColor: accent,
                      foregroundColor: Colors.black,
                      padding: const EdgeInsets.symmetric(vertical: 14),
                      shape: RoundedRectangleBorder(
                        borderRadius: BorderRadius.circular(12),
                      ),
                      elevation: 0,
                    ),
                    child: Text(
                      _tr('Р“РѕС‚РѕРІРѕ', 'Done'),
                      style: const TextStyle(
                        fontWeight: FontWeight.w700,
                        fontSize: 15,
                      ),
                    ),
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildMapCanvasForDeliveryPoint() {
    return Center(
      child: Transform.scale(
        scale: 1.2,
        child: AspectRatio(
          aspectRatio: 0.72,
          child: LayoutBuilder(
            builder: (context, c) {
              final size = Size(c.maxWidth, c.maxHeight);
              final playable = _playableRect(size);
              final zonesPx = _buildZonesPx(playable);
              
              // Calculate delivery point in pixels
              Offset deliveryPx() {
                return Offset(
                  playable.left + _deliveryPoint.dx * playable.width,
                  playable.top + _deliveryPoint.dy * playable.height,
              );
              }
              
              // Check if tap is on delivery point
              bool hitDeliveryPoint(Offset p) {
                final dp = deliveryPx();
                return (p - dp).distance <= 30; // 30px touch radius
              }
              
              return GestureDetector(
                behavior: HitTestBehavior.opaque,
                onTapDown: (d) {
                  // Set delivery point on tap if not hitting existing point
                  if (!hitDeliveryPoint(d.localPosition)) {
                    final tapPos = d.localPosition;
                    final relativeX = (tapPos.dx - playable.left) / playable.width;
                    final relativeY = (tapPos.dy - playable.top) / playable.height;
                    final newPoint = Offset(
                      relativeX.clamp(0.0, 1.0),
                      relativeY.clamp(0.0, 1.0),
                    );
                    setState(() {
                      _deliveryPoint = newPoint;
                    });
                    final wifiConn = ref.read(wifiConnectionProvider.notifier);
                    final homeX = newPoint.dy * _workL;
                    final homeY = newPoint.dx * _workW;
                    wifiConn.sendRaw('SET_HOME:${homeX.toStringAsFixed(2)},${homeY.toStringAsFixed(2)}');
                  }
                },
                onPanStart: (d) {
                  if (hitDeliveryPoint(d.localPosition)) {
                    setState(() => _isDraggingDeliveryPoint = true);
                  }
                },
                onPanUpdate: (d) {
                  if (_isDraggingDeliveryPoint) {
                    final tapPos = d.localPosition;
                    final relativeX = (tapPos.dx - playable.left) / playable.width;
                    final relativeY = (tapPos.dy - playable.top) / playable.height;
                    setState(() {
                      _deliveryPoint = Offset(
                        relativeX.clamp(0.0, 1.0),
                        relativeY.clamp(0.0, 1.0),
                      );
                    });
                  }
                },
                onPanEnd: (d) {
                  if (_isDraggingDeliveryPoint) {
                    final wifiConn = ref.read(wifiConnectionProvider.notifier);
                    final homeX = _deliveryPoint.dy * _workL;
                    final homeY = _deliveryPoint.dx * _workW;
                    wifiConn.sendRaw('SET_HOME:${homeX.toStringAsFixed(2)},${homeY.toStringAsFixed(2)}');
                  }
                  setState(() => _isDraggingDeliveryPoint = false);
                },
                onPanCancel: () => setState(() => _isDraggingDeliveryPoint = false),
                child: CustomPaint(
                  painter: CourtPainter(
                    panel: panel,
                    accent: accent,
                    playable: playable,
                    zonesPx: zonesPx,
                    selectedZoneIds: _selectedZoneIds,
                    robot01: _robot01,
                    robotAngle: _robotAngle,
                    robotSelected: false,
                    robotRadius: _robotRadius,
                    handleRadius: _handleRadius,
                    handleDistance: _handleDistance,
                    robotImage: _robotImage,
                    leftConnected: _leftConnected,
                    leftError: _leftError,
                    rightConnected: _rightConnected,
                    rightError: _rightError,
                    deliveryPoint: _deliveryPoint,
                    beacon1Connected: _beacon1Connected,
                    beacon2Connected: _beacon2Connected,
                    isRotating: false,
                  ),
                  child: const SizedBox.expand(),
                ),
              );
            },
          ),
        ),
      ),
    );
  }

  // Trail of recent UWB positions (normalized 0..1 for painter)
  final List<Offset> _positionTrail = [];

  Widget _buildMapCanvas() {
    final wifiState = ref.watch(wifiConnectionProvider);

    // ESP32 coord: X=0.._workL (working length, vertical on screen) maps from dy.
    //              Y=0.._workW (working width, horizontal on screen) maps from dx.
    final effectiveRobot01 = wifiState.isConnected && (wifiState.robotX != 0 || wifiState.robotY != 0)
        ? Offset(wifiState.robotY / _workW, wifiState.robotX / _workL)
        : _robot01;
    final effectiveLeftConnected = wifiState.isConnected ? wifiState.anchorLOnline : _leftConnected;
    final effectiveRightConnected = wifiState.isConnected ? wifiState.anchorROnline : _rightConnected;

    return Center(
      child: Transform.scale(
        scale: 0.95,
        child: AspectRatio(
          aspectRatio: 0.72,
          child: LayoutBuilder(
            builder: (context, c) {
              final size = Size(c.maxWidth, c.maxHeight);
              final playable = _playableRect(size);
              final zonesPx = _buildZonesPx(playable);
              return GestureDetector(
                behavior: HitTestBehavior.opaque,
                onTapDown: (d) {
                  // Block zone selection on main screen (only allow in calibration mode)
                  if (!_showCalibrationMode) {
                    // Only handle robot deselection on main screen
                    setState(() => _robotSelected = false);
                    return;
                  }
                  final z = _hitZone(d.localPosition, zonesPx);
                  if (z != null) {
                    setState(() {
                      if (_selectedZoneIds.contains(z.id)) {
                        _selectedZoneIds.remove(z.id);
                      } else {
                        _selectedZoneIds.add(z.id);
                      }
                    });
                    return;
                  }
                  setState(() => _robotSelected = false);
                },
                onPanStart: (d) => _showCalibrationMode ? null : _onPanStart(d.localPosition, playable),
                onPanUpdate: (d) => _showCalibrationMode ? null : _onPanUpdate(d.localPosition, playable),
                onPanEnd: (_) => _showCalibrationMode ? null : setState(() => _dragMode = DragMode.none),
                child: Stack(
                  children: [
                    CustomPaint(
                      painter: CourtPainter(
                        panel: panel,
                        accent: accent,
                        playable: playable,
                        zonesPx: zonesPx,
                        selectedZoneIds: _selectedZoneIds,
                        robot01: effectiveRobot01,
                        robotAngle: _robotAngle,
                        robotSelected: _showCalibrationMode ? false : _robotSelected,
                        robotRadius: _robotRadius,
                        handleRadius: _handleRadius,
                        handleDistance: _handleDistance,
                        robotImage: _robotImage,
                        leftConnected: effectiveLeftConnected,
                        leftError: _leftError,
                        rightConnected: effectiveRightConnected,
                        rightError: _rightError,
                        deliveryPoint: (_calibrationCompleted && !_deliveryPointSkipped) ? _deliveryPoint : null,
                        beacon1Connected: _beacon1Connected,
                        beacon2Connected: _beacon2Connected,
                        isRotating: _dragMode == DragMode.rotateRobot,
                        positionTrail: _showTrail ? List.unmodifiable(_positionTrail) : const [],
                        livePosition: wifiState.isConnected && (wifiState.robotX != 0 || wifiState.robotY != 0),
                        showHandle: false,
                        showCoordinates: _showCoordinates,
                      ),
                      child: const SizedBox.expand(),
                    ),
                  ],
                ),
              );
            },
          ),
        ),
      ),
    );
  }

  // Map-only mode widget with smaller map and controls
  Widget _buildMapOnlyMode() {
    final wifiState = ref.watch(wifiConnectionProvider);
    final wifiConnection = ref.read(wifiConnectionProvider.notifier);
    const maxRadius = _CoolJoystick._radius;
    // Speed mode multiplier: 0=slow(~50%), 1=medium(~75%), 2=fast(100%)
    final speedMultiplier = switch (_speedMode) {
      0 => 0.5,
      1 => 0.75,
      2 => 1.0,
      _ => 0.75,
    };

    final effectiveRobot01 = wifiState.isConnected && (wifiState.robotX != 0 || wifiState.robotY != 0)
        ? Offset(wifiState.robotY / _workW, wifiState.robotX / _workL)
        : _robot01;
    final effectiveLeftConnected = wifiState.isConnected ? wifiState.anchorLOnline : _leftConnected;
    final effectiveRightConnected = wifiState.isConnected ? wifiState.anchorROnline : _rightConnected;
    final displayBallsCount = wifiState.isConnected ? wifiState.ballsCount : _ballCount;
    final displayMaxBalls = wifiState.isConnected ? wifiState.maxBalls : _ballMaxCapacity;
    final isAutoMode = wifiState.robotState == 'AUTO';

    void sendFromOffset(Offset o) {
      final nx = (o.dx / maxRadius).clamp(-1.0, 1.0);
      final ny = (o.dy / maxRadius).clamp(-1.0, 1.0);
      final forward = -ny;
      final turn = nx;
      final left = ((forward + turn) * 100 * speedMultiplier).round();
      final right = ((forward - turn) * 100 * speedMultiplier).round();
      wifiConnection.sendMove(left, right);
    }

    void maybeStop(Offset o) {
      final nx = (o.dx / maxRadius).abs();
      final ny = (o.dy / maxRadius).abs();
      if (nx < 0.05 && ny < 0.05) {
        wifiConnection.sendStop();
      } else {
        sendFromOffset(o);
      }
    }

    return SafeArea(
      child: Column(
        children: [
          // Map at top (scale 1.0)
          SizedBox(
            height: MediaQuery.of(context).size.height * 0.5,
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
              child: Center(
                child: Transform.scale(
                  scale: 1.0,
                  child: AspectRatio(
                    aspectRatio: 0.72,
                    child: LayoutBuilder(
                      builder: (context, c) {
                        final size = Size(c.maxWidth, c.maxHeight);
                        final playable = _playableRect(size);
                        final zonesPx = _buildZonesPx(playable);
                        return GestureDetector(
                          behavior: HitTestBehavior.opaque,
                          onTapDown: (d) {
                            setState(() => _robotSelected = false);
                          },
                          child: CustomPaint(
                            painter: CourtPainter(
                              panel: panel,
                              accent: accent,
                              playable: playable,
                              zonesPx: zonesPx,
                              selectedZoneIds: _selectedZoneIds,
                              robot01: effectiveRobot01,
                              robotAngle: _robotAngle,
                              robotSelected: false,
                              robotRadius: _robotRadius,
                              handleRadius: _handleRadius,
                              handleDistance: _handleDistance,
                              robotImage: _robotImage,
                              leftConnected: effectiveLeftConnected,
                              leftError: _leftError,
                              rightConnected: effectiveRightConnected,
                              rightError: _rightError,
                              deliveryPoint: (_calibrationCompleted && !_deliveryPointSkipped) ? _deliveryPoint : null,
                              beacon1Connected: _beacon1Connected,
                              beacon2Connected: _beacon2Connected,
                              isRotating: false,
                              positionTrail: _showTrail ? List.unmodifiable(_positionTrail) : const [],
                              livePosition: wifiState.isConnected && (wifiState.robotX != 0 || wifiState.robotY != 0),
                              showHandle: false,
                              showCoordinates: _showCoordinates,
                            ),
                            child: const SizedBox.expand(),
                          ),
                        );
                      },
                    ),
                  ),
                ),
              ),
            ),
          ),
          // Control panel below
          Expanded(
            child: Container(
              padding: const EdgeInsets.all(16),
              child: Column(
                children: [
                  // Ball counter with status
                  Container(
                    padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 14),
                    decoration: BoxDecoration(
                      color: panel,
                      borderRadius: BorderRadius.circular(16),
                    ),
                    child: Row(
                      mainAxisAlignment: MainAxisAlignment.spaceBetween,
                      children: [
                        Row(
                          children: [
                            Container(
                              padding: const EdgeInsets.all(8),
                              decoration: BoxDecoration(
                                color: accent.withValues(alpha: 0.15),
                                borderRadius: BorderRadius.circular(10),
                              ),
                              child: const Icon(Icons.sports_tennis, color: accent, size: 24),
                            ),
                            const SizedBox(width: 12),
                            Column(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                Text(
                                  '$displayBallsCount',
                                  style: const TextStyle(
                                    fontSize: 28,
                                    fontWeight: FontWeight.w700,
                                    color: Colors.white,
                                  ),
                                ),
                                Text(
                                  '${_tr('РјР°РєСЃ.', 'max')} $displayMaxBalls',
                                  style: const TextStyle(
                                    fontSize: 11,
                                    color: Color(0xFFA9B3BC),
                                  ),
                                ),
                              ],
                            ),
                          ],
                        ),
                        // Status indicator вЂ” special UNLOADING banner
                        if (wifiState.robotState == 'UNLOADING')
                          Container(
                            padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
                            decoration: BoxDecoration(
                              color: Colors.orange.withValues(alpha: 0.2),
                              borderRadius: BorderRadius.circular(20),
                            ),
                            child: Row(
                              mainAxisSize: MainAxisSize.min,
                              children: [
                                const Icon(Icons.shopping_basket, size: 18, color: Colors.orange),
                                const SizedBox(width: 6),
                                Text(
                                  _tr('РЎРЅРёРјРёС‚Рµ РєРѕСЂР·РёРЅСѓ', 'Remove basket'),
                                  style: const TextStyle(
                                    fontSize: 11,
                                    fontWeight: FontWeight.w600,
                                    color: Colors.orange,
                                  ),
                                ),
                              ],
                            ),
                          )
                        else
                          Container(
                            padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
                            decoration: BoxDecoration(
                              color: isAutoMode ? accent.withValues(alpha: 0.2) : Colors.grey.withValues(alpha: 0.1),
                              borderRadius: BorderRadius.circular(20),
                            ),
                            child: Row(
                              mainAxisSize: MainAxisSize.min,
                              children: [
                                Icon(
                                  isAutoMode ? Icons.play_circle_filled : Icons.pause_circle_filled,
                                  size: 18,
                                  color: isAutoMode ? accent : Colors.grey,
                                ),
                                const SizedBox(width: 6),
                                Text(
                                  wifiState.robotState,
                                  style: TextStyle(
                                    fontSize: 12,
                                    fontWeight: FontWeight.w600,
                                    color: isAutoMode ? accent : Colors.grey,
                                  ),
                                ),
                              ],
                            ),
                          ),
                      ],
                    ),
                  ),
                  const SizedBox(height: 16),
                  // Joystick in manual mode (hide other controls)
                  if (!_autoMode) ...[
                    Row(
                      children: [
                        Expanded(
                          child: ElevatedButton.icon(
                            onPressed: () {
                              wifiConnection.sendStop();
                              if (_rollersEnabled) {
                                wifiConnection.sendAttachment(false);
                              }
                              setState(() {
                                _autoMode = true;
                                _rollersEnabled = false;
                              });
                            },
                            icon: const Icon(Icons.auto_mode, size: 18),
                            label: Text(_tr('Авто', 'Auto')),
                            style: ElevatedButton.styleFrom(
                              backgroundColor: accent,
                              foregroundColor: Colors.black,
                              padding: const EdgeInsets.symmetric(
                                vertical: 12,
                                horizontal: 10,
                              ),
                              shape: RoundedRectangleBorder(
                                borderRadius: BorderRadius.circular(12),
                              ),
                            ),
                          ),
                        ),
                        const SizedBox(width: 12),
                        Expanded(
                          child: _buildRollersButton(
                            wifiConnection,
                            compact: true,
                          ),
                        ),
                      ],
                    ),
                    const SizedBox(height: 16),
                    // Joystick only in manual mode
                    Expanded(
                      child: Center(
                        child: _CoolJoystick(
                          accent: accent,
                          panel: panel,
                          active: _joyActive,
                          offset: _joyOffset,
                          onStart: () => setState(() => _joyActive = true),
                          onChanged: (o) {
                            setState(() => _joyOffset = o);
                            if (wifiState.isConnected) {
                              maybeStop(o);
                            }
                          },
                          onEnd: () => setState(() {
                            _joyActive = false;
                            _joyOffset = Offset.zero;
                            if (wifiState.isConnected) {
                              wifiConnection.sendStop();
                            }
                          }),
                        ),
                      ),
                    ),
                    const SizedBox(height: 16),
                  ] else ...[
                    // 2Г—2 grid: Р СѓС‡РЅРѕР№ | РЎР±СЂРѕСЃ / Р”РѕСЃС‚Р°РІРёС‚СЊ | РЎС‚Р°СЂС‚
                    Column(
                      children: [
                        Row(
                          children: [
                            // Р СѓС‡РЅРѕР№
                            Expanded(
                              child: ElevatedButton(
                                onPressed: () => setState(() => _autoMode = false),
                                style: ElevatedButton.styleFrom(
                                  backgroundColor: panel,
                                  foregroundColor: Colors.white,
                                  padding: const EdgeInsets.symmetric(vertical: 14),
                                  shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
                                ),
                                child: Column(children: [
                                  const Icon(Icons.touch_app, size: 20),
                                  const SizedBox(height: 3),
                                  Text(_tr('Р СѓС‡РЅРѕР№', 'Manual'),
                                      style: const TextStyle(fontWeight: FontWeight.w600, fontSize: 12)),
                                ]),
                              ),
                            ),
                            const SizedBox(width: 10),
                            // РЎР±СЂРѕСЃ
                            Expanded(
                              child: ElevatedButton(
                                onPressed: () {
                                  wifiConnection.sendRaw('RESET');
                                  setState(() {
                                    _mapOnlyMode = false;
                                    _calibrationCompleted = false;
                                    _showCalibrationMode = false;
                                    _showDeliveryPointMode = false;
                                    _deliveryPointSkipped = false;
                                    _ballCount = 0;
                                    _selectedZoneIds.clear();
                                    _deliveryPoint = const Offset(0.5, 0.95);
                                    _robot01 = const Offset(0.90, 0.92);
                                    _robotAngle = -math.pi / 2;
                                    _calibrationCompleted = false;
                                  });
                                },
                                style: ElevatedButton.styleFrom(
                                  backgroundColor: panel,
                                  foregroundColor: Colors.white,
                                  padding: const EdgeInsets.symmetric(vertical: 14),
                                  shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
                                ),
                                child: Column(children: [
                                  const Icon(Icons.refresh, size: 20),
                                  const SizedBox(height: 3),
                                  Text(_tr('РЎР±СЂРѕСЃ', 'Reset'),
                                      style: const TextStyle(fontWeight: FontWeight.w600, fontSize: 12)),
                                ]),
                              ),
                            ),
                          ],
                        ),
                        const SizedBox(height: 10),
                        Row(
                          children: [
                            // Р”РѕСЃС‚Р°РІРёС‚СЊ СЃРµР№С‡Р°СЃ
                            Expanded(
                              child: ElevatedButton(
                                onPressed: wifiState.isConnected
                                    ? () => wifiConnection.sendRaw('DELIVER_NOW')
                                    : null,
                                style: ElevatedButton.styleFrom(
                                  backgroundColor: const Color(0xFF2196F3),
                                  foregroundColor: Colors.white,
                                  disabledBackgroundColor: Colors.grey[800],
                                  padding: const EdgeInsets.symmetric(vertical: 14),
                                  shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
                                ),
                                child: Column(children: [
                                  const Icon(Icons.home, size: 20),
                                  const SizedBox(height: 3),
                                  Text(_tr('Р”РѕСЃС‚Р°РІРёС‚СЊ', 'Deliver'),
                                      style: const TextStyle(fontWeight: FontWeight.w600, fontSize: 12)),
                                ]),
                              ),
                            ),
                            const SizedBox(width: 10),
                            // РЎС‚Р°СЂС‚ / РЎС‚РѕРї
                            Expanded(
                              child: ElevatedButton(
                                onPressed: () {
                                  if (isAutoMode) {
                                    wifiConnection.sendRaw('AUTO_STOP');
                                  } else {
                                    final centers = _selectedZoneIds
                                        .where(_zoneCenters.containsKey)
                                        .map((id) => _zoneCenters[id]!)
                                        .toList();
                                    wifiConnection.sendZones(centers);
                                    wifiConnection.sendRaw('AUTO_START');
                                  }
                                  setState(() {});
                                },
                                style: ElevatedButton.styleFrom(
                                  backgroundColor: isAutoMode ? const Color(0xFFFF6B6B) : accent,
                                  foregroundColor: Colors.black,
                                  padding: const EdgeInsets.symmetric(vertical: 14),
                                  shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
                                ),
                                child: Column(children: [
                                  Icon(isAutoMode ? Icons.stop : Icons.play_arrow, size: 20),
                                  const SizedBox(height: 3),
                                  Text(
                                    isAutoMode ? _tr('РЎС‚РѕРї', 'Stop') : _tr('РЎС‚Р°СЂС‚', 'Start'),
                                    style: const TextStyle(fontWeight: FontWeight.w700, fontSize: 12),
                                  ),
                                ]),
                              ),
                            ),
                          ],
                        ),
                      ],
                    ),
                    const Spacer(),
                  ],
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildBottomJoystickDock() {
    final wifiState = ref.watch(wifiConnectionProvider);
    final wifiConnection = ref.read(wifiConnectionProvider.notifier);

    // Hide joystick when not connected
    if (!wifiState.isConnected) {
      return const SizedBox.shrink();
    }

    const maxRadius = _CoolJoystick._radius;
    // Speed mode multiplier: 0=slow(~50%), 1=medium(~75%), 2=fast(100%)
    final speedMultiplier = switch (_speedMode) {
      0 => 0.5,
      1 => 0.75,
      2 => 1.0,
      _ => 0.75,
    };

    void sendFromOffset(Offset o) {
      final nx = (o.dx / maxRadius).clamp(-1.0, 1.0);
      final ny = (o.dy / maxRadius).clamp(-1.0, 1.0);
      final forward = -ny;
      final turn = nx;
      final left = ((forward + turn) * 100 * speedMultiplier).round();
      final right = ((forward - turn) * 100 * speedMultiplier).round();
      wifiConnection.sendMove(left, right);
    }

    void maybeStop(Offset o) {
      final nx = (o.dx / maxRadius).abs();
      final ny = (o.dy / maxRadius).abs();
      if (nx < 0.05 && ny < 0.05) {
        wifiConnection.sendStop();
      } else {
        sendFromOffset(o);
      }
    }

    return SafeArea(
      top: false,
      child: Container(
        padding: const EdgeInsets.fromLTRB(16, 10, 16, 14),
        decoration: BoxDecoration(
          color: panel,
          border: Border(top: BorderSide(color: Colors.white.withValues(alpha: 0.08))),
        ),
        child: Row(
          children: [
            Expanded(
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  _buildRollersButton(wifiConnection),
                  const SizedBox(height: 14),
                  Center(
                    child: _CoolJoystick(
                      accent: accent,
                      panel: panel,
                      active: _joyActive,
                      offset: _joyOffset,
                      onStart: () => setState(() => _joyActive = true),
                      onChanged: (o) {
                        setState(() => _joyOffset = o);
                        maybeStop(o);
                      },
                      onEnd: () => setState(() {
                        _joyActive = false;
                        _joyOffset = Offset.zero;
                        wifiConnection.sendStop();
                      }),
                    ),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  // Visual-only joystick
  // (does not control robot; safe to keep map gestures unchanged)
  Widget _buildSettingsTab() {
    final wifiState = ref.watch(wifiConnectionProvider);
    final wifiConnection = ref.read(wifiConnectionProvider.notifier);
    final displayBallsCount = wifiState.isConnected ? wifiState.ballsCount : _ballCount;
    final displayMaxBalls = wifiState.isConnected ? wifiState.maxBalls : _ballMaxCapacity;
    final ballProgress = displayMaxBalls > 0 ? displayBallsCount / displayMaxBalls : 0.0;
    final batteryColor = _batteryLevel > 20 ? accent : const Color(0xFFFF6B6B);

    return SingleChildScrollView(
      padding: const EdgeInsets.fromLTRB(14, 12, 14, 14),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          _buildSectionCard(
            child: Column(
              children: [
                Row(
                  children: [
                    _buildStatusChip(
                      icon: _isCharging ? Icons.battery_charging_full : Icons.battery_std,
                      label: '$_batteryLevel%',
                      color: batteryColor,
                    ),
                    const SizedBox(width: 8),
                    _buildStatusChip(
                      icon: wifiState.isConnected ? Icons.wifi : Icons.wifi_off,
                      label: wifiState.isConnected ? _tr('РЎРІСЏР·СЊ', 'Link') : _tr('РќРµС‚', 'No'),
                      color: wifiState.isConnected ? accent : const Color(0xFFFF6B6B),
                    ),
                    const Spacer(),
                    // Connect/Disconnect buttons
                    if (wifiState.isConnecting)
                      const SizedBox(
                        width: 20,
                        height: 20,
                        child: CircularProgressIndicator(strokeWidth: 2, color: accent),
                      )
                    else if (!wifiState.isConnected)
                      ElevatedButton(
                        onPressed: () => wifiConnection.connect(),
                        style: ElevatedButton.styleFrom(
                          backgroundColor: accent,
                          foregroundColor: Colors.black,
                          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
                          shape: RoundedRectangleBorder(
                            borderRadius: BorderRadius.circular(8),
                          ),
                          elevation: 0,
                        ),
                        child: Text(_tr('РџРѕРґРєР»СЋС‡РёС‚СЊ', 'Connect'), style: const TextStyle(fontWeight: FontWeight.w600, fontSize: 12)),
                      )
                    else
                      ElevatedButton(
                        onPressed: () => wifiConnection.disconnect(),
                        style: ElevatedButton.styleFrom(
                          backgroundColor: panel,
                          foregroundColor: Colors.white,
                          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
                          shape: RoundedRectangleBorder(
                            borderRadius: BorderRadius.circular(8),
                          ),
                          elevation: 0,
                        ),
                        child: Text(_tr('РћС‚РєР»СЋС‡РёС‚СЊ', 'Disconnect'), style: const TextStyle(fontWeight: FontWeight.w600, fontSize: 12)),
                      ),
                  ],
                ),
              ],
            ),
          ),
          const SizedBox(height: 12),

          _buildSectionTitle(_tr('РЎС‡С‘С‚С‡РёРє РјСЏС‡РµР№', 'Ball Counter')),
          const SizedBox(height: 8),
          _buildSectionCard(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    Text(
                      '$displayBallsCount',
                      style: const TextStyle(fontSize: 48, fontWeight: FontWeight.w700, color: accent),
                    ),
                    Column(
                      crossAxisAlignment: CrossAxisAlignment.end,
                      children: [
                        Text(
                          _tr('РњР°РєСЃРёРјСѓРј', 'Max'),
                          style: const TextStyle(fontSize: 12, color: Color(0xFFA9B3BC)),
                        ),
                        Text(
                          '$displayMaxBalls',
                          style: const TextStyle(fontSize: 24, fontWeight: FontWeight.w600),
                        ),
                      ],
                    ),
                  ],
                ),
                const SizedBox(height: 12),
                ClipRRect(
                  borderRadius: BorderRadius.circular(8),
                  child: LinearProgressIndicator(
                    value: ballProgress.clamp(0.0, 1.0),
                    backgroundColor: Colors.white.withValues(alpha: 0.1),
                    valueColor: AlwaysStoppedAnimation<Color>(
                      ballProgress > 0.9 ? const Color(0xFFFF6B6B) : accent,
                    ),
                    minHeight: 8,
                  ),
                ),
                const SizedBox(height: 8),
                // MAX BALLS slider
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    Text(
                      _tr('РњСЏС‡РµР№ РјР°РєСЃ:', 'Max balls:'),
                      style: const TextStyle(fontSize: 13, color: Color(0xFFA9B3BC)),
                    ),
                    Text(
                      '$displayMaxBalls',
                      style: TextStyle(
                        fontSize: 14,
                        fontWeight: FontWeight.w700,
                        color: accent,
                      ),
                    ),
                  ],
                ),
                SliderTheme(
                  data: SliderThemeData(
                    activeTrackColor: accent,
                    inactiveTrackColor: accent.withValues(alpha: 0.2),
                    thumbColor: accent,
                    overlayColor: accent.withValues(alpha: 0.15),
                    trackHeight: 4,
                  ),
                  child: Slider(
                    value: displayMaxBalls.toDouble().clamp(1, 60),
                    min: 1,
                    max: 60,
                    onChanged: (v) => setState(() => _ballMaxCapacity = v.round()),
                    onChangeEnd: (v) {
                      final n = v.round();
                      setState(() => _ballMaxCapacity = n);
                      wifiConnection.sendSetBalls(n);
                    },
                  ),
                ),
              ],
            ),
          ),
          const SizedBox(height: 12),

          _buildSectionTitle(_tr('РЎРєРѕСЂРѕСЃС‚СЊ', 'Speed')),
          const SizedBox(height: 8),
          _buildSectionCard(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(
                  children: List.generate(3, (i) {
                    final isSelected = _speedMode == i;
                    return Expanded(
                      child: GestureDetector(
                        onTap: () {
                          if (i == 2 && _speedMode != 2) {
                            // Show warning for fast mode
                            showModalBottomSheet(
                              context: context,
                              backgroundColor: Colors.transparent,
                              isScrollControlled: true,
                              builder: (context) => Align(
                                alignment: Alignment.center,
                                child: Container(
                                  margin: const EdgeInsets.all(12),
                                  decoration: BoxDecoration(
                                    color: const Color(0xFF1A2229),
                                    borderRadius: BorderRadius.circular(24),
                                    border: Border.all(color: Colors.white.withValues(alpha: 0.08)),
                                  ),
                                  child: Column(
                                    mainAxisSize: MainAxisSize.min,
                                    children: [
                                      // Handle
                                      Container(
                                        margin: const EdgeInsets.only(top: 12, bottom: 8),
                                        width: 40,
                                        height: 4,
                                        decoration: BoxDecoration(
                                          color: Colors.white.withValues(alpha: 0.2),
                                          borderRadius: BorderRadius.circular(2),
                                        ),
                                      ),
                                      // Title
                                      Padding(
                                        padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 12),
                                        child: Row(
                                          children: [
                                            Container(
                                              padding: const EdgeInsets.all(8),
                                              decoration: BoxDecoration(
                                                color: const Color(0xFFFF6B6B).withValues(alpha: 0.15),
                                                borderRadius: BorderRadius.circular(10),
                                              ),
                                              child: const Icon(Icons.warning_amber, color: Color(0xFFFF6B6B), size: 20),
                                            ),
                                            const SizedBox(width: 12),
                                            Text(
                                              _tr('Р’РЅРёРјР°РЅРёРµ', 'Warning'),
                                              style: const TextStyle(
                                                color: Colors.white,
                                                fontWeight: FontWeight.w700,
                                                fontSize: 18,
                                              ),
                                            ),
                                          ],
                                        ),
                                      ),
                                      // Content
                                      Padding(
                                        padding: const EdgeInsets.symmetric(horizontal: 20),
                                        child: Text(
                                          _tr(
                                            'Р’ Р±С‹СЃС‚СЂРѕРј СЂРµР¶РёРјРµ СЂРѕР±РѕС‚ РјРѕР¶РµС‚ СЂР°Р±РѕС‚Р°С‚СЊ РјРµРЅРµРµ СЃС‚Р°Р±РёР»СЊРЅРѕ. РџСЂРѕРґРѕР»Р¶РёС‚СЊ?',
                                            'In fast mode, the robot may work less stable. Continue?',
                                          ),
                                          style: const TextStyle(fontSize: 14, color: Color(0xFFA9B3BC)),
                                        ),
                                      ),
                                      const SizedBox(height: 16),
                                      // Buttons
                                      Padding(
                                        padding: const EdgeInsets.fromLTRB(20, 0, 20, 20),
                                        child: Row(
                                          children: [
                                            Expanded(
                                              child: ElevatedButton(
                                                onPressed: () => Navigator.of(context).pop(),
                                                style: ElevatedButton.styleFrom(
                                                  backgroundColor: panel,
                                                  foregroundColor: Colors.white,
                                                  padding: const EdgeInsets.symmetric(vertical: 14),
                                                  shape: RoundedRectangleBorder(
                                                    borderRadius: BorderRadius.circular(12),
                                                  ),
                                                  elevation: 0,
                                                ),
                                                child: Text(_tr('РћС‚РјРµРЅР°', 'Cancel'), style: const TextStyle(fontWeight: FontWeight.w700, fontSize: 15)),
                                              ),
                                            ),
                                            const SizedBox(width: 12),
                                            Expanded(
                                              child: ElevatedButton(
                                                onPressed: () {
                                                  setState(() => _speedMode = 2);
                                                  wifiConnection.sendRaw('SET_SPEED:2');
                                                  Navigator.of(context).pop();
                                                },
                                                style: ElevatedButton.styleFrom(
                                                  backgroundColor: accent,
                                                  foregroundColor: Colors.black,
                                                  padding: const EdgeInsets.symmetric(vertical: 14),
                                                  shape: RoundedRectangleBorder(
                                                    borderRadius: BorderRadius.circular(12),
                                                  ),
                                                  elevation: 0,
                                                ),
                                                child: Text(_tr('РџСЂРѕРґРѕР»Р¶РёС‚СЊ', 'Continue'), style: const TextStyle(fontWeight: FontWeight.w700, fontSize: 15)),
                                              ),
                                            ),
                                          ],
                                        ),
                                      ),
                                    ],
                                  ),
                                ),
                              ),
                            );
                          } else {
                            setState(() => _speedMode = i);
                            wifiConnection.sendRaw('SET_SPEED:$i');
                          }
                        },
                        child: Container(
                          margin: EdgeInsets.only(left: i == 0 ? 0 : 6),
                          padding: const EdgeInsets.symmetric(vertical: 14),
                          decoration: BoxDecoration(
                            color: isSelected ? accent.withValues(alpha: 0.15) : Colors.transparent,
                            borderRadius: BorderRadius.circular(10),
                            border: Border.all(
                              color: isSelected ? accent : Colors.white.withValues(alpha: 0.1),
                              width: isSelected ? 1.5 : 1,
                            ),
                          ),
                          child: Column(
                            children: [
                              Icon(
                                _speedIcon(i),
                                size: 22,
                                color: isSelected ? accent : const Color(0xFFA9B3BC),
                              ),
                              const SizedBox(height: 4),
                              Text(
                                _speedLabel(i),
                                style: TextStyle(
                                  fontSize: 11,
                                  fontWeight: FontWeight.w600,
                                  color: isSelected ? accent : const Color(0xFFA9B3BC),
                                ),
                              ),
                            ],
                          ),
                        ),
                      ),
                    );
                  }),
                ),
              ],
            ),
          ),
          const SizedBox(height: 12),

          // Device names section
          _buildSectionTitle(_tr('РЈСЃС‚СЂРѕР№СЃС‚РІР°', 'Devices')),
          const SizedBox(height: 8),
          
          // Robot name
          _buildSectionCard(
            child: Row(
              children: [
                Container(
                  padding: const EdgeInsets.all(10),
                  decoration: BoxDecoration(
                    color: accent.withValues(alpha: 0.15),
                    borderRadius: BorderRadius.circular(10),
                  ),
                  child: const Icon(Icons.smart_toy, color: accent, size: 20),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        _tr('Р РѕР±РѕС‚', 'Robot'),
                        style: const TextStyle(fontSize: 11, color: Color(0xFFA9B3BC)),
                      ),
                      Text(
                        _robotName,
                        style: const TextStyle(fontSize: 16, fontWeight: FontWeight.w600),
                      ),
                    ],
                  ),
                ),
                TextButton(
                  onPressed: () => _showEditNameDialog(
                    title: _tr('РРјСЏ СЂРѕР±РѕС‚Р°', 'Robot name'),
                    initialValue: _robotName,
                    onSave: (v) => setState(() => _robotName = v),
                  ),
                  child: Text(_tr('РР·РјРµРЅРёС‚СЊ', 'Edit'), style: TextStyle(color: accent, fontWeight: FontWeight.w600)),
                ),
              ],
            ),
          ),
          const SizedBox(height: 8),

          // Beacon L name
          _buildSectionCard(
            child: Row(
              children: [
                Container(
                  padding: const EdgeInsets.all(10),
                  decoration: BoxDecoration(
                    color: accent.withValues(alpha: 0.15),
                    borderRadius: BorderRadius.circular(10),
                  ),
                  child: const Icon(Icons.sensors, color: accent, size: 20),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        _tr('РњР°СЏРє L', 'Beacon L'),
                        style: const TextStyle(fontSize: 11, color: Color(0xFFA9B3BC)),
                      ),
                      Text(
                        _beaconLName,
                        style: const TextStyle(fontSize: 16, fontWeight: FontWeight.w600),
                      ),
                    ],
                  ),
                ),
                TextButton(
                  onPressed: () => _showEditNameDialog(
                    title: _tr('РРјСЏ РјР°СЏРєР° L', 'Beacon L name'),
                    initialValue: _beaconLName,
                    onSave: (v) => setState(() => _beaconLName = v),
                  ),
                  child: Text(_tr('РР·РјРµРЅРёС‚СЊ', 'Edit'), style: TextStyle(color: accent, fontWeight: FontWeight.w600)),
                ),
              ],
            ),
          ),
          const SizedBox(height: 8),

          // Beacon R name
          _buildSectionCard(
            child: Row(
              children: [
                Container(
                  padding: const EdgeInsets.all(10),
                  decoration: BoxDecoration(
                    color: accent.withValues(alpha: 0.15),
                    borderRadius: BorderRadius.circular(10),
                  ),
                  child: const Icon(Icons.sensors, color: accent, size: 20),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        _tr('РњР°СЏРє R', 'Beacon R'),
                        style: const TextStyle(fontSize: 11, color: Color(0xFFA9B3BC)),
                      ),
                      Text(
                        _beaconRName,
                        style: const TextStyle(fontSize: 16, fontWeight: FontWeight.w600),
                      ),
                    ],
                  ),
                ),
                TextButton(
                  onPressed: () => _showEditNameDialog(
                    title: _tr('РРјСЏ РјР°СЏРєР° R', 'Beacon R name'),
                    initialValue: _beaconRName,
                    onSave: (v) => setState(() => _beaconRName = v),
                  ),
                  child: Text(_tr('РР·РјРµРЅРёС‚СЊ', 'Edit'), style: TextStyle(color: accent, fontWeight: FontWeight.w600)),
                ),
              ],
            ),
          ),
          const SizedBox(height: 12),

          _buildSectionTitle(_tr('РЇР·С‹Рє', 'Language')),
          const SizedBox(height: 8),
          _buildSectionCard(
            child: GestureDetector(
              onTap: () => setState(() => _lang = _lang == AppLang.ru ? AppLang.en : AppLang.ru),
              child: Row(
                children: [
                  Container(
                    padding: const EdgeInsets.all(10),
                    decoration: BoxDecoration(
                      color: accent.withValues(alpha: 0.15),
                      borderRadius: BorderRadius.circular(10),
                    ),
                    child: Icon(Icons.language, color: accent, size: 22),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Text(
                      _lang == AppLang.ru ? 'Р СѓСЃСЃРєРёР№' : 'English',
                      style: const TextStyle(fontSize: 16, fontWeight: FontWeight.w600),
                    ),
                  ),
                  Container(
                    padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
                    decoration: BoxDecoration(
                      color: panel,
                      borderRadius: BorderRadius.circular(8),
                      border: Border.all(color: Colors.white.withValues(alpha: 0.1)),
                    ),
                    child: Text(
                      _lang == AppLang.ru ? 'RU в†’ EN' : 'EN в†’ RU',
                      style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w600, color: Color(0xFFA9B3BC)),
                    ),
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(height: 12),

          // Developer mode вЂ” hidden, activated by tapping version label 5 times
          GestureDetector(
            onTap: () {
              final now = DateTime.now();
              if (_lastDevTap == null || now.difference(_lastDevTap!) > const Duration(seconds: 3)) {
                setState(() { _devTapCount = 1; _lastDevTap = now; });
              } else {
                setState(() { _devTapCount++; _lastDevTap = now; });
                if (_devTapCount >= 5) {
                  setState(() { _devTapCount = 0; _devModeEnabled = true; });
                }
              }
            },
            child: Container(
              alignment: Alignment.center,
              padding: const EdgeInsets.symmetric(vertical: 6),
              child: Text(
                'v1.0.0',
                style: TextStyle(
                  fontSize: 11,
                  color: Colors.white.withValues(alpha: 0.18),
                  letterSpacing: 1.2,
                ),
              ),
            ),
          ),

          if (_devModeEnabled) ...[
            const SizedBox(height: 8),
            _buildDevModePanel(),
          ],
          const SizedBox(height: 20),
        ],
      ),
    );
  }

  Widget _buildSectionCard({required Widget child}) {
    return Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: panel,
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: Colors.white.withValues(alpha: 0.08)),
      ),
      child: child,
    );
  }

  Widget _buildSectionTitle(String title) {
    return Text(
      title,
      style: const TextStyle(fontSize: 13, fontWeight: FontWeight.w600, color: Color(0xFFA9B3BC)),
    );
  }

  Widget _buildDevModePanel() {
    final wifiState = ref.watch(wifiConnectionProvider);
    final wifiConnection = ref.read(wifiConnectionProvider.notifier);

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        // Header
        Row(
          children: [
            Icon(Icons.developer_mode, color: accent, size: 16),
            const SizedBox(width: 6),
            Text(
              'DEV MODE',
              style: TextStyle(fontSize: 13, fontWeight: FontWeight.w800, color: accent, letterSpacing: 1.5),
            ),
            const Spacer(),
            TextButton(
              onPressed: () => setState(() => _devModeEnabled = false),
              style: TextButton.styleFrom(padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4), minimumSize: Size.zero),
              child: Text('СЃРєСЂС‹С‚СЊ', style: TextStyle(color: Colors.white.withValues(alpha: 0.35), fontSize: 11)),
            ),
          ],
        ),
        const SizedBox(height: 8),

        // Telemetry card
        _buildSectionCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('РўР•Р›Р•РњР•РўР РРЇ', style: TextStyle(fontSize: 10, fontWeight: FontWeight.w700, color: accent.withValues(alpha: 0.7), letterSpacing: 1.2)),
              const SizedBox(height: 10),
              _devRow('РЎС‚Р°С‚СѓСЃ', wifiState.isConnected ? 'ONLINE' : 'OFFLINE',
                  wifiState.isConnected ? accent : const Color(0xFFFF6B6B)),
              _devRow('РЎРѕСЃС‚РѕСЏРЅРёРµ', wifiState.robotState, Colors.white),
              _devRow('X (РґР»РёРЅР°)', '${wifiState.robotX.toStringAsFixed(3)} Рј', Colors.white),
              _devRow('Y (С€РёСЂРёРЅР°)', '${wifiState.robotY.toStringAsFixed(3)} Рј', Colors.white),
              _devRow('РњР°СЏРє L', wifiState.anchorLOnline ? 'online' : 'offline',
                  wifiState.anchorLOnline ? accent : const Color(0xFFFF6B6B)),
              if (wifiState.rangeL > 0)
                _devRow('Р”Р°Р»СЊРЅРѕСЃС‚СЊ L', '${wifiState.rangeL.toStringAsFixed(3)} Рј', const Color(0xFF88CCFF)),
              _devRow('РњР°СЏРє R', wifiState.anchorROnline ? 'online' : 'offline',
                  wifiState.anchorROnline ? Colors.green : const Color(0xFFFF6B6B)),
              if (wifiState.rangeR > 0)
                _devRow('Р”Р°Р»СЊРЅРѕСЃС‚СЊ R', '${wifiState.rangeR.toStringAsFixed(3)} Рј', const Color(0xFF88CCFF)),
              _devRow('РњСЏС‡Рё', '${wifiState.ballsCount} / ${wifiState.maxBalls}', Colors.white),
              if (wifiState.batteryPercent != null)
                _devRow('Р‘Р°С‚Р°СЂРµСЏ', '${wifiState.batteryPercent}%', Colors.white),
            ],
          ),
        ),
        const SizedBox(height: 8),

        // Toggles card
        _buildSectionCard(
          child: Column(
            children: [
              SwitchListTile(
                value: _showCoordinates,
                onChanged: (v) => setState(() => _showCoordinates = v),
                title: const Text('РљРѕРѕСЂРґРёРЅР°С‚С‹ РЅР° РєР°СЂС‚Рµ', style: TextStyle(fontSize: 13)),
                activeThumbColor: accent,
                contentPadding: EdgeInsets.zero,
                dense: true,
              ),
              SwitchListTile(
                value: _showTrail,
                onChanged: (v) => setState(() => _showTrail = v),
                title: const Text('РЎР»РµРґ РїРѕР·РёС†РёРё', style: TextStyle(fontSize: 13)),
                activeThumbColor: accent,
                contentPadding: EdgeInsets.zero,
                dense: true,
              ),
              SwitchListTile(
                value: ref.watch(wifiPingCheckProvider),
                onChanged: (v) => ref.read(wifiPingCheckProvider.notifier).setEnabled(v),
                title: const Text('РџСЂРѕРІРµСЂРєР° Wi-Fi РїРµСЂРµРґ РїРѕРґРєР»СЋС‡РµРЅРёРµРј', style: TextStyle(fontSize: 13)),
                activeThumbColor: accent,
                contentPadding: EdgeInsets.zero,
                dense: true,
              ),
            ],
          ),
        ),
        const SizedBox(height: 8),

        // Manual command card
        _buildSectionCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('РљРћРњРђРќР”Рђ', style: TextStyle(fontSize: 10, fontWeight: FontWeight.w700, color: accent.withValues(alpha: 0.7), letterSpacing: 1.2)),
              const SizedBox(height: 8),
              Row(
                children: [
                  Expanded(
                    child: TextField(
                      controller: _devCmdController,
                      style: const TextStyle(color: Colors.white, fontSize: 13, fontFamily: 'monospace'),
                      decoration: InputDecoration(
                        hintText: 'AUTO_START, M,50,50, ...',
                        hintStyle: TextStyle(color: Colors.white.withValues(alpha: 0.25), fontSize: 12),
                        enabledBorder: OutlineInputBorder(
                          borderSide: BorderSide(color: Colors.white.withValues(alpha: 0.1)),
                          borderRadius: BorderRadius.circular(8),
                        ),
                        focusedBorder: OutlineInputBorder(
                          borderSide: BorderSide(color: accent.withValues(alpha: 0.6)),
                          borderRadius: BorderRadius.circular(8),
                        ),
                        contentPadding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
                        isDense: true,
                      ),
                      onSubmitted: (v) {
                        if (v.trim().isNotEmpty) {
                          wifiConnection.sendRaw(v.trim());
                          _devCmdController.clear();
                        }
                      },
                    ),
                  ),
                  const SizedBox(width: 8),
                  ElevatedButton(
                    onPressed: () {
                      final v = _devCmdController.text.trim();
                      if (v.isNotEmpty) {
                        wifiConnection.sendRaw(v);
                        _devCmdController.clear();
                      }
                    },
                    style: ElevatedButton.styleFrom(
                      backgroundColor: accent,
                      foregroundColor: Colors.black,
                      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
                      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
                      elevation: 0,
                    ),
                    child: const Text('TX', style: TextStyle(fontWeight: FontWeight.w800, fontSize: 13)),
                  ),
                ],
              ),
            ],
          ),
        ),
        const SizedBox(height: 8),

        // Terminal card
        _buildSectionCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                children: [
                  Text('РўР•Р РњРРќРђР›', style: TextStyle(fontSize: 10, fontWeight: FontWeight.w700, color: accent.withValues(alpha: 0.7), letterSpacing: 1.2)),
                  const Spacer(),
                  TextButton(
                    onPressed: () => ref.read(terminalProvider.notifier).clear(),
                    style: TextButton.styleFrom(padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 2), minimumSize: Size.zero),
                    child: Text('РѕС‡РёСЃС‚РёС‚СЊ', style: TextStyle(color: accent.withValues(alpha: 0.7), fontSize: 11)),
                  ),
                ],
              ),
              const SizedBox(height: 6),
              Container(
                height: 200,
                padding: const EdgeInsets.all(8),
                decoration: BoxDecoration(
                  color: bg,
                  borderRadius: BorderRadius.circular(8),
                ),
                child: SingleChildScrollView(
                  reverse: true,
                  child: Consumer(
                    builder: (context, ref, _) {
                      final terminal = ref.watch(terminalProvider);
                      return Text(
                        terminal.lines.join('\n'),
                        style: const TextStyle(fontSize: 10, fontFamily: 'monospace', color: Color(0xFFA9B3BC)),
                      );
                    },
                  ),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 8),

        // Quick actions card
        _buildSectionCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('Р‘Р«РЎРўР Р«Р• РљРћРњРђРќР”Р«', style: TextStyle(fontSize: 10, fontWeight: FontWeight.w700, color: accent.withValues(alpha: 0.7), letterSpacing: 1.2)),
              const SizedBox(height: 10),
              Wrap(
                spacing: 8,
                runSpacing: 8,
                children: [
                  _devBtn('Р РµРєРѕРЅРЅРµРєС‚', () { wifiConnection.disconnect(); Future.delayed(const Duration(milliseconds: 500), wifiConnection.connect); }),
                  _devBtn('AUTO_START', () => wifiConnection.sendRaw('AUTO_START')),
                  _devBtn('AUTO_STOP', () => wifiConnection.sendRaw('AUTO_STOP')),
                  _devBtn('CALIBRATE (РєР°СЂС‚Р°)', _sendCalibration),
                  _devBtn('CAL_RAW РІРєР»', () => wifiConnection.sendRaw('CAL_RAW')),
                  _devBtn('CAL_RAW РІС‹РєР»', () => wifiConnection.sendRaw('CAL_STOP')),
                  _devBtn('RESET', () => wifiConnection.sendRaw('RESET')),
                  _devBtn('ATTACH ON', () => wifiConnection.sendAttachment(true)),
                  _devBtn('ATTACH OFF', () => wifiConnection.sendAttachment(false)),
                  _devBtn('STOP', () => wifiConnection.sendStop()),
                ],
              ),
            ],
          ),
        ),
      ],
    );
  }

  Widget _devRow(String label, String value, Color valueColor) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 3),
      child: Row(
        children: [
          Text(label, style: TextStyle(fontSize: 12, color: Colors.white.withValues(alpha: 0.5))),
          const Spacer(),
          Text(value, style: TextStyle(fontSize: 12, fontWeight: FontWeight.w600, color: valueColor, fontFamily: 'monospace')),
        ],
      ),
    );
  }

  Widget _devBtn(String label, VoidCallback onTap) {
    return GestureDetector(
      onTap: onTap,
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 7),
        decoration: BoxDecoration(
          color: accent.withValues(alpha: 0.12),
          borderRadius: BorderRadius.circular(8),
          border: Border.all(color: accent.withValues(alpha: 0.3)),
        ),
        child: Text(label, style: TextStyle(fontSize: 12, fontWeight: FontWeight.w600, color: accent)),
      ),
    );
  }

  Widget _buildStatusChip({required IconData icon, required String label, required Color color}) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.1),
        borderRadius: BorderRadius.circular(20),
        border: Border.all(color: color.withValues(alpha: 0.3)),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(icon, size: 14, color: color),
          const SizedBox(width: 4),
          Text(
            label,
            style: TextStyle(fontSize: 11, fontWeight: FontWeight.w600, color: color),
          ),
        ],
      ),
    );
  }

  Rect _playableRect(Size size) {
    const pad = 42.0;
    final available = Rect.fromLTWH(pad, pad, size.width - 2 * pad, size.height - 2 * pad);
    final verticalRatio = courtW / courtL;

    double w = available.width;
    double h = w / verticalRatio;
    if (h > available.height) {
      h = available.height;
      w = h * verticalRatio;
    }

    final left = available.left + (available.width - w) / 2;
    final top = available.top + (available.height - h) / 2;
    return Rect.fromLTWH(left, top, w, h);
  }

  List<ZonePx> _buildZonesPx(Rect playable) {
    const double x0 = _courtOriginX;
    const double x1 = _courtOriginX + 5.485;
    const double x2 = _courtOriginX + 11.885;
    const double x3 = _courtOriginX + 18.285;
    const double x4 = _courtOriginX + _courtL;
    const double y0 = _courtOriginY;
    const double yC = _courtOriginY + 5.485;
    const double y4 = _courtOriginY + _courtW;

    double sx(double workY) => playable.left + (workY / _workW) * playable.width;
    double sy(double workX) => playable.top + (workX / _workL) * playable.height;

    return [
      ZonePx(id: 'BACK_A', name: 'Back A',
        rect: Rect.fromLTRB(sx(y0), sy(x0), sx(y4), sy(x1))),
      ZonePx(id: 'SERV_L_A', name: 'Service L A',
        rect: Rect.fromLTRB(sx(y0), sy(x1), sx(yC), sy(x2))),
      ZonePx(id: 'SERV_R_A', name: 'Service R A',
        rect: Rect.fromLTRB(sx(yC), sy(x1), sx(y4), sy(x2))),
      ZonePx(id: 'SERV_L_B', name: 'Service L B',
        rect: Rect.fromLTRB(sx(y0), sy(x2), sx(yC), sy(x3))),
      ZonePx(id: 'SERV_R_B', name: 'Service R B',
        rect: Rect.fromLTRB(sx(yC), sy(x2), sx(y4), sy(x3))),
      ZonePx(id: 'BACK_B', name: 'Back B',
        rect: Rect.fromLTRB(sx(y0), sy(x3), sx(y4), sy(x4))),
      ZonePx(id: 'OUT_BACK_A', name: 'Outside Back A',
        rect: Rect.fromLTRB(sx(0), sy(0), sx(_workW), sy(x0))),
      ZonePx(id: 'OUT_SIDE_L_A', name: 'Outside Left A',
        rect: Rect.fromLTRB(sx(0), sy(x0), sx(y0), sy(x2))),
      ZonePx(id: 'OUT_SIDE_R_A', name: 'Outside Right A',
        rect: Rect.fromLTRB(sx(y4), sy(x0), sx(_workW), sy(x2))),
      ZonePx(id: 'OUT_SIDE_L_B', name: 'Outside Left B',
        rect: Rect.fromLTRB(sx(0), sy(x2), sx(y0), sy(x4))),
      ZonePx(id: 'OUT_SIDE_R_B', name: 'Outside Right B',
        rect: Rect.fromLTRB(sx(y4), sy(x2), sx(_workW), sy(x4))),
      ZonePx(id: 'OUT_BACK_B', name: 'Outside Back B',
        rect: Rect.fromLTRB(sx(0), sy(x4), sx(_workW), sy(_workL))),
    ];
  }

  Offset _robotPx(Rect playable) => Offset(
        playable.left + _robot01.dx * playable.width,
        playable.top + _robot01.dy * playable.height,
      );

  Offset _handlePx(Rect playable) {
    final rp = _robotPx(playable);
    final dirAngle = _robotAngle - math.pi / 2;
    final dir = Offset(math.cos(dirAngle), math.sin(dirAngle));
    return rp + dir * _handleDistance;
  }

  bool _hitRobot(Offset p, Rect playable) => (p - _robotPx(playable)).distance <= _robotRadius + 12;

  bool _hitHandle(Offset p, Rect playable) => (p - _handlePx(playable)).distance <= _handleRadius + 28;

  ZonePx? _hitZone(Offset p, List<ZonePx> zonesPx) {
    for (final z in zonesPx.reversed) {
      if (z.id.startsWith('OUT_')) continue;
      if (z.rect.contains(p)) return z;
    }

    for (final z in zonesPx.reversed) {
      if (z.id.startsWith('OUT_') && z.rect.contains(p)) return z;
    }

    return null;
  }

  void _onPanStart(Offset p, Rect playable) {
    if (_hitHandle(p, playable)) {
      final rp = _robotPx(playable);
      // Calculate the initial angle from robot to finger
      final initialFingerAngle = math.atan2(p.dy - rp.dy, p.dx - rp.dx);
      // Store the offset between current robot angle and finger angle
      // robotAngle is already adjusted by +pi/2, so we compare with that
      _rotateOffset = _robotAngle - initialFingerAngle;
      setState(() => _dragMode = DragMode.rotateRobot);
      return;
    }

    if (_hitRobot(p, playable)) {
      final rp = _robotPx(playable);
      _moveDeltaPx = rp - p;
      setState(() {
        _robotSelected = true;
        _dragMode = DragMode.moveRobot;
      });
      return;
    }

    setState(() => _dragMode = DragMode.none);
  }

  void _onPanUpdate(Offset p, Rect playable) {
    if (_dragMode == DragMode.moveRobot) {
      final np = p + _moveDeltaPx;

      final clamped = Offset(
        np.dx.clamp(playable.left, playable.right).toDouble(),
        np.dy.clamp(playable.top, playable.bottom).toDouble(),
      );

      final nx01 = (clamped.dx - playable.left) / playable.width;
      final ny01 = (clamped.dy - playable.top) / playable.height;

      setState(() {
        _robot01 = Offset(
          nx01.clamp(0.02, 0.98).toDouble(),
          ny01.clamp(0.02, 0.98).toDouble(),
        );
      });

      return;
    }

    if (_dragMode == DragMode.rotateRobot) {
      final rp = _robotPx(playable);
      final fingerAngle = math.atan2(p.dy - rp.dy, p.dx - rp.dx);
      // Apply the stored offset for smooth rotation
      setState(() => _robotAngle = fingerAngle + _rotateOffset);
      return;
    }
  }
}

class _CoolJoystick extends StatelessWidget {
  const _CoolJoystick({
    required this.accent,
    required this.panel,
    required this.active,
    required this.offset,
    required this.onStart,
    required this.onChanged,
    required this.onEnd,
  });

  final Color accent;
  final Color panel;
  final bool active;
  final Offset offset;
  final VoidCallback onStart;
  final ValueChanged<Offset> onChanged;
  final VoidCallback onEnd;

  static const double _size = 132;
  static const double _radius = 54;

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onPanStart: (_) => onStart(),
      onPanUpdate: (d) {
        final local = d.localPosition;
        final center = const Offset(_size / 2, _size / 2);
        final v = local - center;
        final dist = v.distance;
        final clamped = dist <= _radius ? v : (v / dist) * _radius;
        onChanged(clamped);
      },
      onPanEnd: (_) => onEnd(),
      onPanCancel: () => onEnd(),
      child: SizedBox(
        width: _size,
        height: _size,
        child: Stack(
          alignment: Alignment.center,
          children: [
            AnimatedOpacity(
              opacity: active ? 1 : 0.35,
              duration: const Duration(milliseconds: 140),
              child: Container(
                width: _size,
                height: _size,
                decoration: BoxDecoration(
                  shape: BoxShape.circle,
                  boxShadow: [
                    BoxShadow(
                      color: accent.withValues(alpha: active ? 0.22 : 0.10),
                      blurRadius: active ? 26 : 16,
                      spreadRadius: active ? 2 : 0,
                    ),
                  ],
                ),
              ),
            ),
            Container(
              width: _size,
              height: _size,
              decoration: BoxDecoration(
                shape: BoxShape.circle,
                gradient: RadialGradient(
                  colors: [
                    Colors.white.withValues(alpha: 0.06),
                    Colors.white.withValues(alpha: 0.02),
                    const Color(0xFF0B0F12).withValues(alpha: 0.25),
                  ],
                  stops: const [0.0, 0.55, 1.0],
                ),
                border: Border.all(color: Colors.white.withValues(alpha: 0.10)),
              ),
            ),
            Opacity(
              opacity: 0.35,
              child: Container(
                width: _size - 26,
                height: _size - 26,
                decoration: BoxDecoration(
                  shape: BoxShape.circle,
                  border: Border.all(color: accent.withValues(alpha: 0.22), width: 1),
                ),
              ),
            ),
            TweenAnimationBuilder<Offset>(
              tween: Tween(begin: Offset.zero, end: offset),
              duration: const Duration(milliseconds: 90),
              curve: Curves.easeOut,
              builder: (context, o, _) {
                return Transform.translate(
                  offset: o,
                  child: Container(
                    width: 58,
                    height: 58,
                    decoration: BoxDecoration(
                      shape: BoxShape.circle,
                      gradient: RadialGradient(
                        colors: [
                          accent.withValues(alpha: active ? 0.55 : 0.40),
                          accent.withValues(alpha: active ? 0.25 : 0.18),
                          Colors.black.withValues(alpha: 0.40),
                        ],
                        stops: const [0.0, 0.62, 1.0],
                      ),
                      border: Border.all(color: accent.withValues(alpha: active ? 0.95 : 0.75), width: 1.3),
                      boxShadow: [
                        BoxShadow(
                          color: Colors.black.withValues(alpha: 0.55),
                          blurRadius: 14,
                          offset: const Offset(0, 8),
                        ),
                      ],
                    ),
                    child: Center(
                      child: Container(
                        width: 10,
                        height: 10,
                        decoration: BoxDecoration(
                          shape: BoxShape.circle,
                          color: Colors.white.withValues(alpha: 0.9),
                          boxShadow: [
                            BoxShadow(color: accent.withValues(alpha: 0.35), blurRadius: 10),
                          ],
                        ),
                      ),
                    ),
                  ),
                );
              },
            ),
          ],
        ),
      ),
    );
  }
}

