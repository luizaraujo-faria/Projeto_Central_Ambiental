#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define PINO_SDA 5
#define PINO_RST 27

#define LARGURA 128
#define ALTURA 64
#define OLED_RESET -1
#define ENDERECO_OLED 0x3C

Adafruit_SSD1306 display(
  LARGURA,
  ALTURA,
  &Wire,
  OLED_RESET
);

// PINOS
const int buzzer = 26;

// OBJETOS
MFRC522 rfid(PINO_SDA, PINO_RST);

// UID DO CARTAO AUTORIZADO
byte uidAutorizado[] = {0x8B, 0x10, 0xF3, 0x06};

// CONTROLE DO SISTEMA
unsigned long tempoInicial = 0;
unsigned long ultimaAtualizacao = 0;
unsigned long tempoBuzzer = 0;

bool carregando = false;
bool erroLeitura = false;
bool autorizado = false;
bool sistemaIniciado = false;
bool aguardandoDesenhado = false;
bool buzzerAtivo = false;

// ------------------ //
// FUNÇÕES DO SISTEMA //
// ------------------ //

// TELA DE AGUARDO
void telaAguardando() {

  display.clearDisplay();

  // CABEÇALHO
  display.drawRoundRect(0, 0, 128, 64, 5, SSD1306_WHITE);

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(8, 7);
  display.print("CENTRAL AMBIENTAL");

  display.drawLine(5, 19, 123, 19, SSD1306_WHITE);

  // ÍCONE CENTRAL
  display.drawCircle(64, 34, 8, SSD1306_WHITE);
  display.fillCircle(64, 34, 3, SSD1306_WHITE);

  // MENSAGEM
  display.setCursor(5, 48);
  display.print("Aproxime seu cracha");

  display.display();
}


// TELA DE CARREGAMENTO
void telaCarregamento(int progresso) {

  display.clearDisplay();

  // CABEÇALHO
  display.drawRoundRect(0, 0, 128, 64, 5, SSD1306_WHITE);

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(8, 7);
  display.print("CENTRAL AMBIENTAL");

  display.drawLine(5, 19, 123, 19, SSD1306_WHITE);

  // MENSAGEM DE INICIALIZAÇÃO
  display.setCursor(17, 25);
  display.print("Verificando...");

  // BARRA DE CARREGAMENTO
  display.drawRoundRect(10, 40, 108, 12, 3, SSD1306_WHITE);

  display.fillRoundRect(
    12, 42,
    (104 * progresso) / 100,
    8, 2,
    SSD1306_WHITE
  );

  // PORCENTAGEM
  display.setCursor(50, 55);
  display.print(progresso);
  display.print("%");

  display.display();
}


// TELA DE ACESSO NEGADO
void telaAcessoNegado() {

  display.clearDisplay();

  // BORDA
  display.drawRoundRect(0, 0, 128, 64, 5, SSD1306_WHITE);

  // CABEÇALHO
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(8, 7);
  display.print("CENTRAL AMBIENTAL");

  display.drawLine(5, 19, 123, 19, SSD1306_WHITE);

  // ICONE DE BLOQUEIO
  display.drawCircle(64, 34, 9, SSD1306_WHITE);
  display.drawLine(58, 28, 70, 40, SSD1306_WHITE);
  display.drawLine(70, 28, 58, 40, SSD1306_WHITE);

  // MENSAGEM
  display.setCursor(31, 49);
  display.print("ACESSO NEGADO");

  display.display();
}


// TELA PRINCIPAL
void telaPrincipal() {

  display.clearDisplay();

  display.drawRoundRect(0, 0, 128, 64, 5, SSD1306_WHITE);

  display.fillRoundRect(2, 2, 124, 13, 3, SSD1306_WHITE);

  display.setTextSize(1);
  display.setTextColor(SSD1306_BLACK);

  display.setCursor(8, 5);
  display.print("CENTRAL AMBIENTAL");

  // INDICADOR DE SISTEMA ATIVO
  display.fillCircle(115, 8, 3, SSD1306_BLACK);

  // STATUS
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(8, 23);
  display.print("SISTEMA INICIADO");

  display.drawLine(5, 34, 123, 34, SSD1306_WHITE);

  // BOAS-VINDAS
  display.setCursor(8, 40);
  display.print("Seja bem-vindo,");

  display.setTextSize(1);
  display.setCursor(8, 48);
  display.print("Shaolin");

  display.display();
}


