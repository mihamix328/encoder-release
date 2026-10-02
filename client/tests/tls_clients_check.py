"""Exercise the actual client and admin cores against an isolated TLS server."""
import argparse
from pathlib import Path
import socket
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser()
parser.add_argument('--server', required=True)
parser.add_argument('--openssl', required=True)
parser.add_argument('--probe', required=True)
parser.add_argument('--ip-san', action='store_true')
args = parser.parse_args()
server, probe, openssl = (str(Path(p).resolve()) for p in (args.server, args.probe, args.openssl))

def free_port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]

with tempfile.TemporaryDirectory(prefix='encoder-clients-') as temporary:
    root = Path(temporary)
    cert, key = root / 'server.crt', root / 'server.key'
    subprocess.run([openssl, 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
                    '-keyout', str(key), '-out', str(cert), '-days', '1',
                    '-subj', '/CN=ip-fixture.invalid' if args.ip_san else '/CN=localhost',
                    '-addext', 'subjectAltName=IP:127.0.0.1' if args.ip_san else 'subjectAltName=DNS:localhost'], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    client_port, admin_port = free_port(), free_port()
    while client_port == admin_port:
        admin_port = free_port()
    config = root / 'server.conf'
    config.write_text(f'listen_host=127.0.0.1\nlisten_port={client_port}\n'
                      f'admin_host=127.0.0.1\nadmin_port={admin_port}\nadmin_token=test-token\n'
                      f'cert_file={cert}\nkey_file={key}\nstorage_dir={root / "storage"}\n')
    subprocess.run([server, '--config', str(config), '--init-user', 'tester', 'test-password'],
                   cwd=root, check=True, stdout=subprocess.DEVNULL)
    with (root / 'server.log').open('w+') as log:
        process = subprocess.Popen([server, '--config', str(config)], cwd=root, stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 10
            while True:
                try:
                    with socket.create_connection(('127.0.0.1', client_port), timeout=1):
                        break
                except OSError:
                    if time.monotonic() >= deadline or process.poll() is not None:
                        raise RuntimeError('Temporary server did not start')
                    time.sleep(0.1)
            subprocess.run([probe, str(cert), str(client_port), str(admin_port), str(root),
                            '127.0.0.1' if args.ip_san else 'localhost'],
                           cwd=root, check=True, timeout=30)
        except Exception:
            log.seek(0)
            print(log.read())
            raise
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
