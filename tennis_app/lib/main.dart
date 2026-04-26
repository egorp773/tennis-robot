// lib/main.dart

//

// ✅ Исправления по твоим требованиям:

// 1) ЗОНЫ теперь НЕ "кривые прямоугольники" и НЕ "сетка".

//    Выбираемые зоны = РЕАЛЬНЫЕ прямоугольные области, которые ОГРАНИЧЕНЫ ЛИНИЯМИ КОРТА:

//    baseline / service line / net / center line / singles sidelines.

//    (То есть ровно те области, которые образуются зелёными линиями разметки.)

// 2) Можно выбирать НЕСКОЛЬКО зон (toggle по тапу).

// 3) ПОВОРОТ РОБОТА ПОЧИНЕН: ручка вращения всегда "спереди", hit-test и математика согласованы.

//

import 'dart:io' show Platform;
import 'package:flutter/foundation.dart' show kIsWeb;
import 'package:flutter/material.dart';
import 'package:device_preview/device_preview.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'app.dart';

void main() {
  final isMobile = !kIsWeb && (Platform.isIOS || Platform.isAndroid);
  
  if (isMobile) {
    // On mobile: run without DevicePreview
    runApp(
      ProviderScope(
        child: const MyApp(),
      ),
    );
  } else {
    // On web/desktop: run with DevicePreview
    runApp(
      ProviderScope(
        child: DevicePreview(
          builder: (context) => const MyApp(),
        ),
      ),
    );
  }
}