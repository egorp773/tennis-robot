#!/usr/bin/env python3
"""
РЎР±РѕСЂ РґР°РЅРЅС‹С… UWB РґР»СЏ РєР°Р»РёР±СЂРѕРІРєРё.
Р—Р°РїСѓСЃРєР°С‚СЊ РЅР° Windows, РїРѕРґРєР»СЋС‡РёРІС€РёСЃСЊ Рє Wi-Fi СЂРѕР±РѕС‚Р° (TennisRobot / <configured password>).
"""
import json
import sys
import threading
import time

try:
    import websocket
except ImportError:
    print("РћС€РёР±РєР°: СѓСЃС‚Р°РЅРѕРІРё websocket-client:")
    print("  pip install websocket-client")
    sys.exit(1)

WS_URL = "ws://192.168.4.1:81/ws"
OUTPUT_FILE = "measurements.json"

# Control points (X, Y) in robot working-area metres.
# Working area = playable court plus 3 m margin on each side:
#   size = 29.77 x 16.97 m
#   playable court origin = (3.0, 3.0)
# Anchors are diagonal working-area corners: L=(0,0), R=(29.77,16.97).
REFERENCE_POINTS = [
    (3.0,   3.0),    # playable court corner A-left
    (3.0,  13.97),   # playable court corner A-right
    (14.89, 8.49),   # playable court center
    (26.77, 3.0),    # playable court corner B-left
    (26.77, 13.97),  # playable court corner B-right
    (6.0,   8.5),    # baseline A area
    (14.9,  5.7),    # service left area
    (14.9, 11.2),    # service right area
    (0.8,   8.5),    # outside back A inside working area
    (29.0,  8.5),    # outside back B inside working area
]
SAMPLES_PER_POINT = 15   # РёР·РјРµСЂРµРЅРёР№ РЅР° С‚РѕС‡РєСѓ (~5 СЃРµРєСѓРЅРґ РїСЂРё 3 Hz)

# --- РіР»РѕР±Р°Р»СЊРЅС‹Рµ РїРµСЂРµРјРµРЅРЅС‹Рµ ---
_range_l = None
_range_r = None
_pos_x = None
_pos_y = None
_lock = threading.Lock()
_collecting = False
_buf: list = []   # Р±СѓС„РµСЂ С‚РµРєСѓС‰РµР№ С‚РѕС‡РєРё
_ws_app = None


def _on_message(ws_app, message: str):
    global _range_l, _range_r, _pos_x, _pos_y
    try:
        if message.startswith("RANGE,L,"):
            with _lock:
                _range_l = float(message.split(",")[2])
        elif message.startswith("RANGE,R,"):
            with _lock:
                _range_r = float(message.split(",")[2])
        elif message.startswith("POS,"):
            parts = message.split(",")
            with _lock:
                _pos_x = float(parts[1])
                _pos_y = float(parts[2])
                if _collecting and _range_l is not None and _range_r is not None:
                    _buf.append({
                        "pos_x": _pos_x,
                        "pos_y": _pos_y,
                        "range_l": _range_l,
                        "range_r": _range_r,
                    })
    except Exception:
        pass


def _on_error(ws_app, error):
    print(f"\n[WS error] {error}")


def _on_close(ws_app, code, msg):
    print("\n[WS] РЎРѕРµРґРёРЅРµРЅРёРµ Р·Р°РєСЂС‹С‚Рѕ")


def _on_open(ws_app):
    print("[WS] РџРѕРґРєР»СЋС‡РµРЅРѕ. Р’РєР»СЋС‡Р°РµРј CAL_RAW...")
    ws_app.send("CAL_RAW")


def _connect():
    global _ws_app
    _ws_app = websocket.WebSocketApp(
        WS_URL,
        on_open=_on_open,
        on_message=_on_message,
        on_error=_on_error,
        on_close=_on_close,
    )
    t = threading.Thread(target=_ws_app.run_forever, kwargs={"ping_interval": 10}, daemon=True)
    t.start()
    # Р¶РґС‘Рј РїРѕРґРєР»СЋС‡РµРЅРёСЏ
    for _ in range(30):
        time.sleep(0.5)
        if _ws_app.sock and _ws_app.sock.connected:
            return True
    return False


def _collect_samples(n: int) -> list:
    global _collecting, _buf
    with _lock:
        _buf = []
        _collecting = True
    collected = []
    deadline = time.time() + n * 2.0   # РјР°РєСЃРёРјР°Р»СЊРЅРѕРµ РІСЂРµРјСЏ РѕР¶РёРґР°РЅРёСЏ
    while time.time() < deadline:
        time.sleep(0.1)
        with _lock:
            collected = list(_buf)
        if len(collected) >= n:
            break
        sys.stdout.write(f"\r  РЎРѕР±СЂР°РЅРѕ {len(collected)}/{n}...   ")
        sys.stdout.flush()
    with _lock:
        _collecting = False
        _buf = []
    print()
    return collected[:n]


