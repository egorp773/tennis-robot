#!/usr/bin/env python3
"""Запустить команду на Pi без проблем с кодировкой."""
import sys
import os

# Устанавливаем UTF-8 для stdout
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8')
os.environ['PYTHONIOENCODING'] = 'utf-8'

sys.path.insert(0, os.path.dirname(__file__))
from ssh_helper import run_command, upload_content

if __name__ == '__main__':
    cmd = ' '.join(sys.argv[1:])
    out, err, code = run_command(cmd)
    sys.exit(code)
