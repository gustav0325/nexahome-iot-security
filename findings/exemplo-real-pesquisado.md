# Exemplo real pesquisado

## Caso: Sonoff (dispositivo de automação residencial com ESP32) — CVE-2024-7206

**Dispositivo analisado.** Um dispositivo *smart home* da Sonoff baseado em ESP32
(com MCU complementar CC2652 e flash SPI externa), analisado publicamente por
pesquisador de segurança em 2025.

**Superfície de ataque envolvida.** Interfaces físicas de hardware — **porta de
depuração / boot mode** do ESP32 e a **flash SPI externa** — além da análise do
firmware extraído.

**Problema identificado.** Usando os pinos de boot, o pesquisador colocou o ESP32
em *download mode* e, com a ferramenta `esptool`, extraiu o conteúdo completo da
flash. A partir da imagem foi possível **analisar o firmware, encontrar segredos
embutidos, modificar e re-gravar** a imagem (clonar/alterar o dispositivo) — a
ausência de *Secure Boot* e de *Flash Encryption* tornou a flash legível e
regravável (CVE-2024-7206: extração de firmware e bypass de SSL pinning em hardware).

**Como poderia ser mitigado.** Habilitar **Flash Encryption** (confidencialidade da
imagem na flash), **Secure Boot** (impede executar/gravar imagem não assinada),
**bloquear/controlar o acesso às interfaces de debug e ao boot mode** por eFuse, e
**não embutir segredos reutilizáveis** no firmware.

**O que ensina para a NexaHome.** É exatamente a cadeia demonstrada no NexaSense:
interface física acessível (F01) + flash sem criptografia e firmware sem assinatura
(F02, F05, F07) → extração do firmware → coleta de credenciais → comprometimento
além do dispositivo isolado. O caso confirma que, com **acesso físico plausível** a
um produto residencial, as proteções da própria Espressif (Secure Boot + Flash
Encryption) deixam de ser "opcionais de produção" e passam a ser requisito para
evitar comprometimento em escala.

### Referências de apoio
- Jerin Sunny — *Hacking Sonoff Smart Home IoT Device: Extract, Modify, Boot,
  Intercept, Clone* (CVE-2024-7206 / CVE-2024-7205), 2025.
- Vasile, Oswald & Chothia — *Breaking all the things: a systematic survey of
  firmware extraction techniques for IoT devices*, University of Birmingham (CARDIS 2018).
- Software Secured — *Dumping ESP32 flash with esptool for analysis* (estudo de caso de probe BLE).

> As referências acima são material de apoio público; os valores sensíveis usados
> no laboratório NexaSense são **fictícios** e locais.
