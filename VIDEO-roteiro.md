# Roteiro do vídeo de demonstração (até 4 min)

Objetivo: comprovar que **você realizou e compreendeu** a análise. Grave a tela
(QEMU + terminal) com narração. Sugestão de cortes/tempo:

### 0:00–0:25 — Abertura e escopo
- "Avaliação de segurança do protótipo **NexaSense** (ESP32) da NexaHome, em
  **laboratório simulado** (QEMU). Nada foi testado fora do laboratório; segredos
  são fictícios."
- Mostrar o diagrama `architecture/architecture-diagram.png`.

### 0:25–0:55 — Dispositivo e superfície de ataque
- Apontar no diagrama: UART/JTAG/Flash (acesso físico), firmware, MQTT/HTTP.
- "Secure Boot e Flash Encryption **desligados** no protótipo."

### 0:55–1:35 — Evidência de firmware (F02) + preservação
- Mostrar `firmware/hashes.txt` (SHA-256 — preservação/rastreabilidade).
- Rodar `strings nexasense-...app.bin | grep -E 'Nx-Shared|nxh_live|NexaLab'`
  → segredos em texto claro saindo sem descompilação.

### 1:35–2:05 — Evidência de hardware (F01)
- Iniciar o device no QEMU e conectar à UART:
  `python3 lab/uart_client.py /tmp/demo.log 20 "5:config"`
- Mostrar o **boot log com os segredos** e o console `config` devolvendo tudo
  **sem autenticação**.

### 2:05–2:40 — Evidência de comunicação (F03)
- Abrir `network/protocol-analysis.md` **ou** rodar
  `tshark -r network/evidence/capture-01-operacao-normal.pcap -Y mqtt.msgtype==1 -V | grep -i 'User Name\|Password'`
- "Usuário e senha MQTT trafegam em **texto claro**; é uma senha de **frota**."

### 2:40–3:20 — Demonstração controlada A (F04): injeção de comando
- Com o device rodando, publicar `{"cmd":"get_config"}` no tópico `/cmd` e mostrar
  a resposta em `/resp` com **toda a config** (arquivo `demo/evidence/demoA-mqtt-cmd-injection.txt`).
- "Qualquer um com a credencial de frota comanda qualquer dispositivo."

### 3:20–3:50 — Demonstração controlada B (F05): OTA sem assinatura
- Mostrar `demo/evidence/demoB-ota-uart.log`:
  *"OTA: imagem aceita SEM verificar assinatura. Reiniciando..."* e o boot em `ota_0`.
- "A imagem veio de um servidor do **atacante** e foi aceita — execução remota de código."

### 3:50–4:00 — Riscos e hardening
- Tabela de `findings/findings.md` (F05 crítico; F01–F04/F07 alto).
- Top 3 hardening: **Secure Boot + OTA assinado**, **mTLS + credencial por device**,
  **Flash Encryption + remover segredos/console**.

> Dica: não é preciso mostrar todas as ferramentas. Mostre 1 evidência de cada
> pilar (firmware, hardware, comunicação) + as 2 demos. Publique no YouTube
> **não listado** e cole o link aqui e no `README.md`.
