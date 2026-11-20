// Testes das funcoes PURAS: logica.h e protocolo.h (nao precisam de stubs).
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <initializer_list>
#include "teste_comum.h"
#include "logica.h"
#include "protocolo.h"

int main() {
  // ---- calcularInclinacao: ida e volta com os mesmos angulos do mundo real ----
  const float k = 3.14159265f / 180.0f;
  float p, r;
  calcularInclinacao(0, 0, 1, p, r);
  CHECK_NEAR(p, 0, 0.01); CHECK_NEAR(r, 0, 0.01);
  for (float ang : {-60.f, -30.f, -10.f, 10.f, 30.f, 60.f}) {
    // pitch puro: X aponta para baixo => ax = -sin(pitch)
    calcularInclinacao(-sinf(ang * k), 0, cosf(ang * k), p, r);
    CHECK_NEAR(p, ang, 0.05); CHECK_NEAR(r, 0, 0.05);
    // roll puro: lado direito para baixo => ay = +sin(roll)
    calcularInclinacao(0, sinf(ang * k), cosf(ang * k), p, r);
    CHECK_NEAR(r, ang, 0.05); CHECK_NEAR(p, 0, 0.05);
  }

  // ---- anguloParaPercentual ----
  CHECK(anguloParaPercentual(0, 12, 40, true) == 0);
  CHECK(anguloParaPercentual(12, 12, 40, true) == 0);      // exatamente na borda da zona morta
  CHECK(anguloParaPercentual(-11.9f, 12, 40, true) == 0);
  CHECK(anguloParaPercentual(26, 12, 40, true) == 50);     // metade do caminho
  CHECK(anguloParaPercentual(-26, 12, 40, true) == -50);   // simetria
  CHECK(anguloParaPercentual(40, 12, 40, true) == 100);
  CHECK(anguloParaPercentual(85, 12, 40, true) == 100);    // satura
  CHECK(anguloParaPercentual(-85, 12, 40, true) == -100);
  CHECK(anguloParaPercentual(20, 35, 40, false) == 0);     // modo v1: abaixo do limiar
  CHECK(anguloParaPercentual(36, 35, 40, false) == 100);   // modo v1: liga/desliga
  CHECK(anguloParaPercentual(-36, 35, 40, false) == -100);
  CHECK(anguloParaPercentual(30, 40, 40, true) == 0);      // config absurda (max == zona): nao divide por zero
  CHECK(anguloParaPercentual(50, 40, 40, true) == 100);

  // ---- misturarDiferencial ----
  float e, d;
  misturarDiferencial(1, 0, e, d);   CHECK_NEAR(e, 1, 1e-6); CHECK_NEAR(d, 1, 1e-6);    // reta
  misturarDiferencial(-1, 0, e, d);  CHECK_NEAR(e, -1, 1e-6); CHECK_NEAR(d, -1, 1e-6);  // re
  misturarDiferencial(0, 1, e, d);   CHECK_NEAR(e, 1, 1e-6); CHECK_NEAR(d, -1, 1e-6);   // gira p/ direita no lugar
  misturarDiferencial(0, -1, e, d);  CHECK_NEAR(e, -1, 1e-6); CHECK_NEAR(d, 1, 1e-6);   // gira p/ esquerda no lugar
  misturarDiferencial(1, 1, e, d);   CHECK_NEAR(e, 1, 1e-6); CHECK_NEAR(d, 0, 1e-6);    // curva fechada p/ direita
  misturarDiferencial(1, -1, e, d);  CHECK_NEAR(e, 0, 1e-6); CHECK_NEAR(d, 1, 1e-6);    // curva fechada p/ esquerda
  misturarDiferencial(0.5f, 0.2f, e, d); CHECK_NEAR(e, 0.7, 1e-6); CHECK_NEAR(d, 0.3, 1e-6);
  misturarDiferencial(0.9f, 0.9f, e, d); CHECK(fabsf(e) <= 1.0f && fabsf(d) <= 1.0f); CHECK(e > d);
  misturarDiferencial(0, 0, e, d);   CHECK_NEAR(e, 0, 1e-6); CHECK_NEAR(d, 0, 1e-6);
  // Nunca estoura, para qualquer combinacao
  for (float a = -1; a <= 1.001f; a += 0.1f)
    for (float s = -1; s <= 1.001f; s += 0.1f) {
      misturarDiferencial(a, s, e, d);
      CHECK(fabsf(e) <= 1.0001f && fabsf(d) <= 1.0001f);
    }

  // ---- aproximar (rampa) ----
  CHECK_NEAR(aproximar(0, 1, 0.1f), 0.1, 1e-6);
  CHECK_NEAR(aproximar(0.95f, 1, 0.1f), 1, 1e-6);   // nao passa do alvo
  CHECK_NEAR(aproximar(1, -1, 0.1f), 0.9, 1e-6);
  CHECK_NEAR(aproximar(0.3f, 0.3f, 0.1f), 0.3, 1e-6);

  // ---- velocidadeParaPwm ----
  CHECK(velocidadeParaPwm(0.0f, 80, 255) == 0);
  CHECK(velocidadeParaPwm(0.01f, 80, 255) == 0);    // abaixo do limiar: parado
  CHECK(velocidadeParaPwm(0.02f, 80, 255) >= 80);
  CHECK(velocidadeParaPwm(1.0f, 80, 255) == 255);
  CHECK(velocidadeParaPwm(2.0f, 80, 255) == 255);   // satura
  CHECK(velocidadeParaPwm(0.5f, 80, 255) == 168);   // 80 + 175*0.5 = 167.5 -> 168

  // ---- protocolo ----
  PacoteControle q;
  q.magic = PROTOCOLO_MAGIC; q.versao = PROTOCOLO_VERSAO; q.sequencia = 7;
  q.acelerador = 64; q.direcao = -20; q.flags = FLAG_HABILITADO;
  q.checksum = calcularChecksum(q);
  CHECK(sizeof(PacoteControle) == 7);
  CHECK(pacoteValido(q));
  for (size_t i = 0; i < sizeof(q) - 1; i++) {      // corromper QUALQUER byte invalida o pacote
    PacoteControle c = q;
    reinterpret_cast<uint8_t *>(&c)[i] ^= 0x10;
    CHECK(!pacoteValido(c));
  }
  PacoteControle f = q; f.acelerador = 101; f.checksum = calcularChecksum(f);
  CHECK(!pacoteValido(f));                           // fora da faixa mesmo com checksum certo
  f = q; f.direcao = -101; f.checksum = calcularChecksum(f);
  CHECK(!pacoteValido(f));
  f = q; f.versao = 1; f.checksum = calcularChecksum(f);
  CHECK(!pacoteValido(f));                           // versao antiga (pacote cru do v1 tinha 3 bytes)

  return resumo("logica+protocolo");
}
