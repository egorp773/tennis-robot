#!/usr/bin/env python3
"""
Дообучение YOLOv8n на теннисные мячи.

Запуск:
  cd C:\\robot\\tennis\\training
  python train.py

Датасет должен лежать в папке dataset/ (экспорт из Roboflow в формате YOLOv8).
"""
import os
import sys
import shutil
from pathlib import Path

try:
    from ultralytics import YOLO
except ImportError:
    print("Установи ultralytics: pip install ultralytics")
    sys.exit(1)

# ─────────────────────────── НАСТРОЙКИ ───────────────────────────────────────

DATASET_YAML  = "dataset/data.yaml"   # путь к data.yaml из Roboflow
BASE_MODEL    = "yolov8n.pt"          # базовая модель (скачается автоматически)
OUTPUT_NAME   = "tennis_ball_v1"      # имя папки с результатами
EPOCHS        = 100                   # эпох обучения
IMG_SIZE      = 640                   # размер входного изображения
BATCH         = 16                    # batch size (-1 = авто)
PATIENCE      = 20                    # early stopping если нет улучшений

# ─────────────────────────── ПРОВЕРКИ ────────────────────────────────────────

def check_dataset():
    yaml_path = Path(DATASET_YAML)
    if not yaml_path.exists():
        print(f"[ОШИБКА] Файл {DATASET_YAML} не найден.")
        print("  Экспортируй датасет из Roboflow в формате YOLOv8")
        print("  и распакуй в папку training/dataset/")
        sys.exit(1)

    # Читаем data.yaml и выводим информацию
    with open(yaml_path, encoding="utf-8") as f:
        content = f.read()
    print("[ДАТАСЕТ]")
    print(content)

    # Считаем фото
    for split in ("train", "valid", "test"):
        img_dir = yaml_path.parent / split / "images"
        if img_dir.exists():
            count = len(list(img_dir.glob("*.jpg")) + list(img_dir.glob("*.png")))
            print(f"  {split}: {count} фото")

# ─────────────────────────── ОБУЧЕНИЕ ────────────────────────────────────────

def train():
    check_dataset()

    print(f"\n[INFO] Базовая модель: {BASE_MODEL}")
    print(f"[INFO] Эпох: {EPOCHS}, img_size: {IMG_SIZE}, batch: {BATCH}")
    print(f"[INFO] Результаты: runs/detect/{OUTPUT_NAME}/\n")

    model = YOLO(BASE_MODEL)

    results = model.train(
        data=DATASET_YAML,
        epochs=EPOCHS,
        imgsz=IMG_SIZE,
        batch=BATCH,
        patience=PATIENCE,
        name=OUTPUT_NAME,
        exist_ok=True,

        # Аугментации — усиливаем для уличного спорта
        hsv_h=0.02,     # оттенок ±2% (мяч меняет цвет при разном свете)
        hsv_s=0.7,      # насыщенность
        hsv_v=0.4,      # яркость
        degrees=10.0,   # поворот ±10°
        translate=0.1,
        scale=0.5,      # масштаб ×0.5–1.5
        fliplr=0.5,     # отзеркаливание по горизонтали
        mosaic=1.0,     # mosaic аугментация
        mixup=0.1,

        # Производительность
        workers=4,
        device=0 if _has_gpu() else "cpu",

        # Сохранение
        save=True,
        save_period=10,  # сохранять веса каждые 10 эпох
    )

    # Путь к лучшей модели
    best = Path(f"runs/detect/{OUTPUT_NAME}/weights/best.pt")
    if best.exists():
        # Копируем в удобное место
        shutil.copy(best, "best.pt")
        print(f"\n{'='*55}")
        print(f"  Обучение завершено!")
        print(f"  Лучшая модель: training/best.pt")
        print(f"  mAP@50: {results.results_dict.get('metrics/mAP50(B)', '?'):.3f}")
        print(f"{'='*55}")
        print(f"\nСледующий шаг:")
        print(f"  python upload_model.py")
    else:
        print("[ОШИБКА] best.pt не найден — обучение не завершилось")


def _has_gpu():
    try:
        import torch
        return torch.cuda.is_available()
    except ImportError:
        return False


# ─────────────────────────── ТЕСТ ПОСЛЕ ОБУЧЕНИЯ ─────────────────────────────

def test_model(image_path=None):
    """Быстрая проверка обученной модели на одном изображении."""
    model_path = Path("best.pt")
    if not model_path.exists():
        print("best.pt не найден. Сначала запусти train.py")
        return

    model = YOLO(str(model_path))

    if image_path is None:
        # Берём первое фото из test набора
        test_dir = Path("dataset/test/images")
        if test_dir.exists():
            images = list(test_dir.glob("*.jpg")) + list(test_dir.glob("*.png"))
            if images:
                image_path = str(images[0])

    if image_path is None:
        print("Укажи путь к фото: python train.py test путь/к/фото.jpg")
        return

    results = model(image_path, conf=0.25, save=True)
    for r in results:
        print(f"Найдено мячей: {len(r.boxes)}")
        for box in r.boxes:
            print(f"  conf={float(box.conf[0]):.2f}  bbox={[int(v) for v in box.xyxy[0].tolist()]}")
    print(f"Результат сохранён в runs/detect/predict/")


# ─────────────────────────── ТОЧКА ВХОДА ─────────────────────────────────────

if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "test":
        img = sys.argv[2] if len(sys.argv) > 2 else None
        test_model(img)
    else:
        train()
