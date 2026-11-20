// ============================================================================
//  ESP HAND CONTROL 2.0  -  Passo 2: testar o sensor da luva (MPU6050)
//  Prof. Rafael Oliveira (Tio Rafa) - Robotica Paula Souza
//
//  Grave no ESP32 da LUVA (com o MPU6050 ja ligado). Este sketch NAO usa radio:
//  so le o sensor e mostra a inclinacao da sua mao. Se aqui nao funciona, o
//  Passo 5 tambem nao vai funcionar - por isso testamos separado.
//
//  O que fazer:
//    1. Abra o Monitor Serial (115200). Mao parada e plana: pitch e roll ~ 0.
//    2. Incline a mao com os DEDOS PARA BAIXO: "pitch" deve ficar POSITIVO.
//    3. Incline para a DIREITA: "roll" deve ficar POSITIVO.
//    Sinal trocado? Nao mexa na fiacao: ajuste INVERTER_ACELERADOR /
//    INVERTER_DIRECAO / TROCAR_PITCH_ROLL no config_luva.h.
//
//  Modo grafico: ponha MODO_PLOTTER = true e abra Ferramentas > Plotter Serial.
//  Voce vai VER a sua mao desenhando as duas curvas. Bom para aula!
//
//  Resultado esperado: "MPU6050 respondeu" + linhas de pitch/roll a 20 Hz.
//  Se aparecer "MPU6050 NAO respondeu": o sketch escaneia o barramento I2C e
//  lista o que achou (um MPU6050 aparece em 0x68 ou 0x69).
// ============================================================================
#include <Arduino.h>
#include <Wire.h>
#include "config_luva.h"
#include "logica.h"
#include "mpu6050.h"
#include "utilitarios.h"

constexpr bool MODO_PLOTTER = false;   // true = imprime so "pitch:..,roll:.." para o Plotter Serial

static MPU6050Mini mpu;
static float g_ax = 0, g_ay = 0, g_az = 1;
static bool g_filtroIniciado = false;

static void escanearI2C() {
  Serial.println("Escaneando o barramento I2C...");
  int achados = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  dispositivo em 0x%02X\n", a);
      achados++;
    }
  }
  if (achados == 0) {
    Serial.println("  nada encontrado: confira VCC, GND, SDA, SCL (e se SDA/SCL nao estao trocados).");
  }
}

void setup() {
  iniciarSerial();
  ledIniciar(PINO_LED);
  Serial.println();
  Serial.println("=== ESP HAND CONTROL 2.0 - Passo 2: teste do MPU6050 ===");
  Serial.printf("Placa: %s | SDA=%d SCL=%d | endereco 0x%02X\n", PLACA_NOME, PINO_SDA, PINO_SCL, MPU_ENDERECO);

  if (!mpu.iniciar(MPU_ENDERECO, PINO_SDA, PINO_SCL, I2C_CLOCK_HZ)) {
    Serial.println("MPU6050 NAO respondeu.");
    escanearI2C();
    return;
  }
  uint8_t id = 0;
  if (mpu.lerQuemSou(id)) {
    Serial.printf("MPU6050 respondeu (WHO_AM_I = 0x%02X)%s\n", id,
                  id == 0x68 ? "" : "  <- diferente de 0x68: clone ou irmao do MPU6050, vamos em frente");
  }
  Serial.println("Mexa a mao. pitch>0 = dedos para baixo | roll>0 = inclinar para a direita");
}

void loop() {
  static uint32_t ultimo = 0;
  uint32_t agora = millis();
  if (agora - ultimo < 50) return;     // 20 leituras por segundo
  ultimo = agora;

  float ax, ay, az;
  if (!mpu.lerAceleracao(ax, ay, az)) {
    Serial.println("falha de leitura I2C (fio solto?)");
    ledEscrever(PINO_LED, piscando(agora, 200));
    return;
  }

  // Filtro passa-baixa (media movel exponencial): tira o tremido da mao.
  if (!g_filtroIniciado) { g_ax = ax; g_ay = ay; g_az = az; g_filtroIniciado = true; }
  g_ax += FILTRO_ALFA * (ax - g_ax);
  g_ay += FILTRO_ALFA * (ay - g_ay);
  g_az += FILTRO_ALFA * (az - g_az);

  float pitch, roll;
  calcularInclinacao(g_ax, g_ay, g_az, pitch, roll);
  ledEscrever(PINO_LED, true);

  if (MODO_PLOTTER) {
    Serial.printf("pitch:%.1f,roll:%.1f\n", pitch, roll);
  } else {
    Serial.printf("ax=%+.2f ay=%+.2f az=%+.2f g | pitch=%+6.1f  roll=%+6.1f graus\n", g_ax, g_ay, g_az, pitch, roll);
  }
}