// VERIFICA SE EXISTE UM CARTAO
bool verificarCartao() {

  if (!rfid.PICC_IsNewCardPresent()) {
    return false;
  }

  if (!rfid.PICC_ReadCardSerial()) {
    return false;
  }

  Serial.print("UID: ");

  // VERIFICA SE O UID E AUTORIZADO
  autorizado = rfid.uid.size == sizeof(uidAutorizado);

  for (byte i = 0; i < rfid.uid.size; i++) {

    Serial.print(rfid.uid.uidByte[i], HEX);
    Serial.print(" ");

    if (i < sizeof(uidAutorizado) &&
        rfid.uid.uidByte[i] != uidAutorizado[i]) {
      autorizado = false;
    }
  }

  Serial.println();

  // FINALIZA A COMUNICACAO COM O CARTAO
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  return true;
}


// CONTROLE DO BUZZER SEM DELAY
void tocarApito(int frequencia, int duracao) {

  tone(buzzer, frequencia);

  tempoBuzzer = millis() + duracao;
  buzzerAtivo = true;
}

void atualizarBuzzer() {

  if (buzzerAtivo && (long)(millis() - tempoBuzzer) >= 0) {

    noTone(buzzer);
    buzzerAtivo = false;
  }
}


// ----------------- //
// SISTEMA PRINCIPAL //
// ----------------- //
void setup() {
  Serial.begin(115200);
  Wire.begin();

  // Inicializa o display I2C
  if (!display.begin(SSD1306_SWITCHCAPVCC, ENDERECO_OLED)) 
  {
    Serial.println("Erro ao iniciar OLED!");
    while (true) {}
  }

  pinMode(buzzer, OUTPUT);

  // INICIALIZA O RFID
  SPI.begin();
  rfid.PCD_Init();

  telaAguardando();
  aguardandoDesenhado = true;

  Serial.println("Leitor RFID iniciado!");
}

void loop() {

  atualizarBuzzer();

  // SISTEMA AINDA NAO INICIADO
  if (!sistemaIniciado) {

    // AGUARDA A LEITURA DE UM CARTAO
    if (!carregando) {

      if (verificarCartao()) {

        carregando = true;

        tempoInicial = millis();
        ultimaAtualizacao = 0;

        tocarApito(1000, 150);
      }
    }

    // SISTEMA EM PROCESSO DE INICIALIZACAO
    else {

      unsigned long tempoDecorrido = millis() - tempoInicial;

      int progresso = (tempoDecorrido * 100UL) / 5000UL;

      if (progresso > 100) {
        progresso = 100;
      }

      // ATUALIZA A TELA A CADA 100 MS
      if (millis() - ultimaAtualizacao >= 100) {

        telaCarregamento(progresso);
        ultimaAtualizacao = millis();
      }

      // VERIFICA SE O CARREGAMENTO TERMINOU
      if (tempoDecorrido >= 5000) {

        carregando = false;

        // CARTAO AUTORIZADO
        if (autorizado) {

          sistemaIniciado = true;

          telaPrincipal();
          tocarApito(4000, 500);
        }

        // CARTAO NAO AUTORIZADO
        else {

          telaAcessoNegado();
          tocarApito(300, 500);

          delay(1500);

          telaAguardando();
        }
      }
    }
  }

  // SISTEMA INICIADO
  else {

    if (carregando || !aguardandoDesenhado) {

      carregando = false;
      telaAguardando();
      aguardandoDesenhado = true;
    }
  }
}