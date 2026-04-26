from ssh_helper import run_command

cmds = [
    ('/home/pi/tennis/venv/bin/python3 -c "import ultralytics; print(ultralytics.__version__)" 2>&1', 'ultralytics'),
    ('/home/pi/tennis/venv/bin/python3 -c "import cv2; print(cv2.__version__)" 2>&1', 'cv2'),
    ('/home/pi/tennis/venv/bin/python3 -c "import numpy; print(numpy.__version__)" 2>&1', 'numpy'),
    ('/home/pi/tennis/venv/bin/python3 -c "import scipy; print(scipy.__version__)" 2>&1', 'scipy'),
    ('python3 -c "from picamera2 import Picamera2; print(\'picamera2 OK\')" 2>&1', 'picamera2'),
]
for cmd, pkg in cmds:
    out, err, code = run_command(cmd, verbose=False)
    print(f'{pkg}: {(out.strip() or err.strip())[:80]}')
