#!/bin/bash
# Executa o NexaSense no QEMU (Espressif) com:
#  - UART0 exposta em tcp://localhost:5555 (equivale aos pinos TX/RX da placa)
#  - rede emulada (OpenETH) via slirp; host = 10.0.2.2 ; HTTP do device -> localhost:8080
#  - captura de todo o trafego do dispositivo em network/evidence/<nome>.pcap
#  - eFuses emulados em lab/efuse.bin
# uso: run_qemu.sh <flash.bin> <nome_pcap> [download]   (download = modo bootloader UART)
R=$(cd "$(dirname "$0")/.." && pwd)
FLASH=$1; PCAP=$2; MODE=$3
[ -f $R/lab/efuse.bin ] || dd if=/dev/zero of=$R/lab/efuse.bin bs=1 count=124 2>/dev/null
STRAP=""
[ "$MODE" = "download" ] && STRAP="-global driver=esp32.gpio,property=strap_mode,value=0x0f"
exec qemu-system-xtensa -nographic -machine esp32 \
  -drive file=$FLASH,if=mtd,format=raw \
  -drive file=$R/lab/efuse.bin,if=none,format=raw,id=efuse \
  -global driver=nvram.esp32.efuse,property=drive,value=efuse \
  -netdev user,id=net0,hostfwd=tcp::8080-:80 \
  -net nic,model=open_eth,netdev=net0 \
  -object filter-dump,id=f0,netdev=net0,file=$R/network/evidence/$PCAP.pcap \
  -serial tcp::5555,server=on,wait=off -monitor none $STRAP
