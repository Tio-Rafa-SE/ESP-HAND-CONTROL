// ============================================================================
//  config_carro.h  -  ESP HAND CONTROL 2.0  (CARRO / receptor)
//
//  TUDO que voce pode querer ajustar no carro esta AQUI.
//  O resto do codigo nao precisa ser tocado para adaptar o projeto.
//
//  Copia oficial em firmware/comum/ - use tools/sincronizar_comum.sh.
// ============================================================================
#pragma once
#include <Arduino.h>
#include <stdint.h>

#define VERSAO_FIRMWARE "2.0.0"

// ============================================================================
//  1) PINAGEM POR PLACA
//
//  O sketch descobre sozinho em qual chip esta rodando (CONFIG_IDF_TARGET_*)
//  e escolhe a tabela certa. NUNCA copie pinagem de uma placa para outra sem
//  conferir: GPIO e coisa de hardware, nao de "ESP32 em geral".
//
//  Ligacao no L298N (convencao deste manual):
//     Motor A (OUT1/OUT2) = lado DIREITO   IN1 <- DIR_IN1,  IN2 <- DIR_IN2
//     Motor B (OUT3/OUT4) = lado ESQUERDO  IN3 <- ESQ_IN1,  IN4 <- ESQ_IN2
//  Os jumpers ENA e ENB do L298N ficam INSTALADOS.
//  Se a sua montagem do v1 era diferente, nao precisa refazer a fiacao:
//  ajuste os numeros abaixo ou use INVERTER_MOTOR_* / os testes do Passo 3.
// ============================================================================
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  // ---- ESP32-C3 Mini (LOLIN C3 Mini, C3 Super Mini e parecidas) ------------
  // Evitamos os pinos de strapping (GPIO2, GPIO8, GPIO9) e os de USB (18, 19).
  // Estes 4 existem nas duas placas citadas - CONFIRA na sua.
  #define PLACA_NOME "ESP32-C3 Mini"
  constexpr int PINO_MOTOR_DIR_IN1 = 0;
  constexpr int PINO_MOTOR_DIR_IN2 = 1;
  constexpr int PINO_MOTOR_ESQ_IN1 = 3;
  constexpr int PINO_MOTOR_ESQ_IN2 = 4;
  constexpr int PINO_LED           = -1;   // -1 = sem LED. Ligue um LED + resistor de 330 ohm em um GPIO livre se quiser.

  // Relatos da comunidade: algumas C3 Super Mini tem antena mal projetada e
  // ficam instaveis com potencia maxima. 8,5 dBm resolve e sobra para uma sala.
  // Quer mais alcance e sua placa e boa (ex.: LOLIN)? Comente esta linha.
  #define TX_POWER_PERSONALIZADO WIFI_POWER_8_5dBm

#elif defined(CONFIG_IDF_TARGET_ESP32)
  // ---- ESP32 classico (DevKit 38 pinos com ESP-WROOM-32, como no v1) -------
  // Mesmos pinos do v1 original. Atencao: GPIO12 e pino de strapping (MTDI):
  // se o L298N puxar esse fio para ALTO na hora de ligar, a placa pode nao
  // dar boot. Com as entradas do L298N em repouso isso nao acontece; se
  // acontecer, mude para outro GPIO livre (ex.: 32 ou 33).
  #define PLACA_NOME "ESP32 classico"
  constexpr int PINO_MOTOR_DIR_IN1 = 26;
  constexpr int PINO_MOTOR_DIR_IN2 = 27;
  constexpr int PINO_MOTOR_ESQ_IN1 = 14;
  constexpr int PINO_MOTOR_ESQ_IN2 = 12;
  constexpr int PINO_LED           = 2;    // LED azul de muitas DevKit. Se a sua nao tem, nada acontece.

#else
  #error "Placa ainda nao mapeada. Copie um dos blocos acima, ajuste os pinos conferindo o esquema da SUA placa e use CONFIG_IDF_TARGET_<SEU_CHIP>."
#endif

// ============================================================================
//  2) SENTIDO DOS MOTORES
//  Rodou o Passo 3 e uma roda gira ao contrario do esperado? Mude para true.
// ============================================================================
constexpr bool INVERTER_MOTOR_ESQ = false;
constexpr bool INVERTER_MOTOR_DIR = false;

// ============================================================================
//  3) PWM (velocidade)
// ============================================================================
constexpr uint32_t PWM_FREQUENCIA_HZ  = 5000;  // acima do que o ouvido incomoda, abaixo do limite do L298N
constexpr uint8_t  PWM_RESOLUCAO_BITS = 8;     // duty de 0 a 255 (nao mude sem revisar motores.h)
constexpr uint8_t  PWM_MINIMO         = 80;    // abaixo disso o motor so "zumbe". Descubra o seu no Passo 3.
constexpr uint8_t  PWM_MAXIMO         = 255;
constexpr uint8_t  LIMITE_VELOCIDADE_PCT = 100; // modo iniciante/sala pequena: use 60

// ============================================================================
//  4) SUAVIDADE E SEGURANCA
// ============================================================================
constexpr uint32_t PERIODO_CONTROLE_MS = 20;    // motores atualizados 50x por segundo
constexpr float    PASSO_RAMPA         = 0.10f; // quanto a velocidade pode mudar por ciclo (0.10 = 0 a 100% em 0,2 s)
constexpr uint32_t TIMEOUT_SINAL_MS    = 400;   // sem pacote valido por este tempo => motores PARAM

// ============================================================================
//  5) QUEM PODE DIRIGIR ESTE CARRO
//  Por padrao o carro obedece a QUALQUER luva que fale o protocolo certo.
//  Em sala, com varios carros e luvas, ligue o filtro e informe o MAC da luva.
// ============================================================================
#define FILTRAR_REMETENTE 0
static const uint8_t MAC_LUVA_PERMITIDA[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// ============================================================================
//  6) DEPURACAO
// ============================================================================
constexpr bool     DEBUG_SERIAL        = true;
constexpr uint32_t DEBUG_INTERVALO_MS  = 500;
