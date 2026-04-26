#!/usr/bin/env python3
"""
Загружает обученную модель best.pt на Raspberry Pi
и обновляет main.py чтобы использовать её вместо yolov8n.pt.

Запуск:
  cd C:\\robot\\tennis\\training
  python upload_model.py
"""
import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from pathlib import Path
from ssh_helper import upload_file, run_command

MODEL_LOCAL  = "best.pt"
MODEL_REMOTE = "/home/pi/tennis/best.pt"
MAIN_REMOTE  = "/home/pi/tennis/main.py"

def main():
    model = Path(MODEL_LOCAL)
    if not model.exists():
        print(f"[ОШИБКА] {MODEL_LOCAL} не найден.")
        print("  Сначала запусти: python train.py")
        sys.exit(1)

    size_mb = model.stat().st_size / 1024 / 1024
    print(f"Загружаем {MODEL_LOCAL} ({size_mb:.1f} МБ) на Pi...")
    upload_file(str(model), MODEL_REMOTE)
    print("Загружено.")

    # Переключаем main.py на новую модель
    print("Обновляем main.py на Pi...")
    run_command(
        f"sed -i \"s|weights='yolov8n.pt'|weights='{MODEL_REMOTE}'|g\" {MAIN_REMOTE}",
        verbose=False
    )
    run_command(
        f"sed -i \"s|weights=\\\"yolov8n.pt\\\"|weights=\\\"{MODEL_REMOTE}\\\"|g\" {MAIN_REMOTE}",
        verbose=False
    )
    print("Готово.")

    # Быстрая проверка
    print("\nПроверяем модель на Pi (--test режим)...")
    out, err, code = run_command(
        f"source /home/pi/tennis/venv/bin/activate && "
        f"python3 {MAIN_REMOTE} --test 2>&1 | tail -10",
        timeout=60
    )
    if code == 0:
        print("\n[OK] Модель работает на Pi.")
    else:
        print(f"\n[ОШИБКА] Код {code}. Проверь вывод выше.")

if __name__ == "__main__":
    main()
