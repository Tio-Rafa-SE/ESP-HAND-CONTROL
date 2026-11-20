// STUB de Wire.h com um MPU6050 "de mentira" dentro (registradores em memoria).
#pragma once
#include "Arduino.h"

struct TwoWireStub {
  uint8_t regs[256];
  uint8_t ponteiro = 0;
  int     bytesTx = 0;
  uint8_t rx[16];
  int     rxLen = 0, rxPos = 0;
  bool    falhar = false;       // teste: simula fio solto (NACK)

  TwoWireStub() { memset(regs, 0, sizeof(regs)); regs[0x75] = 0x68; }

  bool begin(int, int) { return true; }
  void setClock(uint32_t) {}
  void setTimeOut(uint16_t) {}
  void beginTransmission(uint8_t) { bytesTx = 0; }
  size_t write(uint8_t b) {
    if (bytesTx == 0) ponteiro = b;
    else regs[(uint8_t)(ponteiro + bytesTx - 1)] = b;
    bytesTx++;
    return 1;
  }
  uint8_t endTransmission(bool) { return falhar ? 2 : 0; }
  uint8_t endTransmission() { return endTransmission(true); }
  uint8_t requestFrom(uint16_t, uint8_t n, bool) {
    if (falhar) return 0;
    for (int i = 0; i < n && i < 16; i++) rx[i] = regs[(uint8_t)(ponteiro + i)];
    rxLen = n; rxPos = 0;
    return n;
  }
  int read() { return rxPos < rxLen ? rx[rxPos++] : -1; }

  // ---- ganchos de teste ----
  void definirAceleracao(float ax, float ay, float az) {
    auto grava = [&](int reg, float g) {
      int16_t v = (int16_t)lroundf(g * 16384.0f);
      regs[reg] = (uint8_t)((uint16_t)v >> 8);
      regs[reg + 1] = (uint8_t)((uint16_t)v & 0xFF);
    };
    grava(0x3B, ax); grava(0x3D, ay); grava(0x3F, az);
  }
  // Mao inclinada: pitch (dedos para baixo) e roll (direita para baixo), em graus.
  void definirInclinacao(float pitchGraus, float rollGraus) {
    const float k = 3.14159265358979f / 180.0f;
    // Montagem: X para os dedos, Y para a esquerda, Z para cima.
    float ax = -sinf(pitchGraus * k);
    float ay = sinf(rollGraus * k) * cosf(pitchGraus * k);
    float az = cosf(rollGraus * k) * cosf(pitchGraus * k);
    definirAceleracao(ax, ay, az);
  }
};
inline TwoWireStub Wire;
