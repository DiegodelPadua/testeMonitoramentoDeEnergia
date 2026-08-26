#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <PZEM004Tv30.h>

// ================================
// WIFI
// ================================

const char* ssid = "Diego jogador";
const char* password = "Rosalina10**";


// ================================
// PZEM
// ================================

PZEM004Tv30 pzem(
    Serial2,
    16,
    17
);


// ================================
// SERVIDOR WEB
// ================================

WebServer server(80);


// ================================
// ENVIA O DASHBOARD
// ================================

void enviarPagina()
{
    File arquivo = LittleFS.open(
        "/index.html",
        "r"
    );

    server.streamFile(
        arquivo,
        "text/html"
    );

    arquivo.close();
}


// ================================
// ENVIA O CSS
// ================================

void enviarCSS()
{
    File arquivo = LittleFS.open(
        "/style.css",
        "r"
    );

    server.streamFile(
        arquivo,
        "text/css"
    );

    arquivo.close();
}


// ================================
// ENVIA JAVASCRIPT
// ================================

void enviarJavaScript()
{
    File arquivo = LittleFS.open(
        "/script.js",
        "r"
    );

    server.streamFile(
        arquivo,
        "application/javascript"
    );

    arquivo.close();
}


// ================================
// ROTA DOS DADOS DO PZEM
// ================================

void enviarDados()
{
    float tensao =
        pzem.voltage();

    float corrente =
        pzem.current();

    float potencia =
        pzem.power();

    float energia =
        pzem.energy();

    float frequencia =
        pzem.frequency();

    float fatorPotencia =
        pzem.pf();


    String json = "{";

    json += "\"tensao\":";
    json += String(tensao, 1);

    json += ",";

    json += "\"corrente\":";
    json += String(corrente, 3);

    json += ",";

    json += "\"potencia\":";
    json += String(potencia, 1);

    json += ",";

    json += "\"energia\":";
    json += String(energia, 3);

    json += ",";

    json += "\"frequencia\":";
    json += String(frequencia, 1);

    json += ",";

    json += "\"fp\":";
    json += String(fatorPotencia, 2);

    json += "}";


    server.send(
        200,
        "application/json",
        json
    );
}


// ================================
// SETUP
// ================================

void setup()
{
    // Inicia o monitor serial
    Serial.begin(115200);

    delay(1000);


    // ============================
    // LITTLEFS
    // ============================

    if (LittleFS.begin(true))
    {
        Serial.println("LittleFS iniciado!");
    }
    else
    {
        Serial.println("Erro ao iniciar LittleFS!");
    }


    // ============================
    // WIFI
    // ============================

    Serial.println();
    Serial.println("==========================");
    Serial.println("      TESTE DE WI-FI      ");
    Serial.println("==========================");

    Serial.print("Tentando conectar em: ");
    Serial.println(ssid);


    // Inicia conexão com a rede
    WiFi.begin(
        ssid,
        password
    );


    // Vamos esperar no máximo 10 segundos
    // pela conexão Wi-Fi.

    int tentativas = 0;


    while (
        WiFi.status() != WL_CONNECTED
        &&
        tentativas < 20
    )
    {
        delay(500);

        Serial.print(".");

        tentativas++;
    }


    Serial.println();


    // ============================
    // VERIFICA SE CONECTOU
    // ============================

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println();
        Serial.println("Wi-Fi CONECTADO!");

        Serial.print("IP do ESP32: ");
        Serial.println(
            WiFi.localIP()
        );

        Serial.print("Sinal Wi-Fi: ");
        Serial.print(
            WiFi.RSSI()
        );

        Serial.println(" dBm");
    }
    else
    {
        Serial.println();
        Serial.println("Wi-Fi NAO CONECTADO!");
        Serial.println("Verifique o nome da rede e a senha.");
    }


    // ============================
    // ROTAS WEB
    // ============================

    server.on(
        "/",
        HTTP_GET,
        enviarPagina
    );


    server.on(
        "/style.css",
        HTTP_GET,
        enviarCSS
    );


    server.on(
        "/script.js",
        HTTP_GET,
        enviarJavaScript
    );


    server.on(
        "/dados",
        HTTP_GET,
        enviarDados
    );


    // ============================
    // SERVIDOR
    // ============================

    // Só inicia o servidor se
    // estiver conectado ao Wi-Fi.

    if (WiFi.status() == WL_CONNECTED)
    {
        server.begin();

        Serial.println(
            "Servidor iniciado!"
        );
    }
}


// ================================
// LOOP
// ================================

void loop()
{
    // Só tenta atender clientes
    // se estiver conectado ao Wi-Fi.

    if (WiFi.status() == WL_CONNECTED)
    {
        server.handleClient();
    }
}