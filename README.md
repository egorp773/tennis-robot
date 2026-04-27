# Tennis Robot

Автономный прототип робота для сбора теннисных мячей: мобильная платформа на ESP32, Flutter-приложение оператора, Raspberry Pi с камерой и YOLO-детекцией, UWB-навигация по двум якорям и набор отдельных прошивок для безопасной отладки железа.

> Статус: рабочий прототип. Ручное управление уже является основной проверенной возможностью. Автономный режим имеет программные safety-gates, но еще не прошел полный физический цикл UWB/Pi/camera/AUTO-тестов. Не запускайте unrestricted `AUTO_START`, пока не пройдены проверки из [операторского регламента](docs/OPERATOR_RUNBOOK_RU.md).

## Что Уже Есть

- Flutter-приложение для ручного управления, выбора зон, калибровки позиции и запуска ограниченного demo-режима.
- Основная прошивка ESP32 с ручным режимом, автономным state machine, UWB-трекингом, UART-протоколом с Raspberry Pi, управлением hoverboard-моторами, коллектором и телеметрией.
- Raspberry Pi pipeline: камера IMX708 Wide NoIR, YOLOv8 sports-ball detection, MJPEG stream и UART-команды на ESP32.
- Две UWB-метки/якоря DW1000 в диагональных углах рабочей зоны робота.
- Отдельные прошивки для anchor, UWB-калибровки, ручных моторов, коллектора и camera-only collection.
- Bench safety injection-команды для проверки STOP-сценариев без движения.
- Консервативный `DEMO_START`: одна зона, низкая скорость, строгие таймауты.

## Архитектура

В проекте есть две разные геометрии, и их нельзя смешивать:

- **Playable court**: стандартный теннисный корт 23.77 x 10.97 м. Используется для линий, сетки, service boxes и игровых зон.
- **Robot working area**: больший прямоугольник вокруг корта, где робот может ездить и собирать мячи. Сейчас в коде используется фиксированная рабочая зона с отступами 3 м: 29.77 x 16.97 м.

UWB-якоря:

- `Anchor L` и `Anchor R` стоят в противоположных диагональных углах **robot working area**, а не обязательно на углах игрового корта.
- Первая версия приложения показывает фиксированную раскладку якорей. Drag-and-drop якорей не нужен.
- Два якоря дают две возможные позиции. Калибровка выбирает ветку, ближайшую к позиции робота, заданной в приложении. В рантайме выбирается кандидат, ближайший к последней принятой позиции.

AUTO запрещен, если нет валидной калибровки, heading, свежего UWB и принятой позиции внутри рабочей зоны.

## Основные Компоненты

| Компонент | Где находится | Назначение |
|---|---|---|
| Flutter app | [`tennis_app/`](tennis_app/) | UI оператора, ручной режим, зоны, калибровка, telemetry, demo |
| ESP32 robot | [`esp32_firmware/tennis_robot/tennis_robot.ino`](esp32_firmware/tennis_robot/tennis_robot.ino) | Основная прошивка робота |
| UWB anchors | [`esp32_firmware/anchor_L/`](esp32_firmware/anchor_L/), [`esp32_firmware/anchor_R/`](esp32_firmware/anchor_R/) | Два DW1000-якоря рабочей зоны |
| UWB calibration | [`esp32_firmware/tennis_robot_cal/`](esp32_firmware/tennis_robot_cal/), [`calibration/`](calibration/) | Измерение и анализ UWB-точек |
| Pi vision | [`main.py`](main.py) | Камера, YOLO, MJPEG, UART-команды |
| Training | [`training/`](training/) | Обучение/загрузка custom YOLO-модели |
| Runbook | [`docs/OPERATOR_RUNBOOK_RU.md`](docs/OPERATOR_RUNBOOK_RU.md) | Порядок физических проверок |
| Status | [`IMPLEMENTATION_STATUS.md`](IMPLEMENTATION_STATUS.md) | Текущий статус реализации |
| Decisions | [`DECISIONS.md`](DECISIONS.md) | Принятые архитектурные решения |
| Roadmap | [`AUTONOMY_ROADMAP.md`](AUTONOMY_ROADMAP.md) | План доведения autonomy |
| Test log | [`TEST_LOG.md`](TEST_LOG.md) | Журнал физических тестов |

