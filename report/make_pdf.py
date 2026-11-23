#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import base64, markdown, pathlib
from weasyprint import HTML

R = pathlib.Path("/home/claude/nexahome-iot-security")
img = base64.b64encode((R/"architecture/architecture-diagram.png").read_bytes()).decode()

body_md = r"""
# Relatório Técnico — Avaliação de Segurança do Protótipo NexaSense

**Cliente:** NexaHome Technologies &nbsp;•&nbsp; **Dispositivo:** NexaSense v1.0.0-proto (ESP32)
**Papel:** IoT Security Analyst &nbsp;•&nbsp; **Data:** 22/11/2026 &nbsp;•&nbsp; **Ambiente:** laboratório simulado (QEMU/Espressif + Wokwi)

> **Escopo e autorização.** Toda a avaliação foi realizada **exclusivamente** em
> ambiente de laboratório simulado, sobre o **firmware educacional** do dispositivo.
> Nenhum teste foi executado contra dispositivos de terceiros, serviços externos ou
> redes de produção. Todos os valores sensíveis (senhas, tokens, SSID) são
> **fictícios** e locais. Mecanismos irreversíveis de hardware (eFuses) **não** foram
> gravados — são propostos conceitualmente.

## 1. Descrição do dispositivo analisado
O **NexaSense** é um sensor ambiental baseado em **ESP32** que coleta temperatura e
umidade e transmite os dados por rede sem fio a uma plataforma de monitoramento. O
protótipo (ESP-IDF v5.3.2) publica telemetria via **MQTT**, expõe um servidor
**HTTP** local, mantém um **console de diagnóstico** na UART0 e suporta atualização
**OTA** por HTTP. Partições em flash de 4 MB: `factory + ota_0 + ota_1 + NVS`.

## 2. Diagrama simplificado da arquitetura
<img src="data:image/png;base64,__IMG__" style="width:100%;max-width:560px;display:block;margin:6px auto"/>

**Principais componentes:** MCU ESP32 (Wi-Fi), sensores T/umidade, SPI Flash 4 MB
(sem Flash Encryption), UART0/console, JTAG/boot mode, firmware sem Secure Boot;
externamente: broker MQTT (credencial de frota), servidor OTA HTTP, plataforma de nuvem.

## 3. Mapa da superfície de ataque
| Camada | Pontos de exposição | Achados |
|---|---|---|
| Hardware / acesso físico | UART0 (console), boot/download mode, SPI Flash, JTAG | F01, F07 |
| Firmware | segredos embutidos, imagem sem assinatura, Secure Boot/Flash Enc OFF | F02, F05, F07 |
| Comunicação | MQTT texto claro + credencial de frota; HTTP `/config` sem auth; comando sem autorização | F03, F04, F06 |
| Lógica / estado | factory reset e reconfiguração sem autenticação | F08 |

## 4. Metodologia
Avaliação estruturada relacionando **Hardware + Firmware + Comunicação + Acesso
físico → Superfície de ataque → Riscos → Mitigações**, apoiada em OWASP ISTG
(planejamento e organização dos testes), NIST IR 8259A (linha de base de capacidades),
ETSI EN 303 645 V3.1.3 (requisitos de hardening), MITRE EMB3D (ameaças a dispositivos
embarcados) e ESP-IDF Security (mecanismos do próprio MCU). Etapas: (1) reconhecimento;
(2) **preservação e hash** do firmware; (3) análise estática do firmware; (4) análise
das interfaces físicas; (5) análise das comunicações (captura pcap); (6) **demonstrações
controladas**; (7) mapeamento de riscos; (8) plano de hardening. Ferramentas: QEMU
(`qemu-system-xtensa`), ESP-IDF (`esptool image_info`, `gen_esp32part.py`), mosquitto,
tshark, `strings`, `sha256sum`.

## 5. Análise do firmware
Imagem **preservada em somente-leitura** e analisada em cópia de trabalho
(rastreabilidade). SHA-256 registrados:

| Arquivo | Tamanho | SHA-256 |
|---|---|---|
| app.bin | 630.784 B | `3d31f8b1…cc78fecc` |
| flash 4 MB | 4.194.304 B | `a5ec33c3…e2144505` |

Formato ESP32 válido (5 segmentos; checksum válido — **integridade, não
autenticidade**). `strings` recuperou, **sem descompilação**, todos os segredos em
texto claro: senha Wi-Fi, **senha MQTT de frota compartilhada**, **token de API**,
endpoint de nuvem e OTA URL. Console `info`: `secure_boot=OFF`, `flash_encryption=OFF`.

## 6. Análise das interfaces físicas
A pergunta aplicada a cada interface foi *"o que permite e qual o risco de permanecer
acessível em produção?"*. **UART0** mantém um console de diagnóstico **sem
autenticação** (lê o boot log com segredos e executa `config/ota/factory`). **Boot/
download mode** e **JTAG** não têm bloqueio por eFuse; a **flash** não é criptografada
(leitura/clonagem). Uma interface acessível não é, por si só, vulnerabilidade — o
risco vem da **combinação** interface acessível + ausência de proteção + segredos de
alto impacto por trás dela.

## 7. Análise das comunicações
No pcap de operação normal, o **MQTT CONNECT** expõe `user=nexasense` e
`pass=Nx-Shared-Fleet-Pass-2026`, e cada mensagem de telemetria carrega o `token` —
tudo **sem TLS**. O endpoint **HTTP `/config`** devolve a configuração completa sem
autenticação. Identificar o protocolo (MQTT) é diferente de concluir que seu uso é
seguro: aqui, o uso **não** é seguro (sem confidencialidade, sem autenticação por
dispositivo, credencial compartilhada entre toda a frota).

## 8. Demonstrações controladas
**Demo A — injeção de comando MQTT sem autorização.** Publicando
`{"cmd":"get_config"}` no tópico `nexahome/sense/NXS-000000/cmd` com a credencial de
frota, o dispositivo respondeu em `/resp` com **toda a configuração** (340 bytes,
incluindo token). Reproduzível e registrado em `demo/evidence/demoA-*` e `capture-02`.

**Demo B — OTA sem verificação de assinatura.** Um comando OTA apontando para um
servidor sob controle do atacante (`http://10.0.2.2:8000/nexasense.bin`) levou o
dispositivo a registrar *"OTA: imagem aceita SEM verificar assinatura. Reiniciando…"*
e, no reboot, a executar a partição `ota_0` com a imagem fornecida — **execução
remota de código persistente**. Registrado em `demo/evidence/demoB-*` e `capture-03`.

## 9. Riscos identificados e classificação
Risco = f(Impacto, Probabilidade).

| ID | Achado | Evidência | Impacto | Probab. | Risco | Recomendação |
|----|--------|-----------|---------|---------|-------|--------------|
| **F01** | Console UART de diagnóstico sem autenticação | uart-01 log | Alto | Alta | **Alto** | Desativar/proteger console em produção |
| **F02** | Segredos em texto claro no firmware | analysis-notes §4 | Alto | Alta | **Alto** | Provisionar credenciais por dispositivo; Flash Encryption |
| **F03** | MQTT em claro + credencial de frota | capture-01 | Alto | Média | **Alto** | TLS/mTLS + credencial única |
| **F04** | Comando MQTT sem autorização | demoA / capture-02 | Alto | Média | **Alto** | ACL por client_id; autorizar comandos |
| **F05** | OTA HTTP **sem assinatura** | demoB / capture-03 | Crítico | Média | **Alto** | Assinatura + Secure Boot + OTA/TLS |
| **F06** | `/config` HTTP sem autenticação | http_config.txt | Alto | Média | **Médio/Alto** | Remover `/config`; reduzir superfície |
| **F07** | Secure Boot / Flash Encryption OFF | console info | Alto | Alta | **Alto** | Habilitar ambos na produção |
| **F08** | Factory reset / reconfig sem auth | console | Médio | Média | **Médio** | Autenticar comandos de estado |

## 10. Relação dos achados com referências reconhecidas
- **MITRE EMB3D:** F02/F05 correspondem a *extração/modificação de firmware* e
  *interfaces privilegiadas expostas* (F01).
- **ETSI EN 303 645 V3.1.3:** F03 → 5.5 (comunicação segura); F02/F06 → 5.4
  (armazenamento seguro de parâmetros) e 5.6 (reduzir superfície); F05 → 5.3
  (atualização segura de software); F07 complementa todas.
- **NIST IR 8259A:** F01/F08 → *controle de acesso às interfaces*; F02/F03 →
  *proteção de dados*; F05 → *atualização de software*.
- **ESP-IDF Security:** F05/F07 → Secure Boot v2 e Flash Encryption (funções
  **complementares** na proteção do software do dispositivo).

## 11. Recomendações de mitigação
1. **Assinatura de firmware + Secure Boot v2** e **OTA assinado sobre HTTPS** com
   rollback/partição de recuperação (fecha F05/F07).
2. **mTLS no MQTT + credencial única por dispositivo** e **ACL por client_id**
   (fecha F03/F04).
3. **Flash Encryption**, **segredos fora da imagem**, **desativar console e `/config`
   em produção** (fecha F01/F02/F06).

## 12. Proposta de hardening para a versão comercial
Prioridade 1 (comprometimento remoto/total): assinatura+Secure Boot, OTA assinado/
HTTPS, mTLS+credencial por device. Prioridade 2 (superfície/segredos): Flash
Encryption, provisionamento individual, console off, remover `/config`. Prioridade 3
(robustez/hardware): reduzir logs de debug, desabilitar JTAG por eFuse, versionamento
anti-rollback e estratégia de recuperação. Detalhe em `remediation/hardening-plan.md`.

## 13. Exemplo real pesquisado — Sonoff / ESP32 (CVE-2024-7206)
Em um dispositivo *smart home* Sonoff baseado em ESP32, um pesquisador colocou o MCU
em *download mode* pelos pinos de boot e, com `esptool`, **extraiu toda a flash**;
a ausência de Secure Boot e Flash Encryption permitiu analisar, **modificar e
re-gravar** o firmware (CVE-2024-7206). **Lição para a NexaHome:** é a mesma cadeia
demonstrada no NexaSense — interface física acessível + flash sem criptografia +
firmware sem assinatura → extração de firmware → coleta de credenciais →
comprometimento em escala. Com acesso físico plausível a um produto residencial, as
proteções da Espressif deixam de ser opcionais. (Refs.: Jerin Sunny, 2025; Vasile,
Oswald & Chothia, Univ. Birmingham, CARDIS 2018.)

---
*Relatório para fins educacionais (UniFECAF — IoT Security). Evidências completas e
scripts reproduzíveis no repositório `nexahome-iot-security/`. Vídeo: _<inserir link>_.*
"""

