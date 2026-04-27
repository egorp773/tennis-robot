# Порядок работ и тестов

Этот порядок нужен, чтобы не переходить к автономному движению раньше, чем проверены ручное управление, UWB, safety stops и камера.

## 1. Подготовить локальные секреты

1. Скопировать [`esp32_firmware/wifi_config.example.h`](../esp32_firmware/wifi_config.example.h) в `esp32_firmware/wifi_config.h`.
2. В `wifi_config.h` указать реальный `WIFI_SSID` и `WIFI_PASS`.
3. Для Raspberry Pi не хранить пароль в коде. Использовать переменные окружения:
   - `TENNIS_PI_HOST`
   - `TENNIS_PI_USER`
   - `TENNIS_PI_PASSWORD`

Файл `wifi_config.h` игнорируется Git.

## 2. Проверить сборку и приложение

Из корня проекта:

```powershell
.\arduino-cli-bin\arduino-cli.exe compile --fqbn esp32:esp32:esp32 esp32_firmware\tennis_robot
.\arduino-cli-bin\arduino-cli.exe compile --fqbn esp32:esp32:esp32 esp32_firmware\manual_control
.\arduino-cli-bin\arduino-cli.exe compile --fqbn esp32:esp32:esp32 esp32_firmware\tennis_robot_cal
```

Из [`tennis_app`](../tennis_app):

```powershell
flutter pub get
flutter analyze lib
```

## 3. Проверить ручной режим

1. Прошить [`esp32_firmware/manual_control/manual_control.ino`](../esp32_firmware/manual_control/manual_control.ino).
2. Подключиться приложением [`tennis_app`](../tennis_app) к ESP32.
3. Проверить:
   - joystick movement;
   - STOP;
   - rollers/attachment on/off;
   - disconnect safety stop.
4. Записать результат в [`TEST_LOG.md`](../TEST_LOG.md).

Не переходить к AUTO, пока ручной STOP и связь работают стабильно.

## 4. Проверить UWB без движения

1. Прошить anchors:
   - [`esp32_firmware/anchor_L/anchor_L.ino`](../esp32_firmware/anchor_L/anchor_L.ino)
   - [`esp32_firmware/anchor_R/anchor_R.ino`](../esp32_firmware/anchor_R/anchor_R.ino)
2. Прошить calibration firmware [`esp32_firmware/tennis_robot_cal/tennis_robot_cal.ino`](../esp32_firmware/tennis_robot_cal/tennis_robot_cal.ino).
3. Поставить anchors в диагональные углы robot working area.
4. Проверить, что оба anchors online и ranges стабильны.
5. Записать результат в [`TEST_LOG.md`](../TEST_LOG.md).

## 5. Сделать UWB-калибровку рабочей зоны

1. Использовать [`calibration/measure.py`](../calibration/measure.py).
2. Измерить точки:
   - углы robot working area;
   - углы playable court;
   - центр;
   - service boxes;
   - зоны за пределами корта, но внутри working area.
3. Прогнать [`calibration/analyze.py`](../calibration/analyze.py).
4. Записать RMS/error в [`TEST_LOG.md`](../TEST_LOG.md).

Не запускать физический AUTO, если RMS 2D больше 1 м. Для осторожного теста цель: меньше 0.5 м.

## 6. Проверить safety stops на стенде

Прошить [`esp32_firmware/tennis_robot/tennis_robot.ino`](../esp32_firmware/tennis_robot/tennis_robot.ino).

Через developer terminal в приложении отправить:

```text
TEST_STOP:UWB_LOST
TEST_STOP:UWB_STALE
TEST_STOP:UWB_BRANCH
TEST_STOP:OUT_OF_BOUNDS
TEST_STOP:PI_TIMEOUT
```

Ожидаемо:

- моторы не двигаются;
- приложение показывает `ERROR,...`;
- serial log ESP32 показывает `[SAFE] STOP: ...`.

Результаты записать в [`TEST_LOG.md`](../TEST_LOG.md).

## 7. Проверить камеру и детекцию мячей

На Raspberry Pi проверить [`main.py`](../main.py):

```bash
python3 main.py --test
python3 main.py --no-display
```

Проверить MJPEG stream, FPS и UART-команды:

- `SEARCH`
- `TRACK:dx,size`
- `COLLECT`
- `RETURN`

Снимать реальные кадры теннисного корта:

- солнце;
- тень;
- линии;
- сетка;
- один мяч;
- несколько мячей;
- разные дистанции.

Результаты записать в [`TEST_LOG.md`](../TEST_LOG.md).

## 8. Обучить/проверить модель ИИ для мячей

Только после проверки навигации и базовой камеры.

Файлы:

- [`training/README.md`](../training/README.md)
- [`training/train.py`](../training/train.py)
- [`training/train_colab.py`](../training/train_colab.py)
- [`training/upload_model.py`](../training/upload_model.py)

Порядок:

1. Собрать реальные изображения с Pi.
2. Разметить теннисные мячи.
3. Обучить модель.
4. Сравнить custom `best.pt` с текущим COCO `yolov8n.pt`.
5. Записать false positives / missed balls в [`TEST_LOG.md`](../TEST_LOG.md).

## 9. Первый физический DEMO-тест одной зоны

Использовать только после пунктов 3-7.

1. Прошить full firmware [`esp32_firmware/tennis_robot/tennis_robot.ino`](../esp32_firmware/tennis_robot/tennis_robot.ino).
2. В приложении [`tennis_app`](../tennis_app):
   - подключиться к ESP32;
   - проверить anchors online;
   - поставить robot pose;
   - задать heading;
   - нажать calibration;
   - выбрать одну зону;
   - нажать `DEMO: 1 zone / slow`.
3. Держать физическое аварийное отключение рядом.
4. Людей рядом с роботом не должно быть.
5. Ожидаемо:
   - ESP32 принимает только одну зону;
   - скорость low;
   - по завершении одной зоны приходит `DEMO_DONE`;
   - при сбое приходит понятный `ERROR,...`.
6. Записать результат в [`TEST_LOG.md`](../TEST_LOG.md).

## 10. Только после стабильного DEMO

Переходить к обычному `AUTO_START` и нескольким зонам только если:

- ручной STOP работает;
- UWB RMS приемлемый;
- safety injection прошёл;
- Pi camera/UART стабильны;
- DEMO повторился 3 раза без ручного восстановления.