## Быстрый Старт Для Разработки

### 1. Клонировать репозиторий

```powershell
git clone https://github.com/egorp773/tennis-robot.git
cd tennis-robot
```

### 2. Подготовить локальные секреты

Не коммитьте Wi-Fi пароли, токены, `.env`, приватные ключи и личные данные.

```powershell
Copy-Item esp32_firmware\wifi_config.example.h esp32_firmware\wifi_config.h
```

В `esp32_firmware/wifi_config.h` укажите реальные значения `WIFI_SSID` и `WIFI_PASS`. Этот файл должен оставаться локальным и игнорироваться Git.

Для Raspberry Pi используйте переменные окружения вместо паролей в коде:

```powershell
$env:TENNIS_PI_HOST="192.168.1.115"
$env:TENNIS_PI_USER="pi"
$env:TENNIS_PI_PASSWORD="<local-password>"
```

### 3. Проверить Flutter-приложение

```powershell
cd tennis_app
flutter pub get
flutter analyze lib
```

### 4. Проверить ESP32-прошивку

Установите Arduino CLI или положите локальный бинарник в `arduino-cli-bin/` на своей машине.

```powershell
.\arduino-cli-bin\arduino-cli.exe compile --fqbn esp32:esp32:esp32 esp32_firmware\tennis_robot
.\arduino-cli-bin\arduino-cli.exe compile --fqbn esp32:esp32:esp32 esp32_firmware\manual_control
.\arduino-cli-bin\arduino-cli.exe compile --fqbn esp32:esp32:esp32 esp32_firmware\tennis_robot_cal
.\arduino-cli-bin\arduino-cli.exe compile --fqbn esp32:esp32:esp32 esp32_firmware\anchor_L
.\arduino-cli-bin\arduino-cli.exe compile --fqbn esp32:esp32:esp32 esp32_firmware\anchor_R
```

Если `arduino-cli` установлен в `PATH`, можно заменить `.\arduino-cli-bin\arduino-cli.exe` на `arduino-cli`.

### 5. Проверить Raspberry Pi vision

На Raspberry Pi:

```bash
python3 main.py --test
python3 main.py --no-display
```

Проверьте MJPEG stream, FPS и UART-команды `SEARCH`, `TRACK:dx,size`, `COLLECT`, `RETURN`.

## Safety

Робот должен останавливаться при:

- потере обоих UWB-якорей;
- stale UWB ranges;
- невозможном скачке позиции;
- неоднозначной или сломанной UWB-ветке;
- выходе принятой позиции за robot working area;
- таймауте Raspberry Pi UART, если движение зависит от камеры;
- ручном STOP или потере связи оператора.

Для bench-проверки safety без движения в приложении доступны developer-команды:

```text
TEST_STOP:UWB_LOST
TEST_STOP:UWB_STALE
TEST_STOP:UWB_BRANCH
TEST_STOP:OUT_OF_BOUNDS
TEST_STOP:PI_TIMEOUT
```

Ожидаемо: моторы не двигаются, приложение показывает `ERROR,...`, ESP32 пишет `[SAFE] STOP: ...`.

## Порядок Реальных Тестов

Подробный порядок находится в [docs/OPERATOR_RUNBOOK_RU.md](docs/OPERATOR_RUNBOOK_RU.md). Короткая версия:

