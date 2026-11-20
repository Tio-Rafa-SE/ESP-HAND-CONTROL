// ============================================================================
//  mpu6050.h  -  ESP HAND CONTROL 2.0
//
//  Driver MINIMO do MPU6050 - so o acelerometro, direto nos registradores.
//
//  Por que nao usar a biblioteca I2Cdev + MPU6050_6Axis_MotionApps20 do v1?
//    - O DMP ("motion processor") fazia yaw, mas o yaw DERIVA com o tempo
//      (sem magnetometro, nao ha referencia fixa) e no v1 ele podia "puxar"
//      o carro sozinho para girar;
//    - A biblioteca e grande, depende de I2Cdev e de #ifs de plataforma, e
//      era a maior fonte de erro de compilacao ao trocar de placa/core;
//    - Para inclinacao (pitch e roll) o acelerometro basta - e o codigo cabe
//      em 80 linhas que voce consegue ler inteiras.
//
//  Registradores usados (MPU-6000/6050 Register Map, rev 4.2, InvenSense):
//     0x6B PWR_MGMT_1    0x1A CONFIG (filtro DLPF)   0x1C ACCEL_CONFIG (faixa)
//     0x3B ACCEL_XOUT_H  (6 bytes: XH XL YH YL ZH ZL)  0x75 WHO_AM_I
//
//  Copia oficial em firmware/comum/ - use tools/sincronizar_comum.sh.
// ============================================================================
#pragma once
#include <Arduino.h>
#include <Wire.h>

class MPU6050Mini {
 public:
  static constexpr uint8_t REG_CONFIG       = 0x1A;
  static constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
  static constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;
  static constexpr uint8_t REG_PWR_MGMT_1   = 0x6B;
  static constexpr uint8_t REG_WHO_AM_I     = 0x75;

  // Com faixa de +-2 g, o sensor entrega 16384 contagens por 1 g.
  static constexpr float CONTAGENS_POR_G = 16384.0f;

  // Inicia I2C e configura o sensor. Retorna false se o MPU nao respondeu.
  bool iniciar(uint8_t endereco, int pinoSda, int pinoScl, uint32_t clockHz) {
    _endereco = endereco;
    Wire.begin(pinoSda, pinoScl);
    Wire.setClock(clockHz);
    Wire.setTimeOut(30);   // ms. Fio solto nao pode travar o programa inteiro.

    if (!escrever(REG_PWR_MGMT_1, 0x80)) return false;   // DEVICE_RESET
    delay(100);
    if (!escrever(REG_PWR_MGMT_1, 0x01)) return false;   // acorda; clock = PLL do giroscopio X
    delay(10);
    if (!escrever(REG_CONFIG, 0x03)) return false;       // DLPF = 44 Hz: tira vibracao do sinal
    if (!escrever(REG_ACCEL_CONFIG, 0x00)) return false; // faixa +-2 g
    return true;
  }

  // Identidade do chip. O MPU6050 original responde 0x68 (independente do AD0).
  // Clones/irmaos respondem outros valores (ex.: 0x70) - tudo bem, so avisamos.
  bool lerQuemSou(uint8_t &id) { return ler(REG_WHO_AM_I, &id, 1); }

  // Le a aceleracao nos 3 eixos, em g. Retorna false se a leitura I2C falhou.
  bool lerAceleracao(float &ax, float &ay, float &az) {
    uint8_t b[6];
    if (!ler(REG_ACCEL_XOUT_H, b, 6)) return false;
    ax = (int16_t)((b[0] << 8) | b[1]) / CONTAGENS_POR_G;
    ay = (int16_t)((b[2] << 8) | b[3]) / CONTAGENS_POR_G;
    az = (int16_t)((b[4] << 8) | b[5]) / CONTAGENS_POR_G;
    return true;
  }

 private:
  uint8_t _endereco = 0x68;

  bool escrever(uint8_t reg, uint8_t valor) {
    Wire.beginTransmission(_endereco);
    Wire.write(reg);
    Wire.write(valor);
    return Wire.endTransmission() == 0;
  }

  bool ler(uint8_t reg, uint8_t *dest, uint8_t n) {
    Wire.beginTransmission(_endereco);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;      // "repeated start"
    if (Wire.requestFrom((uint16_t)_endereco, (uint8_t)n, true) != n) return false;
    for (uint8_t i = 0; i < n; i++) dest[i] = Wire.read();
    return true;
  }
};
