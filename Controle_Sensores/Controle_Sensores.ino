#include <DHT.h>
#include <SoftwareSerial.h>

#define PIN_DHT 2
#define TIPO_DHT DHT11

#define PIN_CHUVA A1
#define PIN_GAS A0

SoftwareSerial serialESP(10, 11); // RX, TX
DHT dht(PIN_DHT, TIPO_DHT);

unsigned long ultimaLeitura = 0;
const unsigned long INTERVALO = 2000;

void setup() {
  Serial.begin(9600);
  serialESP.begin(9600);

  dht.begin();

  Serial.println("\nSistema de Sensores Iniciado\n");
}

void loop() {

  if(millis() - ultimaLeitura < INTERVALO){
    return;
  }

  ultimaLeitura = millis();
  
  // LE A CHUVA E O GÁS
  int chuva = analogRead(PIN_CHUVA);
  int gas = analogRead(PIN_GAS);

  // LE A TEMPERATURA E UMIDADE
  float temperatura = dht.readTemperature();
  float umidade = dht.readHumidity();

  bool dadosDHTValidos = !isnan(temperatura) && !isnan(umidade);

  float t = dadosDHTValidos ? temperatura : -1;
  float h = dadosDHTValidos ? umidade : -1;

  leituraMonitorSerial(t, h, chuva, gas);

  String mensagem = "T:" + String(t, 1)
                  + ";H:" + String(h, 1)
                  + ";C:" + String(chuva)
                  + ";G:" + String(gas);

  serialESP.println(mensagem);
  Serial.println("\nDados enviados para o ESP");
}

void leituraMonitorSerial(int vT, int vH, float vChuva, float vGas){
  Serial.println("Leitura recebida dos sensores: ");

  Serial.print("Temperatura: ");
  Serial.println(vT);
  Serial.print("Umidade: ");
  Serial.println(vH);
  Serial.print("Chuva: ");
  Serial.println(vChuva);
  Serial.print("Gás: ");
  Serial.println(vGas);
}