1. Подготовить локальные секреты: [`esp32_firmware/wifi_config.example.h`](esp32_firmware/wifi_config.example.h) -> `esp32_firmware/wifi_config.h`, переменные `TENNIS_PI_*`.
2. Проверить сборку: [`esp32_firmware/tennis_robot/`](esp32_firmware/tennis_robot/), [`esp32_firmware/manual_control/`](esp32_firmware/manual_control/), [`tennis_app/`](tennis_app/).
3. Проверить ручной режим: [`esp32_firmware/manual_control/manual_control.ino`](esp32_firmware/manual_control/manual_control.ino), STOP, joystick, attachment/rollers, disconnect safety.
4. Проверить UWB без движения: [`esp32_firmware/anchor_L/`](esp32_firmware/anchor_L/), [`esp32_firmware/anchor_R/`](esp32_firmware/anchor_R/), [`esp32_firmware/tennis_robot_cal/`](esp32_firmware/tennis_robot_cal/).
5. Сделать UWB-калибровку рабочей зоны: [`calibration/measure.py`](calibration/measure.py), [`calibration/analyze.py`](calibration/analyze.py). Логировать RMS/error в [`TEST_LOG.md`](TEST_LOG.md).
6. Проверить safety stops на стенде: `TEST_STOP:UWB_LOST`, `TEST_STOP:UWB_STALE`, `TEST_STOP:UWB_BRANCH`, `TEST_STOP:OUT_OF_BOUNDS`, `TEST_STOP:PI_TIMEOUT`.
7. Проверить камеру и детекцию мячей: [`main.py`](main.py), `python3 main.py --test`, `python3 main.py --no-display`.
8. Собрать реальные кадры корта и только после базовой навигационной проверки обучить/проверить AI-модель: [`training/README.md`](training/README.md), [`training/train.py`](training/train.py), [`training/train_colab.py`](training/train_colab.py), [`training/upload_model.py`](training/upload_model.py).
9. Запустить первый физический `DEMO_START` только для одной зоны через [`tennis_app/`](tennis_app/) и [`esp32_firmware/tennis_robot/tennis_robot.ino`](esp32_firmware/tennis_robot/tennis_robot.ino).
10. Переходить к обычному `AUTO_START` и нескольким зонам только после стабильного demo, UWB RMS, camera/UART и safety checks.

Каждый физический или интеграционный тест записывайте в [`TEST_LOG.md`](TEST_LOG.md).

## Команды Приложения И Прошивки

Ключевые команды, которые важно знать при отладке:

```text
CALIBRATE:heading:x:y
ZONES:...
SET_SPEED:0
AUTO_START
DEMO_START
STOP
TEST_STOP:UWB_LOST
TEST_STOP:UWB_STALE
TEST_STOP:UWB_BRANCH
TEST_STOP:OUT_OF_BOUNDS
TEST_STOP:PI_TIMEOUT
```

`DEMO_START` принимает только одну выбранную зону, принудительно использует низкую скорость и завершает сценарий после одного прохода/сбора или demo timeout.

## Что Не Готово

- Нет полного журнала физических UWB/Pi/camera/AUTO-тестов.
- UWB нужно проверить на реальной рабочей зоне, а не только компиляцией и bench-сценариями.
- Камеру и YOLO нужно проверить на реальном корте при солнце, тени, сетке, линиях, одном мяче, нескольких мячах и разных дистанциях.
- Custom YOLO-модель стоит обучать после того, как навигация и базовая камера стабильно работают.
- Для продуктовой надежности в будущем могут потребоваться 3-4 UWB-якоря.

## Правила Разработки

- Не смешивать playable court и robot working area.
- Не запускать физический unrestricted AUTO без валидной калибровки, heading, свежего UWB и safety checks.
- При изменении архитектуры обновлять [`MEMORY.md`](MEMORY.md), [`DECISIONS.md`](DECISIONS.md), [`AUTONOMY_ROADMAP.md`](AUTONOMY_ROADMAP.md), [`IMPLEMENTATION_STATUS.md`](IMPLEMENTATION_STATUS.md) и релевантные [`docs/ai_skills/*/SKILL.md`](docs/ai_skills/).
- При физических тестах дописывать результаты в [`TEST_LOG.md`](TEST_LOG.md).
- Не коммитить `build/`, `.dart_tool/`, `.idea/`, `.vscode/`, `.DS_Store`, `*.log`, `.env`, секреты, ключи, токены и Wi-Fi пароли.

## Репозиторий

GitHub: https://github.com/egorp773/tennis-robot
