import 'dart:ui' as ui;
import 'package:flutter/material.dart';

import '../models/court_models.dart';

class CourtPainter extends CustomPainter {
  const CourtPainter({
    required this.panel,
    required this.accent,
    required this.playable,
    required this.zonesPx,
    required this.selectedZoneIds,
    required this.robot01,
    required this.robotAngle,
    required this.robotSelected,
    required this.robotRadius,
    required this.handleRadius,
    required this.handleDistance,
    required this.robotImage,
    required this.leftConnected,
    required this.leftError,
    required this.rightConnected,
    required this.rightError,
    this.deliveryPoint,
    this.beacon1Connected = true,
    this.beacon2Connected = true,
    this.isRotating = false,
    this.positionTrail = const [],
    this.livePosition = false,
    this.showHandle = false,
    this.showCoordinates = false,
  });

  final Color panel;
  final Color accent;
  final Rect playable;
  final List<ZonePx> zonesPx;
  final Set<String> selectedZoneIds;

  final Offset robot01;
  final double robotAngle;
  final bool robotSelected;

  final double robotRadius;
  final double handleRadius;
  final double handleDistance;

  final ui.Image? robotImage;

  final bool leftConnected;
  final bool leftError;
  final bool rightConnected;
  final bool rightError;

  final Offset? deliveryPoint;
  final bool beacon1Connected;
  final bool beacon2Connected;
  final bool isRotating;
  final List<Offset> positionTrail;
  final bool livePosition;
  final bool showHandle;
  final bool showCoordinates;

