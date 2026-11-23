# Simulação online no Wokwi (alternativa em navegador)

Além do QEMU local (usado para coletar as evidências deste repositório), o
firmware pode ser executado no simulador **online Wokwi** (https://wokwi.com),
útil para demonstração em vídeo sem hardware.

## Opção A — projeto ESP-IDF no Wokwi
1. Crie um novo projeto **ESP-IDF** em https://wokwi.com/projects/new/esp32-idf
2. Substitua `main/main.c` pelo firmware em `../src/main/main.c` e crie
   `main/secrets.h` a partir de `../src/main/secrets.example.h`.
3. Cole o conteúdo de `diagram.json` deste diretório (ESP32 + DHT22).
4. Compile e rode. O **Serial Monitor** do Wokwi mostra o mesmo boot log com os
   segredos (evidência de F01/F02).

## Opção B — rodar o binário já compilado
Os arquivos `nexasense.bin` e `nexasense.elf` deste diretório podem ser usados com
`wokwi.toml` na extensão **Wokwi for VS Code**, apontando para este projeto.

## Observações
- No Wokwi o enlace é **Wi-Fi** (Gateway virtual do Wokwi dá acesso à Internet do
  simulador); no QEMU local usamos Ethernet emulada (OpenETH) para capturar o pcap.
- Para a captura de tráfego MQTT/HTTP e as demos controladas (A e B), use o
  ambiente QEMU local descrito no README principal — ele isola 100% o tráfego no
  laboratório.
