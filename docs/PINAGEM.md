# Pinagem

Os pinos reais ficam em `firmware/comum/config_carro.h` e `config_luva.h`, escolhidos pelo chip detectado. Esta tabela é um espelho; se houver divergência, **vale o código**.

Os pinos abaixo são escolhas do projeto, não verdades universais de cada placa. **CONFIRME NA SUA PLACA** se os furos existem, se não estão ocupados por periféricos da sua versão e se o seu fabricante não usa os mesmos GPIOs de outra forma.

## Luva

| Função | ESP32 clássico | ESP32-C3 Mini |
|---|---:|---:|
| SDA (MPU6050) | GPIO21 | GPIO4 |
| SCL (MPU6050) | GPIO22 | GPIO5 |
| Botão (trava e calibração) | GPIO0 (BOOT) | GPIO9 (BOOT) |
| LED de estado | GPIO2 | nenhum (-1) |
| MPU6050 endereço | 0x68 (AD0 no GND) | 0x68 |

## Carro

| Função | ESP32 clássico | ESP32-C3 Mini |
|---|---:|---:|
| Motor direito, IN1 | GPIO26 | GPIO0 |
| Motor direito, IN2 | GPIO27 | GPIO1 |
| Motor esquerdo, IN3 | GPIO14 | GPIO3 |
| Motor esquerdo, IN4 | GPIO12 | GPIO4 |
| LED de estado | GPIO2 | nenhum (-1) |

Valor -1 em um pino de LED ou botão significa "não usar": as funções correspondentes viram no-op.

## Cuidados por chip

**ESP32 clássico:** GPIO0, GPIO2, GPIO12 e GPIO15 são pinos de boot (strapping). GPIO12 é usado como saída de motor aqui; veja a observação na seção 7.2 do manual. GPIOs 6 a 11 ligam à flash e não devem ser usados.

**ESP32-C3:** GPIO2, GPIO8 e GPIO9 são pinos de boot; GPIO18 e GPIO19 são a USB. O projeto usa o GPIO9 apenas como entrada de botão depois que a placa já ligou. O LEDC tem 6 canais nesse chip; o projeto usa 4.

## Para mapear outra placa

1. Copie um dos blocos `#if defined(CONFIG_IDF_TARGET_...)` nos dois arquivos de configuração.
2. Troque o identificador do chip.
3. Consulte o esquema ou o pinout oficial da **sua** placa para escolher pinos livres, sem função de boot e sem periférico embutido.
4. Rode o Passo 3 (motores) e o Passo 2 (sensor) antes do resto.
