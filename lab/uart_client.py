#!/usr/bin/env python3
"""Cliente da UART0 emulada (equivale a um adaptador USB-TTL nos pinos TX/RX).
Registra toda a saida em arquivo e, opcionalmente, envia comandos.
uso: uart_client.py <log> <duracao_s> [atraso:comando ...]
ex.: uart_client.py boot.log 30 20:help 22:config
"""
import socket, sys, time
log, dur = sys.argv[1], float(sys.argv[2])
cmds = [(float(a.split(':', 1)[0]), a.split(':', 1)[1]) for a in sys.argv[3:]]
for _ in range(50):
    try:
        s = socket.create_connection(('127.0.0.1', 5555)); break
    except OSError:
        time.sleep(0.2)
s.settimeout(0.2); t0 = time.time(); buf = b''
while time.time() - t0 < dur:
    while cmds and time.time() - t0 >= cmds[0][0]:
        s.sendall(cmds.pop(0)[1].encode() + b'\r')
    try:
        d = s.recv(4096)
        if not d: break
        buf += d
    except socket.timeout:
        pass
open(log, 'wb').write(buf.replace(b'\r\n', b'\n'))
