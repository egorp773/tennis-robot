#!/usr/bin/env python3
"""SSH helper для работы с Raspberry Pi через paramiko."""
import paramiko
import sys
import time
import socket
import os
import getpass

HOST = os.environ.get("TENNIS_PI_HOST", "raspberrypi.local")
USER = os.environ.get("TENNIS_PI_USER", "pi")
PASSWORD = os.environ.get("TENNIS_PI_PASSWORD")
VENV = "/home/pi/tennis/venv/bin/python3"
VENV_PIP = "/home/pi/tennis/venv/bin/pip"

def get_client():
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    password = PASSWORD or getpass.getpass("TENNIS_PI_PASSWORD: ")
    client.connect(HOST, username=USER, password=password, timeout=15,
                   banner_timeout=30, auth_timeout=30)
    return client

def run_command(cmd, timeout=300, verbose=True):
    """Выполнить команду с потоковым выводом."""
    client = get_client()
    transport = client.get_transport()
    transport.set_keepalive(30)

    channel = transport.open_session()
    channel.settimeout(timeout)
    channel.exec_command(cmd)

    out_parts = []
    err_parts = []

    start = time.time()
    while True:
        if time.time() - start > timeout:
            channel.close()
            client.close()
            raise TimeoutError(f"Command timed out after {timeout}s: {cmd[:80]}")

        # Читаем stdout
        if channel.recv_ready():
            data = channel.recv(4096).decode("utf-8", errors="replace")
            out_parts.append(data)
            if verbose:
                sys.stdout.buffer.write(data.encode("utf-8"))
                sys.stdout.flush()

        # Читаем stderr
        if channel.recv_stderr_ready():
            data = channel.recv_stderr(4096).decode("utf-8", errors="replace")
            err_parts.append(data)
            if verbose:
                sys.stdout.buffer.write(data.encode("utf-8"))
                sys.stdout.flush()

        # Проверяем завершение
        if channel.exit_status_ready():
            # Дочитываем остатки
            while channel.recv_ready():
                data = channel.recv(4096).decode("utf-8", errors="replace")
                out_parts.append(data)
                if verbose:
                    sys.stdout.buffer.write(data.encode("utf-8"))
                    sys.stdout.flush()
            while channel.recv_stderr_ready():
                data = channel.recv_stderr(4096).decode("utf-8", errors="replace")
                err_parts.append(data)
                if verbose:
                    sys.stdout.buffer.write(data.encode("utf-8"))
                    sys.stdout.flush()
            break

        time.sleep(0.1)

    exit_code = channel.recv_exit_status()
    channel.close()
    client.close()

    return "".join(out_parts), "".join(err_parts), exit_code

def run_bg(cmd, marker="/tmp/cmd_done.marker"):
    """Запустить команду в фоне, записать статус в маркер-файл."""
    wrapped = f"( {cmd} ) > /tmp/bg_out.log 2>&1; echo $? > {marker}"
    client = get_client()
    client.exec_command(f"nohup bash -c '{wrapped}' &")
    client.close()

def check_bg(marker="/tmp/cmd_done.marker"):
    """Проверить завершение фонового процесса. Вернуть (done, exit_code, output)."""
    client = get_client()
    _, stdout, _ = client.exec_command(f"cat {marker} 2>/dev/null")
    marker_content = stdout.read().decode().strip()

    _, stdout2, _ = client.exec_command("cat /tmp/bg_out.log 2>/dev/null | tail -20")
    log_content = stdout2.read().decode()
    client.close()

    if marker_content.isdigit():
        return True, int(marker_content), log_content
    return False, None, log_content

def upload_file(local_path, remote_path):
    client = get_client()
    sftp = client.open_sftp()
    sftp.put(local_path, remote_path)
    sftp.close()
    client.close()
    print(f"Загружен {local_path} -> {remote_path}")

def upload_content(content, remote_path):
    """Загрузить строку как файл на Pi."""
    client = get_client()
    sftp = client.open_sftp()
    with sftp.open(remote_path, 'wb') as f:
        f.write(content.encode('utf-8'))
    sftp.close()
    client.close()

if __name__ == "__main__":
    if len(sys.argv) > 1:
        cmd = " ".join(sys.argv[1:])
        out, err, code = run_command(cmd)
        sys.exit(code)
