#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <string.h>

// UART2 do ESP32
#define RX 16
#define TX 17

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

// COMUNICAÇÃO SERIAL COM ARDUINO NANO
HardwareSerial serialNano(2);
String linha = "";

float temperatura = -1;
float umidade = -1;
int chuva = 0;
int gas = 0;

bool dadosRecebidos = false;
unsigned long ultimaRecepcao = 0;

// PINOS
const int buzzer = 26;

// OBJETOS
MFRC522 rfid(PINO_SDA, PINO_RST);

// UID DO CARTAO AUTORIZADO
byte uidAutorizado[] = {0x8B, 0x10, 0xF3, 0x06};

// CHAVE PADRAO DO RFID
MFRC522::MIFARE_Key chave;

// DADOS EXTRAIDOS DO CARTAO
char nomeCartao[17] = "";
char pinCartao[17] = "";

// PIN AUTORIZADO
const char pinAutorizado[] = "Shaolin@308_";

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

  atualizarDisplay();
}


// VERIFICA SE EXISTE UM CARTAO E LE SEUS DADOS
bool verificarCartao() {

  if (!rfid.PICC_IsNewCardPresent()) {
    return false;
  }

  if (!rfid.PICC_ReadCardSerial()) {
    return false;
  }

  Serial.print("UID: ");

  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.print(rfid.uid.uidByte[i], HEX);
    Serial.print(" ");
  }

  Serial.println();

  // LIMPA OS DADOS ANTERIORES
  memset(nomeCartao, 0, sizeof(nomeCartao));
  memset(pinCartao, 0, sizeof(pinCartao));

  autorizado = false;
  erroLeitura = false;

  // AUTENTICA O SETOR 1
  MFRC522::StatusCode status;

  status = rfid.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A,
    4,
    &chave,
    &rfid.uid
  );

  if (status != MFRC522::STATUS_OK) {

    Serial.println("Erro na autenticacao!");

    Serial.print("Erro RFID: ");
    Serial.println(rfid.GetStatusCodeName(status));

    erroLeitura = true;

  } else {

    byte buffer[18];
    byte tamanho;

    // LE O NOME - BLOCO 4
    tamanho = sizeof(buffer);

    status = rfid.MIFARE_Read(4, buffer, &tamanho);

    if (status == MFRC522::STATUS_OK) {

      memcpy(nomeCartao, buffer, 16);
      nomeCartao[16] = '\0';

      // REMOVE ESPACOS DO FINAL
      for (int i = 15; i >= 0 && nomeCartao[i] == ' '; i--) {
        nomeCartao[i] = '\0';
      }

      Serial.print("Nome: ");
      Serial.println(nomeCartao);

    } else {

      Serial.println("Erro ao ler o nome!");

      Serial.print("Erro RFID: ");
      Serial.println(rfid.GetStatusCodeName(status));

      erroLeitura = true;
    }


    // LE O PIN - BLOCO 6
    if (!erroLeitura) {

      tamanho = sizeof(buffer);

      status = rfid.MIFARE_Read(6, buffer, &tamanho);

      if (status == MFRC522::STATUS_OK) {

        memcpy(pinCartao, buffer, 16);
        pinCartao[16] = '\0';

        // REMOVE ESPACOS DO FINAL
        for (int i = 15; i >= 0 && pinCartao[i] == ' '; i--) {
          pinCartao[i] = '\0';
        }

        Serial.print("PIN: ");
        Serial.println(pinCartao);

      } else {

        Serial.println("Erro ao ler o PIN!");

        Serial.print("Erro RFID: ");
        Serial.println(rfid.GetStatusCodeName(status));

        erroLeitura = true;
      }
    }
  }

  // FINALIZA A COMUNICACAO COM O CARTAO
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  // VERIFICA O PIN
  if (!erroLeitura) {

    if (strcmp(pinCartao, pinAutorizado) == 0) {

      autorizado = true;

      Serial.println("PIN CORRETO!");
      Serial.println("ACESSO AUTORIZADO!");

    } else {

      autorizado = false;

      Serial.println("PIN INCORRETO!");
      Serial.println("ACESSO NEGADO!");
    }

  } else {

    autorizado = false;

    Serial.println("FALHA NA LEITURA DO CARTAO!");
  }

  return true;
}

