# Changelog

## 2.0.0

Reescrita completa a partir da versão 1 (ESP32 + MPU6050 + L298N via ESP-NOW).

Novidades:
- Suporte ao ESP32 clássico e ao ESP32-C3 Mini, com pinos escolhidos pelo chip detectado.
- Compatível com Arduino-ESP32 2.x e 3.x (API de PWM e callback de recepção).
- Pacote de 7 bytes com marcador, versão, sequência, flags e checksum.
- Failsafe de 400 ms, luva que inicia travada, parada imediata ao travar.
- Calibração do neutro pelo botão (pressão longa).
- Velocidade proporcional ao ângulo, zona morta, filtro e mistura diferencial.
- Rampa de aceleração.
- Driver mínimo do MPU6050 por registradores (sem biblioteca externa, sem deriva).
- Cinco sketches em ordem didática e suíte de testes no computador.
- Manual completo com interlúdios filosóficos, diagnóstico e tabela de testes.

Corrigido em relação à v1:
- Índices de motor fora dos limites e PWM anexado ao GPIO0.
- Deriva de giro por uso do DMP.
- Pacote sem validação.
- Trabalho de motor dentro do callback do rádio.
- Incompatibilidade com o core 3.x.
- MAC escrito direto no código.

Pendente: testes em hardware real.
