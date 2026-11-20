// ============================================================================
//  logica.h  -  ESP HAND CONTROL 2.0
//
//  A "matematica" do projeto, em funcoes PURAS (sem hardware, sem Arduino).
//  Isso e proposital: funcao pura se testa no computador, sem placa nenhuma.
//  (Veja tools/teste_host/ - la essas funcoes sao testadas de verdade.)
//
//  Copia oficial em firmware/comum/ - use tools/sincronizar_comum.sh.
// ============================================================================
#pragma once
#include <math.h>
#include <stdint.h>

constexpr float RAD_PARA_GRAUS = 57.29577951308232f;

inline float limitarF(float v, float minimo, float maximo) {
  return v < minimo ? minimo : (v > maximo ? maximo : v);
}

// ----------------------------------------------------------------------------
//  1) Aceleracao (em g) -> inclinacao da mao (em graus)
//
//  Convencao de montagem: MPU6050 com os componentes para CIMA e a seta "X"
//  do modulo apontando para a ponta dos dedos.
//    pitch > 0  : dedos para BAIXO  (mao "empurrando" para a frente)
//    roll  > 0  : lado direito da mao para BAIXO (inclinar para a direita)
//
//  Por que isso funciona? Parada, a unica aceleracao que o sensor sente e a
//  gravidade. Inclinar a mao muda COMO a gravidade se reparte entre os eixos
//  X, Y e Z; o arco-tangente recupera o angulo dessa divisao.
//  Limite honesto: com a mao ACELERANDO (um tranco), o sensor mistura
//  gravidade com movimento - por isso filtramos o sinal no sketch da luva.
// ----------------------------------------------------------------------------
inline void calcularInclinacao(float ax, float ay, float az, float &pitchGraus, float &rollGraus) {
  pitchGraus = atan2f(-ax, sqrtf(ay * ay + az * az)) * RAD_PARA_GRAUS;
  rollGraus  = atan2f(ay, az) * RAD_PARA_GRAUS;
}

// ----------------------------------------------------------------------------
//  2) Angulo -> comando em porcentagem (-100..+100)
//
//  zonaMorta : abaixo disso, nada acontece (a mao nunca fica 100% parada).
//  anguloMax : a partir daqui, comando maximo.
//  proporcional:
//      true  -> sobe de forma linear entre zonaMorta e anguloMax
//      false -> "liga/desliga": passou da zona morta, vai a 100% (como o v1)
// ----------------------------------------------------------------------------
inline int8_t anguloParaPercentual(float angulo, float zonaMorta, float anguloMax, bool proporcional) {
  float a = fabsf(angulo);
  if (a <= zonaMorta) return 0;
  float p = 100.0f;
  if (proporcional && anguloMax > zonaMorta) {
    if (a > anguloMax) a = anguloMax;
    p = (a - zonaMorta) / (anguloMax - zonaMorta) * 100.0f;
  }
  int v = (int)lroundf(p);
  if (v > 100) v = 100;
  return (int8_t)(angulo < 0.0f ? -v : v);
}

// ----------------------------------------------------------------------------
//  3) Mistura diferencial ("tank drive")
//
//  Entradas  : acel e dir em -1..+1   (+dir = virar para a direita)
//  Saidas    : esq e dirOut em -1..+1 (velocidade de cada lado)
//
//      esquerda = acel + dir        direita = acel - dir
//
//  Exemplos:  acel=1, dir=0  -> os dois lados a 100%  (reta)
//             acel=0, dir=1  -> esq +1, dir -1        (gira no lugar)
//             acel=1, dir=1  -> esq 1,  dir 0         (curva fechada)
//  Se algum lado passar de +-1, os dois sao reduzidos juntos (mantem a "forma"
//  da curva em vez de cortar o excesso).
// ----------------------------------------------------------------------------
inline void misturarDiferencial(float acel, float dir, float &esq, float &dirOut) {
  float e = acel + dir;
  float d = acel - dir;
  float m = fmaxf(fabsf(e), fabsf(d));
  if (m > 1.0f) { e /= m; d /= m; }
  esq = e;
  dirOut = d;
}

// ----------------------------------------------------------------------------
//  4) Rampa de aceleracao: anda "atual" na direcao de "alvo", no maximo
//  "passo" por chamada. Protege engrenagens, o L298N e a bateria contra
//  tranco (o pico de corrente de um tranco pode derrubar a tensao e resetar
//  o ESP32).
// ----------------------------------------------------------------------------
inline float aproximar(float atual, float alvo, float passo) {
  if (alvo > atual + passo) return atual + passo;
  if (alvo < atual - passo) return atual - passo;
  return alvo;
}

// ----------------------------------------------------------------------------
//  5) Velocidade (0..1) -> duty de PWM, pulando a "zona preguicosa" do motor.
//  Motor DC de brinquedo nao gira com 5% de PWM: ele zumbe e nao anda.
//  Entao mapeamos 0..1 para [pwmMin..pwmMax], e 0 continua sendo 0.
// ----------------------------------------------------------------------------
inline uint8_t velocidadeParaPwm(float u, uint8_t pwmMin, uint8_t pwmMax) {
  if (u < 0.02f) return 0;
  if (u > 1.0f) u = 1.0f;
  return (uint8_t)lroundf((float)pwmMin + (float)(pwmMax - pwmMin) * u);
}