def main():
    print("=" * 55)
    print("  РљР°Р»РёР±СЂРѕРІРєР° UWB вЂ” СЃР±РѕСЂ РёР·РјРµСЂРµРЅРёР№")
    print("=" * 55)
    print(f"РџРѕРґРєР»СЋС‡РµРЅРёРµ Рє {WS_URL} ...")

    if not _connect():
        print("РќРµ СѓРґР°Р»РѕСЃСЊ РїРѕРґРєР»СЋС‡РёС‚СЊСЃСЏ. РџСЂРѕРІРµСЂСЊ:")
        print("  1. Wi-Fi: TennisRobot / <configured password>")
        print("  2. ESP32 РІРєР»СЋС‡С‘РЅ")
        sys.exit(1)

    print("РџРѕРґРєР»СЋС‡РµРЅРѕ!\n")
    time.sleep(1.5)  # Р¶РґС‘Рј РїРµСЂРІС‹Рµ РїР°РєРµС‚С‹

    all_measurements = []

    for i, (ref_x, ref_y) in enumerate(REFERENCE_POINTS):
        print(f"[{i+1}/{len(REFERENCE_POINTS)}] РўРѕС‡РєР° P{i+1}: X={ref_x:.1f}, Y={ref_y:.1f}")
        input(f"  РЈСЃС‚Р°РЅРѕРІРё Р¦Р•РќРўР  СЂРѕР±РѕС‚Р° РЅР° СЌС‚Сѓ С‚РѕС‡РєСѓ Рё РЅР°Р¶РјРё Enter...")

        time.sleep(0.5)
        samples = _collect_samples(SAMPLES_PER_POINT)

        if len(samples) < 3:
            print(f"  Р’РќРРњРђРќРР•: РјР°Р»Рѕ РґР°РЅРЅС‹С… ({len(samples)}). РџСЂРѕРїСѓСЃРєР°РµРј.")
            continue

        # РњРµРґРёР°РЅС‹ РґР»СЏ СѓСЃС‚РѕР№С‡РёРІРѕСЃС‚Рё Рє РІС‹Р±СЂРѕСЃР°Рј
        def median(lst): return sorted(lst)[len(lst) // 2]

        avg = {
            "ref_x": ref_x,
            "ref_y": ref_y,
            "pos_x": median([s["pos_x"] for s in samples]),
            "pos_y": median([s["pos_y"] for s in samples]),
            "range_l": median([s["range_l"] for s in samples]),
            "range_r": median([s["range_r"] for s in samples]),
            "n_samples": len(samples),
            "raw": samples,
        }
        all_measurements.append(avg)

        # True distances to diagonal working-area anchors.
        import math
        AL_X, AL_Y = 0.0, 0.0
        AR_X, AR_Y = 29.77, 16.97
        true_l = math.hypot(ref_x - AL_X, ref_y - AL_Y)
        true_r = math.hypot(ref_x - AR_X, ref_y - AR_Y)

        err_l = avg["range_l"] - true_l
        err_r = avg["range_r"] - true_r
        err_x = avg["pos_x"] - ref_x
        err_y = avg["pos_y"] - ref_y

        print(f"  РР·РјРµСЂРµРЅРѕ: L={avg['range_l']:.3f} Рј (РѕС€РёР±РєР° {err_l:+.3f} Рј)")
        print(f"  РР·РјРµСЂРµРЅРѕ: R={avg['range_r']:.3f} Рј (РѕС€РёР±РєР° {err_r:+.3f} Рј)")
        print(f"  РџРѕР·РёС†РёСЏ: ({avg['pos_x']:.2f}, {avg['pos_y']:.2f}) "
              f"в†’ РѕС€РёР±РєР° ({err_x:+.2f}, {err_y:+.2f}) Рј\n")

    if _ws_app:
        _ws_app.send("CAL_STOP")
        _ws_app.close()

    if not all_measurements:
        print("РќРµС‚ РґР°РЅРЅС‹С…. Р’С‹С…РѕРґРёРј.")
        sys.exit(1)

    with open(OUTPUT_FILE, "w", encoding="utf-8") as f:
        json.dump({
            "anchors": {
                "L": {"x": 0.0, "y": 0.0},
                "R": {"x": 29.77, "y": 16.97},
            },
            "playable_court": {"x": 3.0, "y": 3.0, "w": 23.77, "h": 10.97},
            "working_area": {"w": 29.77, "h": 16.97},
            "points": all_measurements,
        }, f, indent=2, ensure_ascii=False)

    print(f"Р”Р°РЅРЅС‹Рµ СЃРѕС…СЂР°РЅРµРЅС‹: {OUTPUT_FILE}")
    print("Р—Р°РїСѓСЃС‚Рё: python analyze.py")


if __name__ == "__main__":
    main()
