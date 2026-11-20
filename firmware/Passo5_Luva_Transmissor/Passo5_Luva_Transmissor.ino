// ============================================================================
//  ESP HAND CONTROL 2.0  -  Passo 5: LUVA (transmissor)
//  Prof. Rafael Oliveira (Tio Rafa) - Robotica Paula Souza
//
//  Le a inclinacao da mao (MPU6050) e manda, 50 vezes por segundo, dois
//  numeros para o carro via ESP-NOW:
//      acelerador : dedos para baixo = frente, dedos para cima = re
//      direcao    : inclinar para a direita = direita, esquerda = esquerda
//
//  Ao ligar:
//    1. A luva pede para voce segurar a mao PARADA e PLANA e mede o "neutro"
//       (calibracao). Assim a luva funciona mesmo que o sensor tenha sido
//       preso um pouco torto.
//    2. A luva comeca TRAVADA (carro parado). Clique curto no botao = libera.
//
//  Botao (BOOT da placa, ou outro pino em config_luva.h):
//      clique curto  : trava / destrava   (travada => carro para na hora)
//      clique longo  : recalibra o neutro
//
//  LED (se houver): aceso = liberada | piscando lento = travada
//                   piscando rapido = sensor com problema
//
//  Resultado esperado (Monitor Serial, 115200):
//      Calibrando... Pronto! neutro: pitch=+1.2 roll=-0.4
//      Luva TRAVADA. Clique no botao para liberar.
//      [LIBERADA] pitch=+25.3 roll=-3.1 -> acel=+44 dir=+0 | seq=1234 falhas=0
//
//  ESP32-C3: marque  Ferramentas > USB CDC On Boot > Enabled.
// ============================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "config_luva.h"
#include "protocolo.h"
#include "logica.h"
#include "mpu6050.h"
#include "utilitarios.h"

// ---- Tipos (antes de qualquer funcao, por causa do pre-processador da IDE) ----
enum class Clique : uint8_t { NENHUM, CURTO, LONGO };

// ---- Estado ----
static MPU6050Mini g_mpu;
static bool  g_sensorOk = false;
static float g_ax = 0.0f, g_ay = 0.0f, g_az = 1.0f;     // aceleracao filtrada
static bool  g_filtroIniciado = false;
static float g_offsetPitch = 0.0f, g_offsetRoll = 0.0f; // "neutro" medido na calibracao
static bool  g_travada = false;
static uint8_t  g_sequencia = 0;
static uint32_t g_falhasEnvio = 0;
static uint8_t  g_falhasSensor = 0;
static int8_t   g_ultAcel = 0, g_ultDir = 0;
static float    g_ultPitch = 0.0f, g_ultRoll = 0.0f;

// ============================================================================
//  RADIO
// ============================================================================
static bool macZerado(const uint8_t *mac) {
  for (int i = 0; i < 6; i++) if (mac[i] != 0) return false;
  return true;
}

static bool iniciarRadio() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
#ifdef TX_POWER_PERSONALIZADO
  WiFi.setTxPower(TX_POWER_PERSONALIZADO);
#endif
  if (esp_wifi_set_channel(CANAL_ESPNOW, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
    Serial.println("AVISO: nao consegui fixar o canal Wi-Fi.");
  }
  if (esp_now_init() != ESP_OK) return false;

  // No ESP-NOW todo destino precisa ser cadastrado como "peer" antes do envio.
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, MAC_CARRO, 6);
  peer.channel = CANAL_ESPNOW;
  peer.ifidx   = WIFI_IF_STA;
  peer.encrypt = false;     // criptografia e um desafio do manual (PMK/LMK)
  return esp_now_add_peer(&peer) == ESP_OK;
}

static void enviarPacote(int8_t acelerador, int8_t direcao, bool habilitado) {
  PacoteControle p;
  p.magic      = PROTOCOLO_MAGIC;
  p.versao     = PROTOCOLO_VERSAO;
  p.sequencia  = g_sequencia++;
  p.acelerador = acelerador;
  p.direcao    = direcao;
  p.flags      = habilitado ? FLAG_HABILITADO : 0;
  p.checksum   = calcularChecksum(p);
  // esp_now_send devolve erro so para problemas LOCAIS (peer nao cadastrado,
  // fila cheia...). Ele nao garante que o carro recebeu: a luva fala "no escuro"
  // e quem faz o failsafe e o carro.
  if (esp_now_send(MAC_CARRO, reinterpret_cast<const uint8_t *>(&p), sizeof(p)) != ESP_OK) g_falhasEnvio++;
}

