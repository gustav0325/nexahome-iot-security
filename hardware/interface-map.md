# Mapa de interfaces físicas — NexaSense (ESP32)

> No ambiente de laboratório o dispositivo é emulado no **QEMU**. As interfaces
> físicas abaixo são as do ESP32 real; o QEMU expõe a **UART0** (equivalente aos
> pinos TX/RX da placa) em `tcp://localhost:5555`, o que permite reproduzir o
> risco de um console de diagnóstico acessível. JTAG/SPI/I²C são analisados
> conceitualmente a partir da arquitetura do SoC e do firmware.

A pergunta-guia para cada interface não é "existe o pino?", e sim:
**o que ela permite fazer e qual o risco de permanecer acessível em produção?**

| Interface | Pinos ESP32 (típico) | No protótipo | O que permite | Risco se acessível em produto comercial |
|---|---|---|---|---|
| **UART0** (console) | GPIO1 TX / GPIO3 RX | **Ativa**, console de diagnóstico **sem autenticação** | Ler todo o boot log (com segredos), comandos `config/nvs/ota/factory/reboot` | **Alto** — acesso físico → dump de credenciais, troca de firmware (`ota`), reset de fábrica. Evidência: `hardware/evidence/uart-01-boot-e-console.log` |
| **Strapping / Boot** | GPIO0 (+EN) | Disponível | Entrar em *download mode* do bootloader ROM | **Alto** — habilita leitura/escrita da flash via `esptool` (ver caso Sonoff, CVE-2024-7206) |
| **SPI Flash** | GPIO6–11 (barramento) | Flash externa GD, **sem Flash Encryption** | Ler/clonar/alterar a imagem inteira (chip-off ou in-circuit) | **Alto** — `flash_encryption=OFF` → extração completa do firmware e dos segredos |
| **JTAG** | GPIO12–15 (MTMS/MTDI/MTCK/MTDO) | Exposto no SoC; sem fusível de desabilitação | Debug com halt/step, leitura de RAM/registradores, dump de flash | **Médio/Alto** — sem *Secure Boot*/eFuse de bloqueio, permite controle total do MCU |
| **I²C / Pontos de teste** | GPIO configurável | Barramento do sensor | Injetar/sniffar leituras do sensor | **Baixo/Médio** — afeta integridade do dado ambiental, não expõe segredos |

## Avaliação de exposição

O protótipo **não reduz** a superfície física: UART de diagnóstico ativa,
*download mode* acessível, flash não criptografada e JTAG não bloqueado por eFuse.
Numa versão comercial com acesso físico plausível (dispositivo residencial/comercial
instalado em local acessível), isso habilita a cadeia:

`acesso físico → UART/boot mode → dump da flash → extração de credenciais da frota
→ pivô para o broker MQTT e para a conta de nuvem de outros dispositivos`.

Uma interface acessível **não é automaticamente** vulnerabilidade — o problema aqui
é a **combinação** de interface acessível **+** ausência de proteção (sem autenticação
no console, sem Flash Encryption, sem Secure Boot) **+** segredos de alto impacto
(senha de frota compartilhada) atrás dela.
