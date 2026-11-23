# Achados — Avaliação de Segurança NexaSense

Cada achado segue a cadeia **Evidência → Fragilidade → Ameaça → Impacto → Risco → Mitigação**.
Risco = f(Impacto, Probabilidade). Referências: OWASP ISTG, NIST IR 8259A,
ETSI EN 303 645 V3.1.3, MITRE EMB3D, ESP-IDF Security.

## Tabela-resumo

| ID | Achado | Evidência | Impacto | Probab. | Risco | Recomendação |
|----|--------|-----------|---------|---------|-------|--------------|
| **F01** | Console de diagnóstico na UART sem autenticação | `hardware/evidence/uart-01-boot-e-console.log` | Alto | Alta | **Alto** | Desativar/proteger console em produção; remover build de debug |
| **F02** | Segredos embutidos em texto claro no firmware | `firmware/analysis-notes.md` §4 | Alto | Alta | **Alto** | Provisionar credenciais individuais fora da imagem; Flash Encryption |
| **F03** | Credencial MQTT de **frota compartilhada** e token em texto claro na rede | `network/protocol-analysis.md` §1–2; `capture-01` | Alto | Média | **Alto** | TLS (mTLS) + credencial única por dispositivo |
| **F04** | Canal de comando MQTT sem autorização por dispositivo | `demo/evidence/demoA-mqtt-cmd-injection.txt`; `capture-02` | Alto | Média | **Alto** | ACL por `client_id`; autorização de comandos; assinar comandos |
| **F05** | OTA via HTTP **sem verificação de assinatura** | `demo/evidence/demoB-ota-uart.log` + `demoB-ota-server.log`; `capture-03` | Crítico | Média | **Alto** | Assinatura de firmware + Secure Boot + OTA sobre TLS com pinning |
| **F06** | Endpoint HTTP local `/config` expõe toda a configuração sem autenticação | `network/evidence/http_config.txt` | Alto | Média | **Médio/Alto** | Remover `/config`; autenticar `/status`; reduzir superfície |
| **F07** | Secure Boot e Flash Encryption desabilitados | console `info`; boot log | Alto | Alta | **Alto** | Habilitar Secure Boot v2 + Flash Encryption na versão de produção |
| **F08** | Comando de *factory reset* e reconfiguração exposto sem autenticação | console `factory`/`interval` | Médio | Média | **Médio** | Exigir autenticação para comandos de estado |

---

## Detalhamento

### F01 — Console de diagnóstico UART sem autenticação
- **Evidência:** no boot, o firmware anuncia `DEBUG BUILD - diagnostico habilitado`
  e abre `nexa>`; os comandos `help/info/config/nvs/ota/interval/reboot/factory`
  respondem sem qualquer senha. `config` devolve o JSON completo com Wi-Fi, MQTT,
  token e OTA URL.
- **Fragilidade:** interface de depuração privilegiada ativa em build de produção.
- **Ameaça (MITRE EMB3D):** interface privilegiada exposta / acesso via interface de hardware.
- **Impacto:** com acesso físico (UART TX/RX), o atacante lê segredos e **troca o
  firmware** (`ota`). **Alto.**
- **Mitigação:** compilar produção sem o console; se necessário para RMA, exigir
  autenticação forte e desabilitar por eFuse/flag. NIST IR 8259A: *controle de
  acesso às interfaces*. ETSI 5.6 (reduzir superfície).

### F02 — Segredos em texto claro na imagem de firmware
- **Evidência:** `strings` recupera `NexaLab#2026!wifi`, `Nx-Shared-Fleet-Pass-2026`,
  `nxh_live_7f3a9c21e8b04d5f9a6c` sem descompilação.
- **Ameaça (EMB3D):** extração de firmware via interface de hardware → segredos.
- **Impacto:** uma única extração (flash dump ou `/config`) compromete **toda a
  frota** (senha compartilhada). **Alto.**
- **Mitigação:** segredos fora da imagem (provisionamento por dispositivo em NVS
  criptografada); Flash Encryption; ETSI 5.4 (armazenamento seguro de parâmetros).

### F03 — MQTT em texto claro + credencial de frota
- **Evidência:** no pcap, o CONNECT expõe `user=nexasense pass=Nx-Shared-Fleet-Pass-2026`
  e cada telemetria carrega o `token` — tudo sem TLS.
- **Impacto:** quem escuta a rede captura a credencial que vale para **todos** os
  dispositivos. **Alto.**
- **Mitigação:** TLS/mTLS no MQTT; credencial única por dispositivo; rotação. ETSI 5.5
  (comunicação segura); NIST *proteção de dados*.

### F04 — Comando MQTT sem autorização
- **Evidência (demo controlada):** publicando `{"cmd":"get_config"}` em
  `nexahome/sense/NXS-000000/cmd` com a credencial de frota, o dispositivo respondeu
  em `/resp` com **toda a configuração** (340 bytes). O broker registrou a entrega.
- **Impacto:** qualquer detentor da credencial de frota comanda qualquer dispositivo
  (reboot, interval, get_config, **ota**). **Alto.**
- **Mitigação:** ACL por `client_id` no broker; autorização/assinatura de comandos;
  separar credencial de publicação da de controle.

### F05 — OTA sem assinatura (crítico)
- **Evidência (demo controlada):** comando OTA apontando para servidor do atacante
  (`http://10.0.2.2:8000/nexasense.bin`); o firmware registrou
  *"OTA: imagem aceita SEM verificar assinatura. Reiniciando..."* e, no reboot,
  passou a executar a partição `ota_0` com a imagem fornecida pelo atacante.
- **Ameaça (EMB3D):** modificação/substituição de firmware.
- **Impacto:** execução remota de código persistente no dispositivo — **crítico**.
- **Mitigação:** assinatura de firmware (ESP-IDF Secure Boot v2 — verifica a
  assinatura RSA/ECDSA da imagem antes de executar) **+** OTA sobre HTTPS com
  validação de certificado. ETSI 5.3-7 (criptografia para atualização segura).

### F06 — `/config` HTTP sem autenticação
- **Evidência:** `GET /config` devolve o mesmo JSON de segredos; `/status` também
  sem auth.
- **Mitigação:** remover `/config`; autenticar/escopar `/status`; reduzir superfície (ETSI 5.6).

### F07 — Secure Boot / Flash Encryption OFF
- **Evidência:** `info` → `secure_boot=OFF flash_encryption=OFF`.
- **Mitigação:** habilitar ambos na produção (ESP-IDF). *Secure Boot* garante
  autenticidade do que executa; *Flash Encryption* protege confidencialidade do
  que está na flash — funções **complementares**. (Proposto conceitualmente; eFuses
  irreversíveis **não** são gravados nesta atividade.)

### F08 — Comandos de estado sem autenticação
- Reconfiguração (`interval`) e `factory` (apaga NVS) acionáveis sem credencial no
  console → disponibilidade/integridade. **Médio.**
