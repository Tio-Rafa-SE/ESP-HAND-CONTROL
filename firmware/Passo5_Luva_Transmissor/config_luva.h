// ============================================================================
//  config_luva.h  -  ESP HAND CONTROL 2.0  (LUVA / transmissor)
//
//  TUDO que voce pode querer ajustar na luva esta AQUI.
//
//  Copia oficial em firmware/comum/ - use tools/sincronizar_comum.sh.
// ============================================================================
#pragma once
#include <Arduino.h>
#include <stdint.h>

#define VERSAO_FIRMWARE "2.0.0"

// ============================================================================
//  1) PARA QUEM A LUVA FALA  (obrigatorio!)
//
//  Rode o Passo1_Descobrir_MAC no ESP32 do CARRO, copie o "MAC (codigo)" e
//  cole aqui. NAO copie o MAC do exemplo do v1: cada placa tem o seu.
//
//  Truque de aula: use {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF} (broadcast) e UMA luva
//  comanda TODOS os carros ao alcance. Divertido... e perigoso numa sala cheia.
//  Com 00:00:00:00:00:00 a luva se recusa a iniciar (de proposito).
// ============================================================================
static const uint8_t MAC_CARRO[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// ============================================================================
//  2) PINAGEM POR PLACA
//
//  Montagem do sensor: MPU6050 com os componentes para CIMA e a seta X do
//  modulo apontando para a ponta dos dedos. (Outra posicao? Veja
//  TROCAR_PITCH_ROLL e INVERTER_* mais abaixo.)
// ============================================================================
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  // ---- ESP32-C3 Mini (LOLIN C3 Mini, C3 Super Mini e parecidas) ------------
  // I2C padrao dessas placas varia (LOLIN: SDA 8/SCL 10; Super Mini: 8/9) e
  // mistura pinos de strapping/LED. Por isso fixamos SDA=4 e SCL=5, que
  // existem nas duas e nao sao strapping. CONFIRA o pinout da SUA placa.
  #define PLACA_NOME "ESP32-C3 Mini"
  constexpr int PINO_SDA   = 4;
  constexpr int PINO_SCL   = 5;
  constexpr int PINO_BOTAO = 9;    // botao BOOT (GPIO9), ativo em LOW. -1 = sem botao.
  constexpr int PINO_LED   = -1;   // -1 = sem LED.

  // Mesmo motivo do config_carro.h (relatos de C3 Super Mini instavel em potencia maxima).
  #define TX_POWER_PERSONALIZADO WIFI_POWER_8_5dBm

#elif defined(CONFIG_IDF_TARGET_ESP32)
  // ---- ESP32 classico (DevKit 38 pinos com ESP-WROOM-32, como no v1) -------
  // SDA=21 e SCL=22 sao os pinos I2C padrao (o v1 chamava Wire.begin() sem
  // argumentos, entao usava estes).
  #define PLACA_NOME "ESP32 classico"
  constexpr int PINO_SDA   = 21;
  constexpr int PINO_SCL   = 22;
  constexpr int PINO_BOTAO = 0;    // botao BOOT (GPIO0), ativo em LOW. -1 = sem botao.
  constexpr int PINO_LED   = 2;    // LED azul de muitas DevKit.

#else
  #error "Placa ainda nao mapeada. Copie um dos blocos acima, ajuste os pinos conferindo o esquema da SUA placa e use CONFIG_IDF_TARGET_<SEU_CHIP>."
#endif

// ============================================================================
//  3) SENSOR
// ============================================================================
constexpr uint8_t  MPU_ENDERECO = 0x68;     // AD0 no GND = 0x68 | AD0 no 3V3 = 0x69
constexpr uint32_t I2C_CLOCK_HZ = 400000;

// ============================================================================
//  4) "SENTIDO" DA MAO  (a parte mais divertida de ajustar)
// ============================================================================
constexpr float ZONA_MORTA_GRAUS    = 12.0f; // dentro disso o carro fica parado. Mao tremida? Aumente.
constexpr float ANGULO_MAXIMO_GRAUS = 40.0f; // a partir daqui, comando maximo
constexpr bool  MODO_PROPORCIONAL   = true;  // true: quanto mais inclina, mais rapido. false: liga/desliga como o v1
                                             // (para recriar o v1: false + ZONA_MORTA_GRAUS = 35)

constexpr bool  TROCAR_PITCH_ROLL   = false; // true se o sensor estiver girado 90 graus na luva
constexpr bool  INVERTER_ACELERADOR = false; // true se "dedos para baixo" fizer o carro dar re
constexpr bool  INVERTER_DIRECAO    = false; // true se inclinar para a direita fizer o carro virar a esquerda

constexpr float FILTRO_ALFA         = 0.25f; // 0..1. Menor = mais suave e mais lento. Maior = mais rapido e mais tremido.

// ============================================================================
//  5) COMUNICACAO
// ============================================================================
constexpr uint32_t PERIODO_ENVIO_MS = 20;    // 50 pacotes por segundo

// ============================================================================
//  6) SEGURANCA E BOTAO
//  Clique curto  = trava/destrava (travada: o carro para na hora)
//  Clique longo  = recalibra o "neutro" (mao parada e plana)
// ============================================================================
constexpr bool     INICIAR_TRAVADA          = true; // so vale se existir botao (PINO_BOTAO >= 0)
constexpr uint32_t BOTAO_PRESSAO_LONGA_MS   = 1500;
constexpr uint32_t BOTAO_DEBOUNCE_MS        = 30;

constexpr uint32_t CALIBRACAO_DURACAO_MS        = 1500;
constexpr float    CALIBRACAO_TOLERANCIA_GRAUS  = 6.0f; // oscilou mais que isso? Pede para tentar de novo.
constexpr uint8_t  CALIBRACAO_TENTATIVAS        = 5;

// ============================================================================
//  7) DEPURACAO
// ============================================================================
constexpr bool     DEBUG_SERIAL        = true;
constexpr uint32_t DEBUG_INTERVALO_MS  = 250;
