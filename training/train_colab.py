#!/usr/bin/env python3
"""
Версия для Google Colab (бесплатный GPU).

Как использовать:
1. Открой colab.research.google.com
2. Создай новый ноутбук
3. Runtime → Change runtime type → GPU (T4)
4. Скопируй этот код в ячейку и запусти

ИЛИ загрузи этот файл на Colab через File → Upload.
"""

# ── Установка ──────────────────────────────────────────────────────────────
# !pip install ultralytics -q

# ── Загрузка датасета ──────────────────────────────────────────────────────
# Вариант 1: загрузить zip напрямую из Roboflow
# (замени URL на свой из Roboflow → Export → Show download code)
#
# import roboflow
# rf = roboflow.Roboflow(api_key="ВАШ_API_KEY")
# project = rf.workspace("ВАШ_WORKSPACE").project("tennis-ball")
# dataset = project.version(1).download("yolov8")

# Вариант 2: загрузить zip файл вручную
import os
from pathlib import Path

DATASET_ZIP  = "dataset.zip"    # загрузи этот файл в Colab
DATASET_DIR  = "dataset"
EPOCHS       = 100
OUTPUT_NAME  = "tennis_ball_v1"

# Распаковка
if Path(DATASET_ZIP).exists() and not Path(DATASET_DIR).exists():
    os.system(f"unzip -q {DATASET_ZIP} -d {DATASET_DIR}")
    print(f"Датасет распакован в {DATASET_DIR}/")

# Поиск data.yaml
yaml_candidates = list(Path(DATASET_DIR).rglob("data.yaml"))
if not yaml_candidates:
    raise FileNotFoundError("data.yaml не найден! Проверь структуру датасета.")
DATASET_YAML = str(yaml_candidates[0])
print(f"Используем: {DATASET_YAML}")

# ── Обучение ───────────────────────────────────────────────────────────────
from ultralytics import YOLO

model = YOLO("yolov8n.pt")

results = model.train(
    data=DATASET_YAML,
    epochs=EPOCHS,
    imgsz=640,
    batch=32,          # на Colab T4 можно больший batch
    patience=20,
    name=OUTPUT_NAME,
    exist_ok=True,
    device=0,          # GPU
    hsv_h=0.02,
    hsv_s=0.7,
    hsv_v=0.4,
    degrees=10.0,
    scale=0.5,
    fliplr=0.5,
    mosaic=1.0,
    mixup=0.1,
)

# ── Скачивание модели ──────────────────────────────────────────────────────
best = Path(f"runs/detect/{OUTPUT_NAME}/weights/best.pt")
print(f"\nЛучшая модель: {best}")
print(f"mAP@50: {results.results_dict.get('metrics/mAP50(B)', '?'):.3f}")

# Скачать best.pt на компьютер (только в Colab)
try:
    from google.colab import files
    files.download(str(best))
    print("Файл best.pt скачивается...")
except ImportError:
    print(f"Скачай вручную: {best}")

print("\nПосле скачивания:")
print("  1. Положи best.pt в папку training/")
print("  2. Запусти: python upload_model.py")
