// Simulacao do CARRO: roda o codigo REAL do Passo4 (incluido abaixo), injeta
// pacotes pelo callback do ESP-NOW e le o duty de PWM que sairia nos pinos.
#include "Arduino.h"
#include "WiFi.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "teste_comum.h"
#include TESTE_SKETCH

static const uint8_t ORIGEM[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

static void injetar(int8_t acel, int8_t dir, uint8_t flags, uint8_t seq) {
  PacoteControle p;
  p.magic = PROTOCOLO_MAGIC; p.versao = PROTOCOLO_VERSAO; p.sequencia = seq;
  p.acelerador = acel; p.direcao = dir; p.flags = flags;
  p.checksum = calcularChecksum(p);
  mockReceberPacote(ORIGEM, reinterpret_cast<uint8_t *>(&p), sizeof(p));
}

// Roda loop() ate passar "ms" milissegundos simulados (como o Arduino faria).
static void ciclo(uint32_t ms) {
  uint32_t alvo = g_mundo.agoraMs + ms;
  while (g_mundo.agoraMs < alvo) {          // loop() termina com delay(1), que avanca o relogio
    uint32_t antes = g_mundo.agoraMs;
    loop();
    if (g_mundo.agoraMs == antes) g_mundo.agoraMs++;
  }
}

// A luva manda a cada 20 ms; simula isso por "ms" milissegundos.
static uint8_t g_seq = 0;
static void luvaFala(int8_t acel, int8_t dir, uint8_t flags, uint32_t ms) {
  for (uint32_t t = 0; t < ms; t += 20) { injetar(acel, dir, flags, g_seq++); ciclo(20); }
}

static uint32_t dutyDirFrente()  { return g_mundo.pwm[PINO_MOTOR_DIR_IN1]; }
static uint32_t dutyDirRe()      { return g_mundo.pwm[PINO_MOTOR_DIR_IN2]; }
static uint32_t dutyEsqFrente()  { return g_mundo.pwm[PINO_MOTOR_ESQ_IN1]; }
static uint32_t dutyEsqRe()      { return g_mundo.pwm[PINO_MOTOR_ESQ_IN2]; }
static bool tudoParado() { return !dutyDirFrente() && !dutyDirRe() && !dutyEsqFrente() && !dutyEsqRe(); }

int main() {
  g_mundo.agoraMs = 1000;
  setup();

  // --- boot seguro ---
  CHECK(tudoParado());
  CHECK(WiFi.modo == WIFI_MODE_STA);
  CHECK(g_canalWifi == CANAL_ESPNOW);
  CHECK(g_espnow.cbReceber != nullptr);
#ifdef TX_POWER_PERSONALIZADO
  CHECK(WiFi.potencia == TX_POWER_PERSONALIZADO);
#endif
  ciclo(200);
  CHECK(tudoParado());                       // sem luva: carro parado
  CHECK(g_estado == Estado::SEM_SINAL);

  // --- frente a fundo: rampa e regime ---
  luvaFala(100, 0, FLAG_HABILITADO, 20);
  uint32_t primeiro = dutyDirFrente();
  CHECK(primeiro > 0 && primeiro < 150);     // rampa: NAO foi a 255 de cara
  CHECK(dutyDirRe() == 0 && dutyEsqRe() == 0);
  luvaFala(100, 0, FLAG_HABILITADO, 300);
  CHECK(dutyDirFrente() == 255 && dutyEsqFrente() == 255);
  CHECK(dutyDirRe() == 0 && dutyEsqRe() == 0);
  CHECK(g_estado == Estado::ATIVO);

  // --- re ---
  luvaFala(-100, 0, FLAG_HABILITADO, 500);
  CHECK(dutyDirRe() == 255 && dutyEsqRe() == 255);
  CHECK(dutyDirFrente() == 0 && dutyEsqFrente() == 0);

  // --- gira no lugar para a DIREITA: esq frente, dir re ---
  luvaFala(0, 100, FLAG_HABILITADO, 500);
  CHECK(dutyEsqFrente() == 255 && dutyEsqRe() == 0);
  CHECK(dutyDirRe() == 255 && dutyDirFrente() == 0);

  // --- gira no lugar para a ESQUERDA ---
  luvaFala(0, -100, FLAG_HABILITADO, 500);
  CHECK(dutyEsqRe() == 255 && dutyEsqFrente() == 0);
  CHECK(dutyDirFrente() == 255 && dutyDirRe() == 0);

  // --- curva fechada para a direita: so o lado esquerdo anda ---
  luvaFala(100, 100, FLAG_HABILITADO, 500);
  CHECK(dutyEsqFrente() == 255);
  CHECK(tudoParado() == false && dutyDirFrente() == 0 && dutyDirRe() == 0);

  // --- velocidade proporcional: 50% ---
  luvaFala(50, 0, FLAG_HABILITADO, 500);
  CHECK(dutyDirFrente() == dutyEsqFrente());
  CHECK(dutyDirFrente() == velocidadeParaPwm(0.5f * (LIMITE_VELOCIDADE_PCT / 100.0f), PWM_MINIMO, PWM_MAXIMO));

  // --- LUVA TRAVADA (flag desligada): parada IMEDIATA, sem rampa ---
  luvaFala(100, 0, FLAG_HABILITADO, 300);
  CHECK(dutyDirFrente() == 255);
  luvaFala(100, 0, 0, 20);                   // continua mandando 100 de acelerador, mas travada
  CHECK(tudoParado());
  CHECK(g_estado == Estado::TRAVADO);

  // --- FAILSAFE: luva some (bateria acabou, saiu do alcance) ---
  luvaFala(100, 0, FLAG_HABILITADO, 300);
  CHECK(dutyDirFrente() == 255);
  ciclo(TIMEOUT_SINAL_MS - 100);             // ainda dentro do prazo
  CHECK(dutyDirFrente() > 0);
  ciclo(200);                                // estourou o prazo
  CHECK(tudoParado());
  CHECK(g_estado == Estado::SEM_SINAL);
  luvaFala(100, 0, FLAG_HABILITADO, 20);     // luva volta: carro volta pela rampa (nao a 255)
  CHECK(dutyDirFrente() > 0 && dutyDirFrente() < 150);

  // --- pacotes ruins sao ignorados e contados ---
  ciclo(TIMEOUT_SINAL_MS + 100);
  CHECK(tudoParado());
  uint32_t ruinsAntes = lerRecepcao().ruins;
  {
    PacoteControle p;
    p.magic = PROTOCOLO_MAGIC; p.versao = PROTOCOLO_VERSAO; p.sequencia = 1;
    p.acelerador = 100; p.direcao = 0; p.flags = FLAG_HABILITADO;
    p.checksum = (uint8_t)(calcularChecksum(p) ^ 0xFF);           // checksum errado
    mockReceberPacote(ORIGEM, reinterpret_cast<uint8_t *>(&p), sizeof(p));
    p.checksum = calcularChecksum(p);
    p.magic = 0x00; p.checksum = calcularChecksum(p);             // assinatura errada
    mockReceberPacote(ORIGEM, reinterpret_cast<uint8_t *>(&p), sizeof(p));
    uint8_t cru_v1[3] = {200, 127, 127};                           // pacote cru de 3 bytes do v1
    mockReceberPacote(ORIGEM, cru_v1, 3);
  }
  ciclo(100);
  CHECK(tudoParado());
  CHECK(lerRecepcao().ruins == ruinsAntes + 3);

  // --- contagem de pacotes perdidos pela sequencia ---
  injetar(0, 0, FLAG_HABILITADO, 9);         // linha de base: a partir daqui a sequencia e conhecida
  uint32_t perdAntes = lerRecepcao().perdidos;
  injetar(0, 0, FLAG_HABILITADO, 10);
  injetar(0, 0, FLAG_HABILITADO, 11);
  injetar(0, 0, FLAG_HABILITADO, 15);        // pulou 12, 13, 14
  CHECK(lerRecepcao().perdidos == perdAntes + 3);
  injetar(0, 0, FLAG_HABILITADO, 16);
  CHECK(lerRecepcao().perdidos == perdAntes + 3);
  // volta de 255 para 0 nao conta como perda
  injetar(0, 0, FLAG_HABILITADO, 255);
  uint32_t p1 = lerRecepcao().perdidos;
  injetar(0, 0, FLAG_HABILITADO, 0);
  CHECK(lerRecepcao().perdidos == p1);

  // --- sem tranco nenhum: nunca o duty pula mais que o passo da rampa por ciclo ---
  ciclo(TIMEOUT_SINAL_MS + 100);
  uint32_t anterior = 0; bool pulouDemais = false;
  for (int i = 0; i < 40; i++) {
    injetar(100, 0, FLAG_HABILITADO, g_seq++);
    ciclo(20);
    uint32_t agora = dutyDirFrente();
    uint32_t passoMax = (uint32_t)lroundf((PWM_MAXIMO - PWM_MINIMO) * PASSO_RAMPA) + 1 + (anterior == 0 ? PWM_MINIMO : 0);
    if (agora > anterior + passoMax) pulouDemais = true;
    anterior = agora;
  }
  CHECK(!pulouDemais);

  return resumo("carro");
}