html = markdown.markdown(body_md.replace("__IMG__", img), extensions=["tables"])
css = """
@page { size: A4; margin: 1.6cm 1.7cm; @bottom-center { content: "NexaSense — Avaliação de Segurança  ·  pág. " counter(page) "/" counter(pages); font-size:8pt; color:#888; } }
* { font-family: 'Helvetica Neue', Arial, sans-serif; }
body { font-size: 9.6pt; line-height: 1.42; color:#1a1a1a; }
h1 { font-size: 17pt; color:#7b241c; border-bottom:3px solid #c0392b; padding-bottom:5px; }
h2 { font-size: 12pt; color:#922b21; margin-top:15px; border-bottom:1px solid #e0b4b0; padding-bottom:2px; }
blockquote { background:#fdf2f2; border-left:4px solid #c0392b; margin:8px 0; padding:6px 12px; font-size:9pt; color:#5b2c2c; }
table { border-collapse:collapse; width:100%; font-size:8.3pt; margin:6px 0; }
th,td { border:1px solid #d9b3af; padding:4px 6px; text-align:left; vertical-align:top; }
th { background:#f7dcd9; color:#7b241c; }
code { background:#f3f3f3; padding:0 3px; border-radius:3px; font-family:'Courier New',monospace; font-size:8.6pt; }
strong { color:#000; }
"""
HTML(string=f"<html><head><meta charset='utf-8'><style>{css}</style></head><body>{html}</body></html>").write_pdf(str(R/"report/Relatorio-Tecnico-NexaSense.pdf"))
print("PDF gerado")
