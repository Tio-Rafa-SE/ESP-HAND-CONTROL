// ============================================================================
//  protocolo.h  -  ESP HAND CONTROL 2.0
//
//  O "idioma" que a LUVA fala e o CARRO entende.
//  Este arquivo e IDENTICO nos dois sketches (Passo4 e Passo5).
//  A copia oficial fica em firmware/comum/ - edite so la e rode
//  tools/sincronizar_comum.sh para atualizar as copias.
//
//  Por que um protocolo proprio, se o v1 mandava 3 bytes crus?
//    - magic + versao: o carro ignora pacotes de outros projetos/dispositivos;
//    - checksum: descarta pacote corrompido ou "inventado";
//    - sequencia: permite contar quantos pacotes se perderam no caminho;
//    - flags: a luva pode dizer "estou TRAVADA" e o carro para na hora;
//    - acelerador/direcao em -100..+100: sem depender da orientacao do sensor.
//
//  Obs.: checksum NAO e seguranca contra ataque. E so higiene de dados.
//  Para criptografia de verdade, veja o desafio no manual (PMK/LMK do ESP-NOW).
// ============================================================================
#pragma once
#include <stdint.h>
#include <stddef.h>

// Canal Wi-Fi usado pelo ESP-NOW. Luva e carro PRECISAM usar o mesmo.
// Fica aqui (e nao nos configs) justamente para nunca ficar diferente.
constexpr uint8_t CANAL_ESPNOW = 1;

constexpr uint8_t PROTOCOLO_MAGIC  = 0xA7;  // "assinatura" do ESP HAND CONTROL
constexpr uint8_t PROTOCOLO_VERSAO = 2;     // v1 era o pacote cru de 3 bytes

// Bits do campo "flags"
constexpr uint8_t FLAG_HABILITADO = 0x01;   // 1 = luva liberada; 0 = travada/calibrando

struct __attribute__((packed)) PacoteControle {
  uint8_t magic;       // sempre PROTOCOLO_MAGIC
  uint8_t versao;      // sempre PROTOCOLO_VERSAO
  uint8_t sequencia;   // 0..255, sobe de 1 em 1 a cada envio (da a volta)
  int8_t  acelerador;  // -100 (re a fundo) .. +100 (frente a fundo)
  int8_t  direcao;     // -100 (esquerda)   .. +100 (direita)
  uint8_t flags;       // FLAG_HABILITADO, ...
  uint8_t checksum;    // XOR dos 6 bytes anteriores com semente 0x5A
};

static_assert(sizeof(PacoteControle) == 7, "PacoteControle deve ter exatamente 7 bytes");

inline uint8_t calcularChecksum(const PacoteControle &p) {
  const uint8_t *b = reinterpret_cast<const uint8_t *>(&p);
  uint8_t c = 0x5A;
  for (size_t i = 0; i < sizeof(PacoteControle) - 1; i++) c ^= b[i];
  return c;
}

inline bool pacoteValido(const PacoteControle &p) {
  return p.magic == PROTOCOLO_MAGIC
      && p.versao == PROTOCOLO_VERSAO
      && p.checksum == calcularChecksum(p)
      && p.acelerador >= -100 && p.acelerador <= 100
      && p.direcao   >= -100 && p.direcao   <= 100;
}
