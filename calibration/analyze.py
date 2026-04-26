#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Анализ данных калибровки UWB.
Запускать после measure.py.
"""
import sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
import json
import math
import sys
import os

INPUT_FILE = "measurements.json"

# Скорость света (м/с)
C = 299_792_458.0
# Базовая задержка антенны DW1000 (в единицах DW1000 time unit = 15.65 пс)
DEFAULT_ANTENNA_DELAY = 16436
DW_TIME_UNIT = 15.65e-12   # секунды


def compute_true_dist(px, py, ax, ay):
    return math.hypot(px - ax, py - ay)


def antenna_delay_from_scale(measured, actual):
    """
    measured = actual * scale  →  scale = measured / actual
    Ошибка расстояния = measured - actual
    Для DW1000: delay_correction = error / C * 2 / DW_TIME_UNIT
    (делим на 2, т.к. дальность = половина RTT)
    Возвращает delta_delay (добавить к ANTENNA_DELAY).
    """
    if actual < 0.01:
        return 0.0
    error_m = measured - actual
    delta_tof_s = error_m / C
    delta_delay_units = delta_tof_s / DW_TIME_UNIT
    return delta_delay_units


def main():
    if not os.path.exists(INPUT_FILE):
        print(f"Файл {INPUT_FILE} не найден. Сначала запусти measure.py")
        sys.exit(1)

    with open(INPUT_FILE, encoding="utf-8") as f:
        data = json.load(f)

    AL = data["anchors"]["L"]
    AR = data["anchors"]["R"]
    points = data["points"]

    if not points:
        print("Нет точек измерения.")
        sys.exit(1)

    print("=" * 60)
    print("  Анализ калибровки UWB")
    print("=" * 60)
    print(f"Маяк L: ({AL['x']:.1f}, {AL['y']:.1f})")
    print(f"Маяк R: ({AR['x']:.1f}, {AR['y']:.1f})")
    print(f"Точек: {len(points)}\n")

    errors_l = []
    errors_r = []
    errors_x = []
    errors_y = []
    deltas_l = []
    deltas_r = []

    print(f"{'Точка':<8} {'Ист.L':>7} {'Изм.L':>7} {'Err L':>7}  "
          f"{'Ист.R':>7} {'Изм.R':>7} {'Err R':>7}  "
          f"{'Err X':>7} {'Err Y':>7}")
    print("-" * 70)

    for i, pt in enumerate(points):
        ref_x, ref_y = pt["ref_x"], pt["ref_y"]
        pos_x, pos_y = pt["pos_x"], pt["pos_y"]
        rl, rr = pt["range_l"], pt["range_r"]

        true_l = compute_true_dist(ref_x, ref_y, AL["x"], AL["y"])
        true_r = compute_true_dist(ref_x, ref_y, AR["x"], AR["y"])

        err_l = rl - true_l
        err_r = rr - true_r
        err_x = pos_x - ref_x
        err_y = pos_y - ref_y

        errors_l.append(err_l)
        errors_r.append(err_r)
        errors_x.append(err_x)
        errors_y.append(err_y)
        deltas_l.append(antenna_delay_from_scale(rl, true_l))
        deltas_r.append(antenna_delay_from_scale(rr, true_r))

        print(f"P{i+1:<7} {true_l:>7.3f} {rl:>7.3f} {err_l:>+7.3f}  "
              f"{true_r:>7.3f} {rr:>7.3f} {err_r:>+7.3f}  "
              f"{err_x:>+7.3f} {err_y:>+7.3f}")

    print("-" * 70)

    def mean(lst): return sum(lst) / len(lst)
    def rms(lst): return math.sqrt(sum(x**2 for x in lst) / len(lst))
    def median(lst): return sorted(lst)[len(lst) // 2]

    print(f"\n{'Среднее':>10}: L={mean(errors_l):>+7.3f} м    R={mean(errors_r):>+7.3f} м    "
          f"X={mean(errors_x):>+7.3f} м    Y={mean(errors_y):>+7.3f} м")
    print(f"{'RMS':>10}: L={rms(errors_l):>7.3f} м    R={rms(errors_r):>7.3f} м    "
          f"X={rms(errors_x):>7.3f} м    Y={rms(errors_y):>7.3f} м")

    # --- Рекомендация antenna delay ---
    delta_l_med = median(deltas_l)
    delta_r_med = median(deltas_r)
    # Усредняем обе антенны (тег + маяки одинаковые чипы)
    delta_avg = (delta_l_med + delta_r_med) / 2.0

    new_delay = round(DEFAULT_ANTENNA_DELAY + delta_avg)

    print("\n" + "=" * 60)
    print("  Рекомендации")
    print("=" * 60)
    print(f"Текущий ANTENNA_DELAY = {DEFAULT_ANTENNA_DELAY}")
    print(f"Поправка (L): {delta_l_med:+.1f} ед.")
    print(f"Поправка (R): {delta_r_med:+.1f} ед.")
    print(f"\n>>> Новый ANTENNA_DELAY = {new_delay} <<<")

    # --- Оценка качества ---
    pos_rms = math.sqrt(rms(errors_x)**2 + rms(errors_y)**2)
    print(f"\nОшибка позиции (RMS 2D): {pos_rms:.3f} м", end="  ")
    if pos_rms < 0.15:
        print("✓ Отлично")
    elif pos_rms < 0.30:
        print("~ Приемлемо")
    else:
        print("✗ Плохо — перепроверь расстановку маяков")

    print()
    if abs(mean(errors_l)) > 0.5 or abs(mean(errors_r)) > 0.5:
        print("ВНИМАНИЕ: Большое систематическое смещение (>0.5 м).")
        print("  Возможные причины:")
        print("  - Неправильно измерены позиции маяков")
        print("  - Маяки на разной высоте")
        print("  - Сильные отражения от стен (metallic)")

    print()
    print("Что сделать:")
    print(f"  1. В tennis_robot.ino замени:")
    print(f"       #define ANTENNA_DELAY {DEFAULT_ANTENNA_DELAY}")
    print(f"     на:")
    print(f"       #define ANTENNA_DELAY {new_delay}")
    print(f"  2. Перекомпилируй и загрузи прошивку")
    print(f"  3. Повтори measure.py для проверки")

    # Сохраняем результаты
    result = {
        "recommended_antenna_delay": new_delay,
        "delta": delta_avg,
        "errors": {
            "mean_l": mean(errors_l),
            "mean_r": mean(errors_r),
            "rms_l": rms(errors_l),
            "rms_r": rms(errors_r),
            "mean_x": mean(errors_x),
            "mean_y": mean(errors_y),
            "rms_x": rms(errors_x),
            "rms_y": rms(errors_y),
            "pos_rms_2d": pos_rms,
        }
    }
    with open("calibration_result.json", "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2)
    print(f"\nРезультат сохранён: calibration_result.json")


if __name__ == "__main__":
    main()
