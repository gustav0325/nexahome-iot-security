# Análise do firmware — NexaSense v1.0.0-proto

Ambiente: **laboratório QEMU (Espressif) + ESP-IDF v5.3.2**, dispositivo emulado
ESP32. Nenhum teste foi executado contra dispositivos de terceiros, serviços
externos ou redes de produção.

## 1. Preservação e integridade (rastreabilidade da evidência)

A imagem original foi preservada **antes** de qualquer modificação e colocada em
somente-leitura. A análise é feita sobre uma **cópia de trabalho**. Registro em
`firmware/hashes.txt`:

| Arquivo | Tamanho | SHA-256 |
|---|---|---|
| `nexasense-v1.0.0-proto_app.bin` (imagem do app) | 630.784 B | `3d31f8b17303c4872e549504fcd4d2f77e3f8b72fdfb76db69848409cc78fecc` |
| `nexasense-v1.0.0-proto_flash4MB.bin` (flash 4 MB completo) | 4.194.304 B | `a5ec33c36204cfb82893eae198594e5d2c22df4a3fb1ad7336013850e2144505` |

Fluxo: `firmware original (ro)` → `SHA-256 registrado` → `cópia de trabalho` → `análise`.

## 2. Identificação do formato e estrutura da imagem

`esptool.py image_info` sobre a imagem do app:

- Formato: **ESP32 application image** (magic `0xE9`), *image version 1*.
- **5 segmentos**: DROM (`0x3f400020`), DRAM, IROM (`0x400d0020`), DRAM, IRAM.
- `Checksum: valid` e `Validation Hash: valid` — a imagem possui **verificação de
  integridade (checksum/SHA-256 do formato)**, mas isso **não é verificação de
  autenticidade**: qualquer um pode gerar uma imagem válida. Não há *Secure Boot*
  (ver F05).

## 3. Tabela de partições (extraída do flash, offset `0x8000`)

```
nvs       data nvs     0x9000   16K
otadata   data ota     0xd000    8K
phy_init  data phy     0xf000    4K
factory   app  factory 0x10000   1M
ota_0     app  ota_0   0x110000  1M
ota_1     app  ota_1   0x210000  1M
```

Esquema **factory + 2× OTA** → confirma que o ciclo de manutenção prevê
atualização de firmware (relevante para F05).

## 4. Informações sensíveis encontradas na imagem (texto claro)

Extração trivial com `strings` sobre a imagem **sem necessidade de descompilação**:

| Tipo | Valor encontrado |
|---|---|
| SSID Wi-Fi | `NexaHome-Lab` |
| Senha Wi-Fi | `NexaLab#2026!wifi` |
| Senha MQTT (frota, compartilhada) | `Nx-Shared-Fleet-Pass-2026` |
| Token de API | `nxh_live_7f3a9c21e8b04d5f9a6c` |
| Endpoint de nuvem | `http://api.nexahome.lab/v1/telemetry` |
| Tópicos MQTT | `nexahome/sense/<id>/{telemetry,cmd,resp}` |
| Endpoints HTTP locais | `GET /status`, `GET /config` (sem autenticação) |
| Marca de build de depuração | `DEBUG BUILD - diagnostico habilitado` |

> Esses mesmos valores também são impressos no log de inicialização da UART e
> retornados pelos endpoints `/config` (HTTP) e `config`/`get_config` (console e MQTT).

## 5. Mecanismo de atualização (OTA)

- OTA acionado por comando (MQTT `cmd=ota` ou console `ota <url>`).
- Transferência por **HTTP em texto claro** (sem TLS).
- **Nenhuma verificação de assinatura/origem** da imagem — apenas o checksum de
  formato. Com *Secure Boot* desabilitado, uma imagem bem-formada de **qualquer
  origem** é aceita e passa a ser o app de boot. Demonstrado de forma controlada
  em `demo/evidence/demoB-*` (F05).

## 6. Estado dos mecanismos do próprio MCU

Lido via console `info` (e confirmado no boot):

```
secure_boot=OFF    flash_encryption=OFF
```

→ Flash legível/clonável e firmware substituível (base para F01, F02 e F05).

## Ferramentas
`esptool.py image_info` · `gen_esp32part.py` · `strings` · `sha256sum`.
