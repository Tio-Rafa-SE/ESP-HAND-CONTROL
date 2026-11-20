# Protocolo de rádio (versão 2)

Fonte da verdade: `firmware/comum/protocolo.h`. Este documento descreve o que está lá.

## Transporte

- ESP-NOW, canal definido por `CANAL_ESPNOW` (padrão 1), nos dois lados.
- A luva envia para o MAC do carro (`MAC_CARRO`, em `config_luva.h`) a cada `PERIODO_ENVIO_MS` (padrão 20 ms, 50 pacotes por segundo).
- Sem criptografia por padrão (veja o Desafio 1 do manual).

## Formato do pacote (7 bytes, sem preenchimento)

| Offset | Campo | Tipo | Valores |
|---:|---|---|---|
| 0 | `magic` | uint8 | `0xA7` |
| 1 | `versao` | uint8 | `2` |
| 2 | `sequencia` | uint8 | contador que dá a volta em 255 |
| 3 | `acelerador` | int8 | -100 a +100 (positivo = frente) |
| 4 | `direcao` | int8 | -100 a +100 (positivo = direita) |
| 5 | `flags` | uint8 | bit 0 (`FLAG_HABILITADO`, 0x01) = luva destravada |
| 6 | `checksum` | uint8 | XOR dos bytes 0 a 5, com semente `0x5A` |

A estrutura é `__attribute__((packed))` e há um `static_assert` garantindo o tamanho de 7 bytes. O limite do ESP-NOW é de 250 bytes por pacote, então há muita folga.

## Validação no carro

Um pacote só é aceito se:

1. o tamanho recebido é exatamente 7 bytes;
2. `magic` e `versao` conferem;
3. o `checksum` confere;
4. `acelerador` e `direcao` estão entre -100 e +100.

Pacote inválido é descartado e contado. O carro também conta pacotes perdidos pela diferença na `sequencia`.

## Comportamento do carro por estado

| Condição | Estado | Motores |
|---|---|---|
| Nenhum pacote válido há mais de `TIMEOUT_SINAL_MS` (400 ms) | SEM SINAL | Parada imediata |
| Pacote válido, `FLAG_HABILITADO` = 0 | TRAVADO | Parada imediata |
| Pacote válido, `FLAG_HABILITADO` = 1 | ATIVO | Mistura diferencial com rampa |

## Mudando o protocolo

Se alterar o formato do pacote: incremente `PROTOCOLO_VERSAO`, atualize `static_assert`, os testes em `tools/teste_host/` e este documento, e regrave **os dois lados**. Firmwares de versões diferentes se ignoram de propósito.

## Observação sobre a v1

A versão 1 enviava 3 bytes crus, sem marcador, versão nem checksum. Qualquer pacote de qualquer origem poderia mexer nos motores.
