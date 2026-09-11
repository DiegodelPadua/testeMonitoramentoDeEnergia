#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <PZEM004Tv30.h>

// ==================================================
// CONFIGURAÇÕES DE WI-FI
// ==================================================

const char* ssid = "aapm";
const char* password = "";

// ==================================================
// CONFIGURAÇÃO DO PZEM
// ==================================================

#define PZEM_RX 16
#define PZEM_TX 17

PZEM004Tv30 pzem(
    Serial2,
    PZEM_RX,
    PZEM_TX
);

// ==================================================
// SERVIDOR WEB
// ==================================================

WebServer server(80);

// ==================================================
// CONTROLE DE TEMPO
// ==================================================

unsigned long ultimoDiagnostico = 0;

const unsigned long intervaloDiagnostico = 2000;

// ==================================================
// VARIÁVEIS DE MEDIÇÃO
// ==================================================

float tensao = NAN;
float corrente = NAN;
float potencia = NAN;
float energia = NAN;
float frequencia = NAN;
float fatorPotencia = NAN;

// ==================================================
// FUNÇÃO: VERIFICA SE O PZEM ESTÁ RESPONDENDO
// ==================================================

bool pzemValido()
{
    return !isnan(tensao);
}

// ==================================================
// FUNÇÃO: LEITURA DO PZEM
// ==================================================

void lerPZEM()
{
    tensao = pzem.voltage();
    corrente = pzem.current();
    potencia = pzem.power();
    energia = pzem.energy();
    frequencia = pzem.frequency();
    fatorPotencia = pzem.pf();
}

// ==================================================
// FUNÇÃO: DIAGNÓSTICO NO SERIAL
// ==================================================

void mostrarDiagnostico()
{
    Serial.println();
    Serial.println("========================================");
    Serial.println("       DIAGNOSTICO DO SISTEMA");
    Serial.println("========================================");

    // --------------------------------------------
    // WIFI
    // --------------------------------------------

    Serial.println();
    Serial.println("[ WIFI ]");

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("Status: CONECTADO");

        Serial.print("SSID: ");
        Serial.println(WiFi.SSID());

        Serial.print("IP: ");
        Serial.println(WiFi.localIP());

        Serial.print("RSSI: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
    }
    else
    {
        Serial.println("Status: DESCONECTADO");
    }

    // --------------------------------------------
    // PZEM
    // --------------------------------------------

    Serial.println();
    Serial.println("[ PZEM-004T ]");

    Serial.print("RX ESP32: GPIO ");
    Serial.println(PZEM_RX);

    Serial.print("TX ESP32: GPIO ");
    Serial.println(PZEM_TX);

    if (!pzemValido())
    {
        Serial.println("Status: SEM RESPOSTA");
        Serial.println("ERRO: PZEM retornou NaN");

        Serial.println();
        Serial.println("Verifique:");

        Serial.println(
            "1 - PZEM TX -> GPIO 25"
        );

        Serial.println(
            "2 - PZEM RX -> GPIO 26"
        );

        Serial.println(
            "3 - GND comum"
        );

        Serial.println(
            "4 - Alimentacao da interface"
        );

        Serial.println(
            "5 - PZEM ligado corretamente a rede AC"
        );
    }
    else
    {
        Serial.println("Status: OK");

        Serial.print("Tensao: ");
        Serial.print(tensao, 1);
        Serial.println(" V");

        Serial.print("Corrente: ");
        Serial.print(corrente, 3);
        Serial.println(" A");

        Serial.print("Potencia: ");
        Serial.print(potencia, 1);
        Serial.println(" W");

        Serial.print("Energia: ");
        Serial.print(energia, 3);
        Serial.println(" kWh");

        Serial.print("Frequencia: ");
        Serial.print(frequencia, 1);
        Serial.println(" Hz");

        Serial.print("FP: ");
        Serial.println(
            fatorPotencia,
            2
        );
    }

    Serial.println();
    Serial.println("========================================");
}

// ==================================================
// FUNÇÃO: ENVIA ARQUIVO HTML
// ==================================================