// ============================================================================
//  SENSOR
// ============================================================================
// Le o MPU, filtra e devolve a inclinacao BRUTA (sem neutro, sem inversoes).
static bool lerInclinacaoBruta(float &pitch, float &roll) {
  float ax, ay, az;
  if (!g_mpu.lerAceleracao(ax, ay, az)) return false;
  if (!g_filtroIniciado) {
    g_ax = ax; g_ay = ay; g_az = az;
    g_filtroIniciado = true;
  }
  // Media movel exponencial: cada leitura nova entra com peso FILTRO_ALFA.
  g_ax += FILTRO_ALFA * (ax - g_ax);
  g_ay += FILTRO_ALFA * (ay - g_ay);
  g_az += FILTRO_ALFA * (az - g_az);
  calcularInclinacao(g_ax, g_ay, g_az, pitch, roll);
  return true;
}

// Aplica neutro, troca de eixos e inversoes -> inclinacao "da mao".
static void aplicarOrientacao(float pitchBruto, float rollBruto, float &acelGraus, float &dirGraus) {
  float p = pitchBruto - g_offsetPitch;
  float r = rollBruto  - g_offsetRoll;
  if (TROCAR_PITCH_ROLL) { float t = p; p = r; r = t; }
  acelGraus = INVERTER_ACELERADOR ? -p : p;
  dirGraus  = INVERTER_DIRECAO    ? -r : r;
}

// Mede o neutro: media da inclinacao enquanto a mao fica parada.
// Durante todo o processo manda pacotes "desabilitados" para o carro ficar quieto.
static bool calibrarNeutro() {
  Serial.println("Calibrando: segure a luva PARADA, mao aberta e plana...");
  for (uint8_t tentativa = 1; tentativa <= CALIBRACAO_TENTATIVAS; tentativa++) {
    float somaP = 0, somaR = 0;
    float minP = 999, maxP = -999, minR = 999, maxR = -999;
    int n = 0;
    uint32_t inicio = millis();
    uint32_t proximo = inicio;
    while (millis() - inicio < CALIBRACAO_DURACAO_MS) {
      if ((int32_t)(millis() - proximo) >= 0) {
        proximo += PERIODO_ENVIO_MS;
        enviarPacote(0, 0, false);
        float p, r;
        if (lerInclinacaoBruta(p, r) && (millis() - inicio) > 300) {   // ignora os 300 ms iniciais (filtro assentando)
          somaP += p; somaR += r; n++;
          if (p < minP) minP = p;
          if (p > maxP) maxP = p;
          if (r < minR) minR = r;
          if (r > maxR) maxR = r;
        }
      }
      ledEscrever(PINO_LED, piscando(millis(), 200));
      delay(1);
    }
    if (n < 10) {
      Serial.println("  poucas leituras do sensor; tentando de novo...");
      continue;
    }
    if ((maxP - minP) <= CALIBRACAO_TOLERANCIA_GRAUS && (maxR - minR) <= CALIBRACAO_TOLERANCIA_GRAUS) {
      g_offsetPitch = somaP / n;
      g_offsetRoll  = somaR / n;
      Serial.printf("Pronto! neutro: pitch=%+.1f roll=%+.1f\n", g_offsetPitch, g_offsetRoll);
      return true;
    }
    Serial.printf("  mao mexeu demais (variou %.1f / %.1f graus). Tentativa %u de %u...\n",
                  maxP - minP, maxR - minR, (unsigned)tentativa, (unsigned)CALIBRACAO_TENTATIVAS);
  }
  Serial.println("AVISO: nao consegui calibrar; usando o neutro anterior (ou zero).");
  return false;
}

// ============================================================================
//  BOTAO  (clique curto = trava/destrava | clique longo = recalibra)
// ============================================================================
static Clique lerBotao(uint32_t agora) {
  if (PINO_BOTAO < 0) return Clique::NENHUM;
  static bool     estavaApertado = false;
  static uint32_t inicioAperto = 0;

  bool apertado = (digitalRead(PINO_BOTAO) == LOW);     // INPUT_PULLUP: apertar = LOW
  if (apertado && !estavaApertado) {
    estavaApertado = true;
    inicioAperto = agora;
  } else if (!apertado && estavaApertado) {
    estavaApertado = false;
    uint32_t duracao = agora - inicioAperto;
    if (duracao >= BOTAO_PRESSAO_LONGA_MS) return Clique::LONGO;
    if (duracao >= BOTAO_DEBOUNCE_MS)      return Clique::CURTO;
  }
  return Clique::NENHUM;
}

// ============================================================================
//  LED E DEBUG
// ============================================================================
static void atualizarLed(uint32_t agora) {
  if (!g_sensorOk)    ledEscrever(PINO_LED, piscando(agora, 200));
  else if (g_travada) ledEscrever(PINO_LED, piscando(agora, 1000));
  else                ledEscrever(PINO_LED, true);
}

