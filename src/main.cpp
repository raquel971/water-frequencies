#include <Arduino.h>
#include <math.h>
#include <avr/interrupt.h>

// LIGACOES
const int sensor = A0;
const int speaker1 = 3;
const int speaker2 = 5;
const int speaker3 = 6;

// AUDIO
const float sampleRate = 15625.0;

uint8_t tabelaSeno[256];

volatile uint32_t faseSom = 0;
volatile uint32_t faseTremolo = 0;

volatile uint32_t passoSom = 0;
volatile uint8_t volume = 0;
volatile uint32_t contador = 0;

// Tremolo de 4 Hz
const uint32_t passoTremolo = 1099512UL;

// SENSOR
int valorSeco = 0;
int valor = 0;
float valorSuave = 0;

float frequenciaSuave = 220;
float volumeSuave = 0;

uint32_t ultimaLeitura = 0;
uint32_t ultimoMonitor = 0;

// GERAR AUDIO
ISR(TIMER1_COMPA_vect) {

  contador++;

  faseSom += passoSom;
  faseTremolo += passoTremolo;

  // Onda sinusoidal
  int16_t onda =
    (int16_t)tabelaSeno[faseSom >> 24] - 128;

  // Tremolo suave
  uint8_t lfo =
    tabelaSeno[faseTremolo >> 24];

  uint8_t envelope = 45 + (lfo >> 1);

  // Controlar amplitude
  uint8_t amplitude =
    ((uint16_t)envelope * volume) >> 8;

  int16_t amostra =
    128 + ((onda * amplitude) >> 8);

  uint8_t saida = constrain(amostra, 0, 255);

  // Mesmo som nas 3 colunas
  OCR2B = saida; // D3
  OCR0B = saida; // D5
  OCR0A = saida; // D6
}

void iniciarAudio() {

  noInterrupts();

  // PWM D5 e D6
  TCCR0A =
    _BV(COM0A1) |
    _BV(COM0B1) |
    _BV(WGM01) |
    _BV(WGM00);

  TCCR0B = _BV(CS00);
  TIMSK0 = 0;

  OCR0A = 128;
  OCR0B = 128;

  // PWM D3
  TCCR2A =
    _BV(COM2B1) |
    _BV(WGM21) |
    _BV(WGM20);

  TCCR2B = _BV(CS20);
  TIMSK2 = 0;
  OCR2B = 128;

  // Timer de audio
  TCCR1A = 0;

  TCCR1B =
    _BV(WGM12) |
    _BV(CS10);

  OCR1A = 1023;
  TCNT1 = 0;

  TIMSK1 = _BV(OCIE1A);

  interrupts();
}

void setup() {

  Serial.begin(9600);

  pinMode(speaker1, OUTPUT);
  pinMode(speaker2, OUTPUT);
  pinMode(speaker3, OUTPUT);

  // Criar tabela sinusoidal
  for (int i = 0; i < 256; i++) {

    float angulo =
      2.0 * PI * i / 256.0;

    tabelaSeno[i] =
      128 + 127 * sin(angulo);
  }

  // Calibrar sensor seco
  Serial.println("Manter sensor seco!");

  delay(1000);

  long soma = 0;

  for (int i = 0; i < 50; i++) {
    soma += analogRead(sensor);
    delay(10);
  }

  valorSeco = soma / 50;
  valorSuave = valorSeco;

  passoSom =
    (uint32_t)(220.0 * 4294967296.0 / sampleRate);

  iniciarAudio();

  Serial.println("Sistema pronto!");
}

void loop() {

  uint32_t agora;

  noInterrupts();
  agora = contador;
  interrupts();

  // Ler sensor aproximadamente
  // a cada 40 milissegundos
  if (agora - ultimaLeitura >= 625) {

    ultimaLeitura = agora;

    valor = analogRead(sensor);

    // Suavizar leitura da agua
    valorSuave =
      valorSuave * 0.8 + valor * 0.2;

    int diferenca =
      abs((int)valorSuave - valorSeco);

    int intensidade =
      constrain(diferenca, 0, 100);

    // Agua controla a frequencia
    float frequenciaAlvo =
      180 + intensidade * 1.6;

    frequenciaSuave +=
      (frequenciaAlvo - frequenciaSuave)
      * 0.08;

    // Agua controla ativacao do som
    float volumeAlvo =
      diferenca > 6 ? 210 : 0;

    // Fade suave
    volumeSuave +=
      (volumeAlvo - volumeSuave) * 0.12;

    uint32_t novoPasso =
      (uint32_t)(
        frequenciaSuave *
        4294967296.0 / sampleRate
      );

    noInterrupts();

    passoSom = novoPasso;
    volume = (uint8_t)volumeSuave;

    interrupts();
  }

  // Monitor de valores
  if (agora - ultimoMonitor >= 3906) {

    ultimoMonitor = agora;

    Serial.print("Agua: ");
    Serial.print(valor);

    Serial.print(" | Frequencia: ");
    Serial.print(frequenciaSuave);

    Serial.println(" Hz");
  }
}