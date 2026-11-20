// Simulacao da LUVA: roda o codigo REAL do Passo5 (incluido abaixo), com um
// MPU6050 "de mentira" (stubs/Wire.h) e captura os pacotes que iriam pelo ar.
#include "Arduino.h"
#include "Wire.h"
#include "WiFi.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "teste_comum.h"
#include TESTE_SKETCH

static void ciclo(uint32_t ms) {
  uint32_t alvo = g_mundo.agoraMs + ms;
  while (g_mundo.agoraMs < alvo) {          // loop() termina com delay(1), que avanca o relogio
    uint32_t antes = g_mundo.agoraMs;
    loop();
    if (g_mundo.agoraMs == antes) g_mundo.agoraMs++;
  }
}

static PacoteControle ultimoPacote() {
  PacoteControle p;
  memset(&p, 0, sizeof(p));
  if (!g_espnow.enviados.empty()) memcpy(&p, g_espnow.enviados.back().data(), sizeof(p));
  return p;
}

static void clicar(uint32_t ms) {
  g_mundo.digital[PINO_BOTAO] = LOW;
  ciclo(ms);
  g_mundo.digital[PINO_BOTAO] = HIGH;
  ciclo(60);
}

int main() {
  g_mundo.agoraMs = 5000;
  Wire.definirInclinacao(0, 0);              // mao plana durante a calibracao
  setup();

  // --- boot: calibrou, travada, peer cadastrado, canal certo ---
  CHECK(g_sensorOk);
  CHECK(g_espnow.peers == 1);
  CHECK(g_canalWifi == CANAL_ESPNOW);
  CHECK_NEAR(g_offsetPitch, 0, 0.5);
  CHECK_NEAR(g_offsetRoll, 0, 0.5);
  CHECK(g_travada == (PINO_BOTAO >= 0 && INICIAR_TRAVADA));
  CHECK(!g_espnow.enviados.empty());          // durante a calibracao ja manda "estou quieta"
  for (auto &pk : g_espnow.enviados) {        // ... e TODOS os pacotes da calibracao sao "desabilitado"
    PacoteControle q; memcpy(&q, pk.data(), sizeof(q));
    CHECK(pacoteValido(q) && !(q.flags & FLAG_HABILITADO) && q.acelerador == 0 && q.direcao == 0);
  }
  { // sempre para o MAC configurado
    const uint8_t *mac = g_espnow.destinos.back().data();
    CHECK(memcmp(mac, MAC_CARRO, 6) == 0);
  }

  // --- travada: mesmo com a mao inclinada, manda zero e flag desligada ---
  Wire.definirInclinacao(30, 0);
  ciclo(500);
  PacoteControle p = ultimoPacote();
  CHECK(pacoteValido(p));
  CHECK(!(p.flags & FLAG_HABILITADO) && p.acelerador == 0 && p.direcao == 0);

  // --- clique curto destrava ---
  clicar(100);
  CHECK(!g_travada);
  p = ultimoPacote();
  CHECK(p.flags & FLAG_HABILITADO);
  CHECK(p.acelerador == 64);                  // (30-12)/(40-12)*100 = 64,3
  CHECK(p.direcao == 0);

  // --- numeracao sequencial: cada pacote = anterior + 1 ---
  {
    bool sequencial = true;
    for (size_t i = 1; i < g_espnow.enviados.size(); i++) {
      uint8_t a = g_espnow.enviados[i - 1][2], b = g_espnow.enviados[i][2];
      if ((uint8_t)(b - a) != 1) sequencial = false;
    }
    CHECK(sequencial);
  }

  // --- taxa de envio: ~50 pacotes por segundo ---
  {
    size_t antes = g_espnow.enviados.size();
    ciclo(1000);
    size_t n = g_espnow.enviados.size() - antes;
    CHECK(n >= 48 && n <= 52);
  }

  // --- dedos para cima = re; direita/esquerda; zona morta; saturacao ---
  Wire.definirInclinacao(-40, 0);   ciclo(500); p = ultimoPacote();
  CHECK(p.acelerador == -100 && p.direcao == 0);
  Wire.definirInclinacao(0, 40);    ciclo(500); p = ultimoPacote();
  CHECK(p.acelerador == 0 && p.direcao == 100);
  Wire.definirInclinacao(0, -26);   ciclo(500); p = ultimoPacote();
  CHECK(p.acelerador == 0 && p.direcao == -50);
  Wire.definirInclinacao(5, -5);    ciclo(500); p = ultimoPacote();
  CHECK(p.acelerador == 0 && p.direcao == 0);                 // dentro da zona morta
  Wire.definirInclinacao(70, 0);    ciclo(500); p = ultimoPacote();
  CHECK(p.acelerador == 100);                                  // satura em 100
  Wire.definirInclinacao(35, 35);   ciclo(500); p = ultimoPacote();
  CHECK(p.acelerador > 0 && p.direcao > 0);                    // diagonal: curva
  CHECK(pacoteValido(p));

  // --- clique curto trava de novo: carro para na hora ---
  clicar(100);
  CHECK(g_travada);
  p = ultimoPacote();
  CHECK(!(p.flags & FLAG_HABILITADO) && p.acelerador == 0 && p.direcao == 0);

  // --- recalibracao (clique longo) com a mao em 8 graus: o 8 vira o "zero" ---
  Wire.definirInclinacao(8, 0);
  ciclo(400);
  clicar(BOTAO_PRESSAO_LONGA_MS + 100);
  CHECK_NEAR(g_offsetPitch, 8, 0.7);
  clicar(100);                                // destrava
  CHECK(!g_travada);
  ciclo(400);
  p = ultimoPacote();
  CHECK(p.acelerador == 0);                   // em 8 graus (o novo neutro) = parado
  Wire.definirInclinacao(38, 0);              // 30 graus acima do neutro novo
  ciclo(500); p = ultimoPacote();
  CHECK(p.acelerador == 64);

  // --- mao tremendo na calibracao: NAO aceita, mantem o neutro anterior ---
  {
    if (!g_travada) clicar(100);               // trava (so para ficar quieta durante o teste)
    float antes = g_offsetPitch;
    static bool alta = false;
    g_mundo.aoPassarTempo = [](uint32_t agora) {
      if (agora % 50 == 0) { alta = !alta; Wire.definirInclinacao(alta ? 12 : -12, 0); }   // +-12 graus: 24 de variacao
    };
    uint32_t t0 = g_mundo.agoraMs;
    clicar(BOTAO_PRESSAO_LONGA_MS + 100);      // dispara a recalibracao
    uint32_t gasto = g_mundo.agoraMs - t0;
    g_mundo.aoPassarTempo = nullptr;
    CHECK_NEAR(g_offsetPitch, antes, 1e-6);    // nao mudou
    CHECK(gasto >= (uint32_t)CALIBRACAO_TENTATIVAS * CALIBRACAO_DURACAO_MS);   // insistiu todas as tentativas
    Wire.definirInclinacao(8, 0);
    ciclo(300);
  }

  // --- falha do sensor (fio solto): manda "parado" e tenta reconectar ---
  clicar(100);                                 // garante estado liberado para o teste
  if (g_travada) clicar(100);
  Wire.definirInclinacao(30, 0);
  ciclo(300);
  Wire.falhar = true;
  ciclo(400);
  CHECK(!g_sensorOk);
  p = ultimoPacote();
  CHECK(!(p.flags & FLAG_HABILITADO) && p.acelerador == 0 && p.direcao == 0);
  Wire.falhar = false;
  ciclo(3500);                                 // reconecta em ate 1 s e recalibra (1,5 s)
  CHECK(g_sensorOk);

  return resumo("luva");
}