static void imprimirDebug() {
  Serial.printf("[%s] pitch=%+6.1f roll=%+6.1f -> acel=%+4d dir=%+4d | seq=%u falhasEnvio=%lu\n",
                g_travada ? "TRAVADA" : "LIBERADA", g_ultPitch, g_ultRoll, g_ultAcel, g_ultDir,
                (unsigned)g_sequencia, (unsigned long)g_falhasEnvio);
}

static void erroFatal(const char *mensagem) {
  for (;;) {
    Serial.printf("ERRO FATAL: %s\n", mensagem);
    for (int i = 0; i < 10; i++) {
      ledEscrever(PINO_LED, (i & 1) == 0);
      delay(200);
    }
  }
}

// ============================================================================
//  SETUP / LOOP
// ============================================================================
void setup() {
  iniciarSerial();
  ledIniciar(PINO_LED);
  if (PINO_BOTAO >= 0) pinMode(PINO_BOTAO, INPUT_PULLUP);

  Serial.println();
  Serial.printf("=== ESP HAND CONTROL %s - LUVA | %s ===\n", VERSAO_FIRMWARE, PLACA_NOME);

  if (macZerado(MAC_CARRO)) {
    erroFatal("MAC_CARRO ainda e 00:00:..: rode o Passo1 no carro e cole o MAC no config_luva.h.");
  }
  if (!iniciarRadio()) erroFatal("ESP-NOW nao iniciou (ou nao consegui cadastrar o carro).");

  g_sensorOk = g_mpu.iniciar(MPU_ENDERECO, PINO_SDA, PINO_SCL, I2C_CLOCK_HZ);
  if (g_sensorOk) {
    calibrarNeutro();
  } else {
    Serial.println("MPU6050 NAO respondeu. Rode o Passo2 para diagnosticar. Vou tentar de novo.");
  }

  g_travada = (PINO_BOTAO >= 0) && INICIAR_TRAVADA;
  Serial.println(g_travada ? "Luva TRAVADA. Clique no botao para liberar."
                           : "Luva LIBERADA.");
}

void loop() {
  static uint32_t tEnvio = 0, tDebug = 0, tTentaSensor = 0;
  uint32_t agora = millis();

  // ---- Botao ----
  Clique c = lerBotao(agora);
  if (c == Clique::CURTO) {
    g_travada = !g_travada;
    Serial.println(g_travada ? ">> TRAVADA" : ">> LIBERADA");
  } else if (c == Clique::LONGO && g_sensorOk) {
    calibrarNeutro();
  }

  atualizarLed(agora);

  // ---- Sensor com problema: carro parado + tentativas de reconectar ----
  if (!g_sensorOk) {
    if (agora - tEnvio >= PERIODO_ENVIO_MS) {
      tEnvio = agora;
      enviarPacote(0, 0, false);
    }
    if (agora - tTentaSensor >= 1000) {
      tTentaSensor = agora;
      g_sensorOk = g_mpu.iniciar(MPU_ENDERECO, PINO_SDA, PINO_SCL, I2C_CLOCK_HZ);
      if (g_sensorOk) {
        g_falhasSensor = 0;
        g_filtroIniciado = false;
        Serial.println("MPU6050 voltou!");
        calibrarNeutro();
      }
    }
    delay(1);
    return;
  }

  // ---- Ciclo normal: 1 leitura + 1 envio a cada PERIODO_ENVIO_MS ----
  if (agora - tEnvio >= PERIODO_ENVIO_MS) {
    tEnvio = agora;

    float pitchBruto, rollBruto;
    if (!lerInclinacaoBruta(pitchBruto, rollBruto)) {
      enviarPacote(0, 0, false);                 // na duvida, carro PARADO
      if (++g_falhasSensor >= 10) {
        g_sensorOk = false;
        Serial.println("Sensor parou de responder (fio solto?).");
      }
    } else {
      g_falhasSensor = 0;
      float acelGraus, dirGraus;
      aplicarOrientacao(pitchBruto, rollBruto, acelGraus, dirGraus);
      g_ultPitch = acelGraus;
      g_ultRoll  = dirGraus;

      if (g_travada) {
        g_ultAcel = 0;
        g_ultDir = 0;
        enviarPacote(0, 0, false);
      } else {
        g_ultAcel = anguloParaPercentual(acelGraus, ZONA_MORTA_GRAUS, ANGULO_MAXIMO_GRAUS, MODO_PROPORCIONAL);
        g_ultDir  = anguloParaPercentual(dirGraus,  ZONA_MORTA_GRAUS, ANGULO_MAXIMO_GRAUS, MODO_PROPORCIONAL);
        enviarPacote(g_ultAcel, g_ultDir, true);
      }
    }
  }

  if (DEBUG_SERIAL && (agora - tDebug >= DEBUG_INTERVALO_MS)) {
    tDebug = agora;
    imprimirDebug();
  }
  delay(1);
}
