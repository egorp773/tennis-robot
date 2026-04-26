import 'dart:ui';

enum DragMode { none, moveRobot, rotateRobot }

enum AppLang { ru, en }

enum RobotMode { calibration, collection, delivery }

class ZonePx {
  ZonePx({required this.id, required this.name, required this.rect});

  final String id;
  final String name;
  final Rect rect; // pixels in canvas
}