void enviarPagina()
{
    Serial.println(
        "[HTTP] GET /"
    );

    if (!LittleFS.exists("/index.html"))
    {
        Serial.println(
            "[ERRO] index.html nao encontrado"
        );

        server.send(
            404,
            "text/plain",
            "index.html nao encontrado"
        );

        return;
    }

    File arquivo =
        LittleFS.open(
            "/index.html",
            "r"
        );

    server.streamFile(
        arquivo,
        "text/html"
    );

    arquivo.close();
}

// ==================================================
// FUNÇÃO: ENVIA CSS
// ==================================================

void enviarCSS()
{
    Serial.println(
        "[HTTP] GET /style.css"
    );

    if (!LittleFS.exists("/style.css"))
    {
        server.send(
            404,
            "text/plain",
            "style.css nao encontrado"
        );

        return;
    }

    File arquivo =
        LittleFS.open(
            "/style.css",
            "r"
        );

    server.streamFile(
        arquivo,
        "text/css"
    );

    arquivo.close();
}

// ==================================================
// FUNÇÃO: ENVIA JAVASCRIPT
// ==================================================

void enviarJavaScript()
{
    Serial.println(
        "[HTTP] GET /script.js"
    );

    if (!LittleFS.exists("/script.js"))
    {
        server.send(
            404,
            "text/plain",
            "script.js nao encontrado"
        );

        return;
    }

    File arquivo =
        LittleFS.open(
            "/script.js",
            "r"
        );

    server.streamFile(
        arquivo,
        "application/javascript"
    );

    arquivo.close();
}

// ==================================================
// ROTA /DADOS
// ==================================================

void enviarDados()
{
    Serial.println();
    Serial.println(
        "[HTTP] GET /dados"
    );

    lerPZEM();

    // --------------------------------------------
    // VERIFICA ERRO DO PZEM
    // --------------------------------------------

    if (
        isnan(tensao) ||
        isnan(corrente) ||
        isnan(potencia) ||
        isnan(energia) ||
        isnan(frequencia) ||
        isnan(fatorPotencia)
    )
    {
        Serial.println(
            "[HTTP] ERRO 503 - PZEM sem resposta"
        );

        String erro = "{";

        erro += "\"status\":\"erro\",";
        erro += "\"mensagem\":\"PZEM sem resposta\"";

        erro += "}";

        server.send(
            503,
            "application/json",
            erro
        );

        return;
    }

    // --------------------------------------------
    // CRIA JSON
    // --------------------------------------------

    String json = "{";

    json += "\"status\":\"ok\",";

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
    json += String(
        fatorPotencia,
        2
    );

    json += "}";

    // --------------------------------------------
    // DEBUG HTTP
    // --------------------------------------------

    Serial.println(
        "[HTTP] Resposta 200"
    );

    Serial.println(
        "[HTTP] JSON:"
    );

    Serial.println(json);

    // --------------------------------------------
    // ENVIA RESPOSTA
    // --------------------------------------------

    server.send(
        200,
        "application/json",
        json
    );
}

// ==================================================
// ROTA /STATUS
// ==================================================

void enviarStatus()
{
    Serial.println(
        "[HTTP] GET /status"
    );

    String json = "{";

    json += "\"wifi\":";

    if (WiFi.status() == WL_CONNECTED)
    {
        json += "\"conectado\"";
    }
    else
    {
        json += "\"desconectado\"";
    }

    json += ",";

    json += "\"ip\":\"";
    json += WiFi.localIP().toString();
    json += "\",";

    json += "\"rssi\":";
    json += String(
        WiFi.RSSI()
    );

    json += ",";

    json += "\"pzem\":";

    if (pzemValido())
    {
        json += "\"ok\"";
    }
    else
    {
        json += "\"erro\"";
    }

    json += "}";

    server.send(
        200,
        "application/json",
        json
    );
}

// ==================================================
// ROTA NÃO ENCONTRADA
// ==================================================

void rotaNaoEncontrada()
{
    Serial.print(
        "[HTTP] 404: "
    );

    Serial.println(
        server.uri()
    );

    server.send(
        404,
        "text/plain",
        "Rota nao encontrada"
    );
}

// ==================================================
// CONECTAR WIFI
// ==================================================