  @override
  void paint(Canvas canvas, Size size) {
    const courtL = 23.77;
    const courtW = 10.97;
    const workL = courtL + 6.0;
    const workW = courtW + 6.0;
    const courtOriginX = 3.0;
    const courtOriginY = 3.0;

    Rect workToRect(double x0, double y0, double x1, double y1) {
      return Rect.fromLTRB(
        playable.left + (y0 / workW) * playable.width,
        playable.top + (x0 / workL) * playable.height,
        playable.left + (y1 / workW) * playable.width,
        playable.top + (x1 / workL) * playable.height,
      );
    }

    final courtRect = workToRect(
      courtOriginX,
      courtOriginY,
      courtOriginX + courtL,
      courtOriginY + courtW,
    );

    // Border around robot working area.
    final borderPaint = Paint()
      ..color = accent.withOpacity(0.3)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 2;

    canvas.drawRect(playable, borderPaint);

    final line = Paint()
      ..color = accent.withOpacity(0.75)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 2;

    // Playable court outer (doubles) inside the robot working area.
    canvas.drawRect(courtRect, line);

    // Singles sidelines
    final singlesInset = courtRect.width * ((courtW - 8.23) / 2) / courtW;
    final singles = Rect.fromLTWH(
      courtRect.left + singlesInset,
      courtRect.top,
      courtRect.width - 2 * singlesInset,
      courtRect.height,
    );
    canvas.drawRect(singles, line);

    // Net
    final netY = courtRect.center.dy;
    canvas.drawLine(Offset(courtRect.left, netY), Offset(courtRect.right, netY), line);

    // Service lines (6.40m from net)
    final serviceOffset = (6.40 / courtL) * courtRect.height;
    final sTop = netY - serviceOffset;
    final sBot = netY + serviceOffset;

    canvas.drawLine(Offset(singles.left, sTop), Offset(singles.right, sTop), line);
    canvas.drawLine(Offset(singles.left, sBot), Offset(singles.right, sBot), line);

    // Center service lines (vertical, between left and right service boxes)
    final centerX = singles.center.dx;
    canvas.drawLine(Offset(centerX, sTop), Offset(centerX, netY), line);
    canvas.drawLine(Offset(centerX, netY), Offset(centerX, sBot), line);

    // UWB anchors are diagonal corners of the robot working area.
    final anchorLPx = playable.topLeft;
    final anchorRPx = playable.bottomRight;

    Color anchorLColor = Colors.white.withOpacity(0.5);
    if (leftError) {
      anchorLColor = Colors.red;
    } else if (leftConnected) {
      anchorLColor = accent;
    }

    Color anchorRColor = Colors.white.withOpacity(0.5);
    if (rightError) {
      anchorRColor = Colors.red;
    } else if (rightConnected) {
      anchorRColor = Colors.green;
    }

    final textPainter = TextPainter(textDirection: TextDirection.ltr);

    void drawAnchor(Offset center, Color color, String label) {
      final r = 10.0;
      final diamond = Path()
        ..moveTo(center.dx, center.dy - r)
        ..lineTo(center.dx + r, center.dy)
        ..lineTo(center.dx, center.dy + r)
        ..lineTo(center.dx - r, center.dy)
        ..close();
      canvas.drawPath(diamond, Paint()..color = color);
      canvas.drawPath(
        diamond,
        Paint()
          ..color = Colors.black.withOpacity(0.4)
          ..style = PaintingStyle.stroke
          ..strokeWidth = 1.5,
      );
      textPainter.text = TextSpan(
        text: label,
        style: const TextStyle(color: Colors.black, fontSize: 11, fontWeight: FontWeight.bold),
      );
      textPainter.layout();
      textPainter.paint(
        canvas,
        Offset(center.dx - textPainter.width / 2, center.dy - textPainter.height / 2),
      );
    }

    drawAnchor(anchorLPx, anchorLColor, 'L');
    drawAnchor(anchorRPx, anchorRColor, 'R');

    // Zones fill strictly INSIDE court-line bounded rects
    for (final z in zonesPx) {
      final enabled = selectedZoneIds.contains(z.id);

      if (enabled) {
        canvas.drawRect(z.rect, Paint()..color = accent.withOpacity(0.14));
      }

      canvas.drawRect(
        z.rect,
        Paint()
          ..color = enabled ? accent.withOpacity(0.45) : accent.withOpacity(0.08)
          ..style = PaintingStyle.stroke
          ..strokeWidth = 2,
      );
    }

    // Position trail (UWB history)
    for (int i = 0; i < positionTrail.length; i++) {
      final t = positionTrail[i];
      final tPx = Offset(
        playable.left + t.dx * playable.width,
        playable.top + t.dy * playable.height,
      );
      final alpha = ((i + 1) / positionTrail.length * 0.6).clamp(0.0, 1.0);
      canvas.drawCircle(tPx, 4, Paint()..color = accent.withOpacity(alpha));
    }

    // Robot
    final robotPx = Offset(
      playable.left + robot01.dx * playable.width,
      playable.top + robot01.dy * playable.height,
    );

    // Live UWB position indicator (pulsing ring)
    if (livePosition) {
      canvas.drawCircle(
        robotPx,
        robotRadius * 1.3,
        Paint()
          ..color = accent.withOpacity(0.15)
          ..style = PaintingStyle.fill,
      );
      canvas.drawCircle(
        robotPx,
        robotRadius * 1.3,
        Paint()
          ..color = accent.withOpacity(0.5)
          ..style = PaintingStyle.stroke
          ..strokeWidth = 2,
      );
    }

    _drawRobot(canvas, robotPx);

    // Coordinate overlay (dev mode)
    if (showCoordinates) {
      final workX = robot01.dy * workL;
      final workY = robot01.dx * workW;
      final coordText = 'X:${workX.toStringAsFixed(2)}  Y:${workY.toStringAsFixed(2)}';
      final tp = TextPainter(textDirection: TextDirection.ltr)
        ..text = TextSpan(
          text: coordText,
          style: const TextStyle(color: Color(0xFF9CCB4A), fontSize: 11, fontWeight: FontWeight.bold),
        )
        ..layout();
      final labelOffset = Offset(robotPx.dx - tp.width / 2 - 4, robotPx.dy - robotRadius - 24);
      canvas.drawRRect(
        RRect.fromRectAndRadius(
          Rect.fromLTWH(labelOffset.dx, labelOffset.dy, tp.width + 8, tp.height + 4),
          const Radius.circular(4),
        ),
        Paint()..color = const Color(0xCC0B0F12),
      );
      tp.paint(canvas, Offset(labelOffset.dx + 4, labelOffset.dy + 2));
    }

    // Delivery point marker
    if (deliveryPoint != null) {
      final deliveryPx = Offset(
        playable.left + deliveryPoint!.dx * playable.width,
        playable.top + deliveryPoint!.dy * playable.height,
      );

      canvas.drawCircle(
        deliveryPx,
        18,
        Paint()
          ..color = accent.withOpacity(0.3)
          ..style = PaintingStyle.stroke
          ..strokeWidth = 3,
      );

      canvas.drawCircle(deliveryPx, 10, Paint()..color = accent.withOpacity(0.8));
      canvas.drawCircle(deliveryPx, 4, Paint()..color = const Color(0xFF0B0F12));

      final pinPath = Path();
      pinPath.moveTo(deliveryPx.dx, deliveryPx.dy - 30);
      pinPath.lineTo(deliveryPx.dx - 8, deliveryPx.dy - 15);
      pinPath.lineTo(deliveryPx.dx + 8, deliveryPx.dy - 15);
      pinPath.close();
      canvas.drawPath(pinPath, Paint()..color = accent);
    }
  }

