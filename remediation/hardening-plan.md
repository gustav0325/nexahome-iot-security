# Plano de Hardening — versão comercial do NexaSense

Prioridade por risco (ver `findings/findings.md`). Não é obrigatório implementar
tudo; a lista mostra como cada achado seria tratado em um produto real.
Mecanismos irreversíveis de hardware (eFuses) são propostos **conceitualmente** —
não foram gravados nesta atividade.

## Prioridade 1 — bloqueia comprometimento remoto/total

1. **Assinatura de firmware + Secure Boot v2** (F05, F07)
   ESP-IDF verifica a assinatura RSA-3072/ECDSA da imagem antes de executar;
   OTA só aceita imagem assinada pela chave da NexaHome. Fecha a execução remota
   de código via OTA e a troca por imagem de atacante.
2. **OTA sobre HTTPS com validação de certificado/pinning** (F05)
   Transporte cifrado + origem confiável, além da assinatura. Prever **rollback /
   partição de recuperação** em caso de falha de atualização.
3. **TLS/mTLS no MQTT + credencial única por dispositivo** (F03, F04)
   Fim do texto claro e da senha de frota compartilhada; certificado por dispositivo
   provisionado em fábrica. ACL no broker por `client_id` (cada device só publica/assina
   seus próprios tópicos).

## Prioridade 2 — reduz superfície e protege segredos

4. **Flash Encryption** (F02, F07) — protege a confidencialidade da imagem e da NVS
   contra dump físico.
5. **Remover credenciais da imagem; provisionamento individual** (F02) — segredos
   em NVS criptografada, gerados por dispositivo, nunca no código.
6. **Desativar/proteger o console de diagnóstico em produção** (F01, F08) — build
   de release sem `DEBUG BUILD`; se houver console de RMA, com autenticação e
   desabilitável por eFuse.
7. **Remover `/config`; autenticar `/status`** (F06) — ou eliminar o servidor HTTP
   local se não for essencial.

## Prioridade 3 — robustez e endurecimento de hardware

8. **Reduzir logs de depuração** no nível de produção (sem segredos no boot).
9. **Desabilitar JTAG por eFuse** e proteger *download mode* (F01/hardware map).
10. **Processo seguro de atualização** com versionamento anti-rollback (eFuse secure
    version) e **estratégia de recuperação** se a imagem nova falhar.

## Mapa achado → medida → referência

| Achado | Medida de hardening | Referência |
|---|---|---|
| F01 | Console off/autenticado em produção | NIST IR 8259A (controle de acesso a interfaces); ETSI 5.6 |
| F02 | Segredos fora da imagem + Flash Encryption | ETSI 5.4; ESP-IDF Flash Encryption |
| F03 | mTLS + credencial por dispositivo | ETSI 5.5; NIST (proteção de dados) |
| F04 | ACL/autorização de comandos | OWASP ISTG; ETSI 5.5 |
| F05 | Secure Boot v2 + OTA assinado/HTTPS + rollback | ETSI 5.3; ESP-IDF Secure Boot |
| F06 | Remover `/config`, reduzir superfície | ETSI 5.6 |
| F07 | Secure Boot + Flash Encryption | ESP-IDF Security (funções complementares) |
| F08 | Autenticar comandos de estado | NIST IR 8259A |

> **Secure Boot** (autenticidade do que executa) e **Flash Encryption**
> (confidencialidade do que está armazenado) têm papéis **complementares** — a
> documentação da Espressif recomenda usar os dois em conjunto na proteção do
> software do dispositivo.
