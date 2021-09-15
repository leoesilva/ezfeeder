/*--------------------------------------------------------------------------------------------------------------

@name EZFeeder
@authors Leonardo Silva, Rodrigo Santos
@year 2019

--------------------------------------------------------------------------------------------------------------*/

//  Bibliotecas (Libraries)
#include <Arduino.h>       //  Framework Arduino
#include "FS.h"            //  Sistema de arquivos
#include "SPIFFS.h"        //  Sistema de arquivos SPIFFS
#include <WiFi.h>          //  Conectividade Wireless
#include "NTPClient.h"     //  Horário
#include <WiFiUdp.h>       //  Necessário para conectar ao NTP
#include "ArduinoJson.h"   //  Manipulação de JSON
#include "FirebaseESP32.h" //  Comunicação com Firebase
#include "AccelStepper.h"  //  Controle do motor

/*
  Parâmetros de rede (Network parameters)
*/
#define WIFI_SSID ""     //  Nome da rede
#define WIFI_PASSWORD "" //  Senha da rede

/*
  Parâmetros de horário (Hour parameters)
*/
WiFiUDP udp;                                      //Cria um objeto "UDP".
NTPClient ntp(udp, "pool.ntp.br", -10800, 60000); //Cria um objeto "NTP" com as configurações.

/*
  Parâmetros de conexão com o projeto do Firebase (Connection parameters to Firebase project)
*/
#define FIREBASE_HOST ""         //  Endereço do projeto no Firebase
#define FIREBASE_AUTH ""         //  Segredo do banco de dados
const String FIREBASE_PATH = ""; //  ID do usuário
FirebaseData firebaseData;       // Instanciando objeto da biblioteca

/*
  Motor
*/
AccelStepper stepper(1, 18, 19);

/*
  Estrutura de configuração do agendamento
*/
struct Config
{
  int qtdPorcPadrao;
  String hrPorc1;
  String hrPorc2;
  String hrPorc3;
};

const char *filename = "/config.txt"; // Nome do arquivo
Config config;

const int pushButton = 15;
int pressed = 0;

// Salva as configurações no arquivo
void saveConfiguration(const char *filename, Config &config)
{

  //  Consulta e salva as informações do Firebase na estrutura de configuração

  if (Firebase.getInt(firebaseData, "/users/" + FIREBASE_PATH + "/qtdRacao"))
  {
    if (firebaseData.dataType() == "int")
    {
      config.qtdPorcPadrao = firebaseData.intData() / 50;
    }
    else
    {
      Serial.println(firebaseData.errorReason());
    }
  }

  if (Firebase.getString(firebaseData, "/users/" + FIREBASE_PATH + "/horario1"))
  {
    if (firebaseData.dataType() == "string")
    {
      config.hrPorc1 = firebaseData.stringData();
    }
    else
    {
      Serial.println(firebaseData.errorReason());
    }
  }

  if (Firebase.getString(firebaseData, "/users/" + FIREBASE_PATH + "/horario2"))
  {
    if (firebaseData.dataType() == "string")
    {
      config.hrPorc2 = firebaseData.stringData();
    }
    else
    {
      Serial.println(firebaseData.errorReason());
    }
  }

  if (Firebase.getString(firebaseData, "/users/" + FIREBASE_PATH + "/horario3"))
  {
    if (firebaseData.dataType() == "string")
    {
      config.hrPorc3 = firebaseData.stringData();
    }
    else
    {
      Serial.println(firebaseData.errorReason());
    }
  }

  // Apaga o arquivo existente. Do contrário, irá adicionar informações ao arquivo atual.
  SPIFFS.remove(filename);

  // Abrir arquivo para a escrita
  File file = SPIFFS.open(filename, FILE_WRITE);
  if (!file)
  {
    Serial.println(F("Failed to create file"));
    return;
  }

  // Alocar um JsonDocument temporário
  StaticJsonDocument<128> doc;

  // Definir os valores no documento
  doc["qtdPorcPadrao"] = config.qtdPorcPadrao;
  doc["hrPorc1"] = config.hrPorc1;
  doc["hrPorc2"] = config.hrPorc2;
  doc["hrPorc3"] = config.hrPorc3;

  // Serializar o JSON no arquivo
  if (serializeJson(doc, file) == 0)
  {
    Serial.println(F("Failed to write to file"));
  }

  // Fecha o arquivo
  file.close();
}