void conectarWiFi()
{
    Serial.println();
    Serial.println(
        "========================================"
    );

    Serial.println(
        "         CONECTANDO AO WI-FI"
    );

    Serial.println(
        "========================================"
    );

    Serial.print(
        "Rede: "
    );

    Serial.println(ssid);

    WiFi.mode(
        WIFI_STA
    );

    WiFi.begin(
        ssid,
        password
    );

    int tentativas = 0;

    while (
        WiFi.status() != WL_CONNECTED &&
        tentativas < 20
    )
    {
        delay(500);

        Serial.print(".");

        tentativas++;
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println(
            "Wi-Fi CONECTADO!"
        );

        Serial.print(
            "IP do ESP32: "
        );

        Serial.println(
            WiFi.localIP()
        );

        Serial.print(
            "RSSI: "
        );

        Serial.print(
            WiFi.RSSI()
        );

        Serial.println(
            " dBm"
        );
    }
    else
    {
        Serial.println(
            "ERRO: Wi-Fi nao conectado"
        );
    }
}

// ==================================================
// SETUP
// ==================================================

void setup()
{
    // --------------------------------------------
    // SERIAL
    // --------------------------------------------

    Serial.begin(
        115200
    );

    delay(1500);

    Serial.println();
    Serial.println(
        "========================================"
    );

    Serial.println(
        " SISTEMA DE MONITORAMENTO DE ENERGIA"
    );

    Serial.println(
        " ESP32 + PZEM-004T V3.0"
    );

    Serial.println(
        "========================================"
    );

    // --------------------------------------------
    // LITTLEFS
    // --------------------------------------------

    Serial.println();
    Serial.println(
        "[ LITTLEFS ]"
    );

    if (LittleFS.begin(true))
    {
        Serial.println(
            "LittleFS iniciado com sucesso"
        );
    }
    else
    {
        Serial.println(
            "ERRO ao iniciar LittleFS"
        );
    }

    // --------------------------------------------
    // WIFI
    // --------------------------------------------

    conectarWiFi();

    // --------------------------------------------
    // PRIMEIRO TESTE PZEM
    // --------------------------------------------

    Serial.println();
    Serial.println(
        "[ TESTE INICIAL DO PZEM ]"
    );

    lerPZEM();

    if (pzemValido())
    {
        Serial.println(
            "PZEM RESPONDEU!"
        );

        Serial.print(
            "Tensao inicial: "
        );

        Serial.print(
            tensao
        );

        Serial.println(
            " V"
        );
    }
    else
    {
        Serial.println(
            "PZEM NAO RESPONDEU"
        );

        Serial.println(
            "Leitura retornou NaN"
        );
    }

    // --------------------------------------------
    // ROTAS HTTP
    // --------------------------------------------

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

    server.on(
        "/status",
        HTTP_GET,
        enviarStatus
    );

    server.onNotFound(
        rotaNaoEncontrada
    );

    // --------------------------------------------
    // INICIA SERVIDOR
    // --------------------------------------------

    if (
        WiFi.status() ==
        WL_CONNECTED
    )
    {
        server.begin();

        Serial.println();
        Serial.println(
            "Servidor HTTP iniciado!"
        );

        Serial.print(
            "Dashboard: http://"
        );

        Serial.println(
            WiFi.localIP()
        );

        Serial.print(
            "Dados: http://"
        );

        Serial.print(
            WiFi.localIP()
        );

        Serial.println(
            "/dados"
        );

        Serial.print(
            "Status: http://"
        );

        Serial.print(
            WiFi.localIP()
        );

        Serial.println(
            "/status"
        );
    }
}

// ==================================================
// LOOP
// ==================================================

void loop()
{
    // --------------------------------------------
    // RECONEXÃO WIFI
    // --------------------------------------------

    if (
        WiFi.status() !=
        WL_CONNECTED
    )
    {
        Serial.println(
            "[WIFI] Conexao perdida"
        );

        Serial.println(
            "[WIFI] Tentando reconectar..."
        );

        WiFi.reconnect();

        delay(1000);
    }

    // --------------------------------------------
    // SERVIDOR HTTP
    // --------------------------------------------

    if (
        WiFi.status() ==
        WL_CONNECTED
    )
    {
        server.handleClient();
    }

    // --------------------------------------------
    // DIAGNÓSTICO PERIÓDICO
    // --------------------------------------------

    unsigned long agora =
        millis();

    if (
        agora - ultimoDiagnostico
        >= intervaloDiagnostico
    )
    {
        ultimoDiagnostico =
            agora;

        lerPZEM();

        mostrarDiagnostico();
    }
}