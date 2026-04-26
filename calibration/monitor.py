#!/usr/bin/env python3
"""
Мониторинг UWB в реальном времени.
Подключись к Wi-Fi TennisRobot, запусти этот скрипт.
Он покажет сырые дальности и позицию в таблице.
"""
import sys
import threading
import time

try:
    import websocket
except ImportError:
    print("pip install websocket-client")
    sys.exit(1)

WS_URL = "ws://192.168.4.1:81/ws"

_range_l = "---"
_range_r = "---"
_pos_x   = "---"
_pos_y   = "---"
_state   = "---"
_lock    = threading.Lock()


def on_message(ws_app, msg):
    global _range_l, _range_r, _pos_x, _pos_y, _state
    with _lock:
        if msg.startswith("RANGE,L,"):
            _range_l = msg.split(",")[2]
        elif msg.startswith("RANGE,R,"):
            _range_r = msg.split(",")[2]
        elif msg.startswith("POS,"):
            p = msg.split(",")
            _pos_x, _pos_y = p[1], p[2]
        elif msg.startswith("STATE,"):
            _state = msg[6:]


def on_open(ws_app):
    ws_app.send("CAL_RAW")


def on_error(ws_app, e): pass
def on_close(ws_app, *a): pass


def main():
    print("Подключение к роботу...")
    ws_app = websocket.WebSocketApp(
        WS_URL,
        on_open=on_open,
        on_message=on_message,
        on_error=on_error,
        on_close=on_close,
    )
    t = threading.Thread(target=ws_app.run_forever, daemon=True)
    t.start()
    time.sleep(2.0)

    print("Мониторинг. Ctrl+C для выхода.\n")
    try:
        while True:
            with _lock:
                rl, rr, px, py, st = _range_l, _range_r, _pos_x, _pos_y, _state
            print(
                f"\r  Маяк L: {rl:>7} м    Маяк R: {rr:>7} м    "
                f"Позиция: ({px}, {py})    Статус: {st:<12}",
                end="", flush=True
            )
            time.sleep(0.3)
    except KeyboardInterrupt:
        ws_app.send("CAL_STOP")
        ws_app.close()
        print("\nВыход.")


if __name__ == "__main__":
    main()