void loadConfiguration(const char *filename, Config &config)
{
  // Abrindo arquivo para leitura
  File file = SPIFFS.open(filename);

  // Alocar um JsonDocument temporário
  StaticJsonDocument<256> doc;

  // Deserializar o documento JSON
  DeserializationError error = deserializeJson(doc, file);
  if (error)
    Serial.println(F("Failed to read file, using default configuration"));

  // Copiar os valores do JSON para a estrutura de configuração
  config.qtdPorcPadrao = doc["qtdPorcPadrao"] | 2;
  config.hrPorc1 = doc["hrPorc1"] | "08:00:00";
  config.hrPorc2 = doc["hrPorc2"] | "14:00:00";
  config.hrPorc3 = doc["hrPorc3"] | "20:00:00";

  // Fechar o arquivo
  file.close();
}

void printFile(const char *filename)
{
  // Abrindo arquivo para leitura
  File file = SPIFFS.open(filename);
  if (!file)
  {
    Serial.println(F("Failed to read file"));
    return;
  }

  // Extrair caracteres um a um
  while (file.available())
  {
    Serial.print((char)file.read());
  }
  Serial.println();

  // Fechar o arquivo
  file.close();
}

void feed()
{
  /*
    Função para acionar o motor e servir a ração
  */
  Serial.println("Iniciando a alimentação");
  digitalWrite(5, LOW); //  Habilitar o driver e o motor
  for (int cnt = 0; cnt < config.qtdPorcPadrao; cnt++)
  {
    /*
      Motor faz 200 passos para um giro completo
      Para servir uma porção padrão: 
      16 giros no sentido anti-horário, 3 giros no sentido horário
      Novamente, 16 giros sentido anti-horário e 3 giros sentido horário
    */
    stepper.move(-3200);
    do
    {
      stepper.run();
    } while (stepper.distanceToGo() != 0);
    delay(200);
    stepper.move(600);
    do
    {
      stepper.run();
    } while (stepper.distanceToGo() != 0);
    delay(200);
    stepper.move(-3200);
    do
    {
      stepper.run();
    } while (stepper.distanceToGo() != 0);
    delay(200);
    stepper.move(600);
    do
    {
      stepper.run();
    } while (stepper.distanceToGo() != 0);
    delay(200);
  }
  digitalWrite(5, HIGH); //  Desabilitar driver e motor
  //  Enviar log para o banco de dados
  if (Firebase.pushTimestamp(firebaseData, "/logs/" + FIREBASE_PATH))
  {
    Serial.println("Registrado no log");
  }
  else
  {
    Serial.println(firebaseData.errorReason());
  }
  Serial.println("Finalizando a alimentação");
}

void setup()
{
  //  Iniciando o monitor serial
  Serial.begin(115200);

  //  Configurando o motor
  stepper.setMaxSpeed(700);
  stepper.setAcceleration(15000);
  stepper.disableOutputs();
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH);

  //  Definindo o pino do botão de alimentação manual como entrada
  pinMode(pushButton, INPUT);

  Serial.printf("Conectando à rede %s ", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); //  Conexão à rede Wireless
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
  }
  Serial.println(" CONECTADO");

  ntp.begin();       //Inicia o NTP.
  ntp.forceUpdate(); //Força o Update.

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH); //  Inicializando o Firebase

  //  Verificando o status do sistema de arquivos
  if (!SPIFFS.begin(true))
  {
    Serial.println("SPIFFS initialization failed.");
    return;
  }

  // Operações relacionadas ao arquivo de configuração
  Serial.println("Carregando configuração");
  loadConfiguration(filename, config); //  Abrir o arquivo de configuração
  Serial.println("Gravando configuração");
  saveConfiguration(filename, config); //  Ler e salvar os parâmetros do banco
  Serial.println("Verificando configuração");
  printFile(filename); //  Exibir o agendamento atual
}

void loop()
{
  Serial.println("Hora atual: " + ntp.getFormattedTime()); //  Mostrar hora atual
  loadConfiguration(filename, config);                     //  Carregar as configurações
  pressed = digitalRead(pushButton);                       //  Ler o estado atual do botão
  /*  
  Verificar se a hora atual corresponde a algum agendamento;
  caso corresponda, acionar a função de alimentação
  */
  if (ntp.getFormattedTime() == config.hrPorc1 || ntp.getFormattedTime() == config.hrPorc2 || ntp.getFormattedTime() == config.hrPorc3)
  {
    feed();
  }
  /*  
  Verificar se o botão foi pressionado;
  caso tenha sido, acionar a função de alimentação
  */
  if (pressed == HIGH)
  {
    Serial.println("Botão pressionado");
    feed();
  }
  delay(200);
}