// RECEBIMENTO DAS MENSAGENS DO ARDUINO
void processarMensagem(String mensagem){

  float t, h;
  int c, g;

  int lidos = sscanf(
    mensagem.c_str(),
    "T:%f;H:%f;C:%d;G:%d",
    &t, &h, &c, &g
  );

  if(lidos != 4){
    return;
  }

  temperatura = t;
  umidade = h;
  chuva = c;
  gas = g;

  dadosRecebidos = true;
  ultimaRecepcao = millis();

  Serial.println("\nDados recebidos: ");
  Serial.println(mensagem);
  Serial.println("\n");
}

// ATUALIZA OS DADOS NO DISPLAY
void atualizarDisplay() {

  display.clearDisplay();

  display.drawRoundRect(0, 0, 128, 64, 5, SSD1306_WHITE);

  // CABECALHO
  display.fillRoundRect(2, 2, 124, 13, 3, SSD1306_WHITE);

  display.setTextSize(1);
  display.setTextColor(SSD1306_BLACK);

  display.setCursor(8, 5);
  display.print("CENTRAL AMBIENTAL");

  display.fillCircle(115, 8, 3, SSD1306_BLACK);

  // VERIFICAR COMUNICACAO
  display.setTextColor(SSD1306_WHITE);

  if (!dadosRecebidos ||
      millis() - ultimaRecepcao > 5000) {

    display.setCursor(17, 29);
    display.print("AGUARDANDO");

    display.setCursor(17, 40);
    display.print("ARDUINO");

    display.display();
    return;
  }

  // TEMPERATURA
  display.setCursor(7, 20);
  display.print("Temp: ");

  if (temperatura < 0) {
    display.print("--");
  } else {
    display.print(temperatura, 1);
    display.print(" C");
  }

  // UMIDADE
  display.setCursor(7, 30);
  display.print("Umidade: ");

  if (umidade < 0) {
    display.print("--");
  } else {
    display.print(umidade, 1);
    display.print(" %");
  }

  // CHUVA
  display.setCursor(7, 40);
  display.print("Chuva: ");

  if (chuva <= 350) {
    display.print("Sim");
  } 
  else {
    display.print("Nao");
  }

  // GAS MQ2
  display.setCursor(7, 50);
  display.print("Gas MQ2: ");
  display.print(gas);

  // RODAPE
  // display.fillRect(2, 58, 124, 4, SSD1306_WHITE);

  // display.setTextColor(SSD1306_BLACK);
  // display.setCursor(20, 57);
  // display.print("COMUNICACAO OK");

  display.display();
}

// RECEBE OS DADOS PELA SERIAL
void receberDados() {

  while (serialNano.available()) {

    char caractere = serialNano.read();

    if (caractere == '\n') {

      processarMensagem(linha);
      linha = "";

    } else if (caractere != '\r') {

      linha += caractere;
    }
  }
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
  serialNano.begin(9600, SERIAL_8N1, RX, TX);
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

  // CONFIGURA CHAVE PADRAO
  for (byte i = 0; i < 6; i++) {
    chave.keyByte[i] = 0xFF;
  }

  telaAguardando();
  aguardandoDesenhado = true;

  Serial.println("Leitor RFID iniciado!");
}

void loop() {

  atualizarBuzzer();
  receberDados();

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

    atualizarDisplay();

    // if (carregando || !aguardandoDesenhado) {

    //   carregando = false;
    //   telaAguardando();
    //   aguardandoDesenhado = true;
    // }
  }
}