  void _drawRobot(Canvas canvas, Offset robotPx) {
    canvas.save();
    canvas.translate(robotPx.dx, robotPx.dy);
    canvas.rotate(robotAngle);

    // Draw circle around robot when rotating or selected
    if (isRotating || robotSelected) {
      final circleRadius = robotRadius * 1.8;
      canvas.drawCircle(
        Offset.zero,
        circleRadius,
        Paint()
          ..color = accent.withOpacity(0.3)
          ..style = PaintingStyle.stroke
          ..strokeWidth = 3,
      );
    }

    if (robotImage != null) {
      final maxSize = robotRadius * 2 * 3 / 2 / 2 / 8 * 3 * 2;
      final aspectRatio = robotImage!.width / robotImage!.height;

      double imageWidth, imageHeight;
      if (aspectRatio > 1) {
        imageWidth = maxSize;
        imageHeight = maxSize / aspectRatio;
      } else {
        imageHeight = maxSize;
        imageWidth = maxSize * aspectRatio;
      }

      final srcRect = Rect.fromLTWH(0, 0, robotImage!.width.toDouble(), robotImage!.height.toDouble());
      final dstRect = Rect.fromCenter(center: Offset.zero, width: imageWidth, height: imageHeight);
      canvas.drawImageRect(robotImage!, srcRect, dstRect, Paint());
    } else {
      final p1 = Offset(0, -robotRadius);
      final p2 = Offset(-robotRadius * 0.8, robotRadius * 0.9);
      final p3 = Offset(robotRadius * 0.8, robotRadius * 0.9);

      final tri = Path()
        ..moveTo(p1.dx, p1.dy)
        ..lineTo(p2.dx, p2.dy)
        ..lineTo(p3.dx, p3.dy)
        ..close();

      canvas.drawPath(tri, Paint()..color = accent.withOpacity(0.95));
      canvas.drawPath(
        tri,
        Paint()
          ..color = Colors.black.withOpacity(0.35)
          ..style = PaintingStyle.stroke
          ..strokeWidth = 2,
      );
    }

    // Rotation handle — visible only when showHandle is true
    if (showHandle) {
      final handlePos = Offset(0, -handleDistance);
      canvas.drawLine(
        Offset.zero,
        handlePos,
        Paint()
          ..color = accent.withOpacity(isRotating ? 0.9 : 0.5)
          ..strokeWidth = isRotating ? 2.5 : 1.5,
      );
      canvas.drawCircle(
        handlePos,
        handleRadius,
        Paint()..color = Colors.white.withOpacity(isRotating ? 1.0 : 0.85),
      );
      canvas.drawCircle(
        handlePos,
        handleRadius,
        Paint()
          ..color = accent.withOpacity(isRotating ? 1.0 : 0.7)
          ..style = PaintingStyle.stroke
          ..strokeWidth = isRotating ? 3 : 2,
      );
    }

    canvas.restore();
  }

  @override
  bool shouldRepaint(covariant CourtPainter oldDelegate) {
    if (oldDelegate.robot01 != robot01) return true;
    if (oldDelegate.robotAngle != robotAngle) return true;
    if (oldDelegate.robotSelected != robotSelected) return true;
    if (oldDelegate.selectedZoneIds.length != selectedZoneIds.length) return true;
    if (oldDelegate.robotImage != robotImage) return true;
    if (oldDelegate.deliveryPoint != deliveryPoint) return true;
    if (oldDelegate.beacon1Connected != beacon1Connected) return true;
    if (oldDelegate.beacon2Connected != beacon2Connected) return true;
    if (oldDelegate.leftConnected != leftConnected) return true;
    if (oldDelegate.rightConnected != rightConnected) return true;
    if (oldDelegate.positionTrail.length != positionTrail.length) return true;
    if (oldDelegate.livePosition != livePosition) return true;
    if (oldDelegate.showHandle != showHandle) return true;
    if (oldDelegate.showCoordinates != showCoordinates) return true;
    for (final id in selectedZoneIds) {
      if (!oldDelegate.selectedZoneIds.contains(id)) return true;
    }
    return false;
  }
}
