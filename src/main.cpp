#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <PZEM004Tv30.h>
#include <PubSubClient.h>
#include <time.h>
// ==================================================
// CONFIGURAÇÕES DE WI-FI
// ==================================================

const char* ssid = "Diego jogador";
const char* password = "Rosalina10**";

// ==================================================
// CONFIGURAÇÃO DO PZEM
// ==================================================

#define PZEM_RX 16
#define PZEM_TX 17

// ==================================================
// IDENTIFICAÇÃO DO PZEM
// ==================================================

// ID deste PZEM cadastrado no banco de dados.
// Neste protótipo, este conjunto ESP32 + PZEM
// corresponde ao registro ID 1 da tbl_pzem.
const int ID_PZEM = 1;


PZEM004Tv30 pzem(
    Serial2,
    PZEM_RX,
    PZEM_TX
);

// ==================================================
// SERVIDOR WEB
// ==================================================

WebServer server(80);

// ==========================================
// WATT VISION - CONFIGURAÇÃO MQTT
// ==========================================

// Endereço e porta do broker HiveMQ.
const char* mqttServidor = "broker.hivemq.com";
const int mqttPorta = 1883;

// Tópico utilizado para publicar as medições do Watt Vision.
const char* mqttTopicoMedicoes = "wattvision/medicoes";

// Cliente de rede utilizado pelo MQTT.
WiFiClient wifiClient;

// Cliente MQTT.
PubSubClient mqttClient(wifiClient);

// Controle de tempo para reconexão.
unsigned long ultimaTentativaMQTT = 0;

const unsigned long intervaloReconexaoMQTT = 5000;

// ==========================================================
// WATT VISION - ARMAZENAMENTO LOCAL DAS MEDIÇÕES
// ==========================================================


// Arquivo utilizado como fila de medições pendentes.
const char* arquivoMedicoes = "/medicoes_pendentes.txt";


// ==========================================================
// WATT VISION - CONFIGURAÇÃO DE DATA E HORA
// ==========================================================

// Servidor NTP utilizado para sincronizar o relógio.
const char* servidorNTP = "pool.ntp.org";

// Brasil / horário de Brasília (UTC-3).
// Neste primeiro teste estamos utilizando offset fixo.
const long fusoHorario = -3 * 3600;

// Sem horário de verão.
const int horarioVerao = 0;

// ==========================================================
// WATT VISION - INTERVALO DAS MEDIÇÕES
// ==========================================================

// 30 segundos somente para testes.
// Depois alteraremos para 30 minutos.
const unsigned long intervaloMedicao = 30000;

unsigned long ultimaMedicao = 0;

// Arquivo que armazenará medições que não puderam ser enviadas.
const char* arquivoPendentes = "/medicoes_pendentes.txt";


// Número máximo de medições que podem permanecer
// armazenadas na fila do LittleFS.
const int limiteMedicoesPendentes = 100;


// ==================================================
// CONTROLE DE TEMPO
// ==================================================

unsigned long ultimoDiagnostico = 0;

const unsigned long intervaloDiagnostico = 2000;

// ==================================================
// VARIÁVEIS DE MEDIÇÃO
// ==================================================

// ==========================================================
// PROTÓTIPOS DAS FUNÇÕES
// ==========================================================

void reenviarMedicoesPendentes();

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

        Serial.print("1 - PZEM TX -> GPIO ");
        Serial.print(PZEM_RX);
        Serial.println(" (RX ESP32)");

        Serial.print("2 - PZEM RX -> GPIO ");
        Serial.print(PZEM_TX);
        Serial.println(" (TX ESP32)");

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

// ==========================================
// WATT VISION - CONEXÃO MQTT
// ==========================================

void conectarMQTT()
{
    // Não tenta conectar ao broker sem Wi-Fi.
    if (WiFi.status() != WL_CONNECTED)
    {
        return;
    }

    // Evita iniciar uma nova conexão
    // quando já estamos conectados.
    if (mqttClient.connected())
    {
        return;
    }

    // Identificador exclusivo para evitar
    // conflitos com outros dispositivos.
    String clientId = "wattvision-esp32-";
    clientId += WiFi.macAddress();

    // Remove os separadores do endereço MAC.
    clientId.replace(":", "");

    Serial.println();
    Serial.println("[MQTT] Conectando ao HiveMQ...");

    // Tenta estabelecer a conexão.
    if (mqttClient.connect(clientId.c_str()))
    {
        Serial.println(
            "[MQTT] Conectado com sucesso!"
        );

        // Após recuperar a conexão MQTT,
        // tenta enviar as medições armazenadas
        // enquanto o sistema estava offline.
        reenviarMedicoesPendentes();
    }
    else
    {
        Serial.print("[MQTT] Falha. Codigo: ");
        Serial.println(mqttClient.state());
    }
}
// ==========================================================
// SINCRONIZAR DATA E HORA PELA INTERNET
// ==========================================================

void sincronizarRelogio()
{
    // Só é possível sincronizar via NTP se houver Wi-Fi.
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[RELOGIO] Wi-Fi indisponivel.");
        return;
    }

    Serial.println();
    Serial.println("[RELOGIO] Sincronizando data e hora...");

    configTime(
        fusoHorario,
        horarioVerao,
        servidorNTP
    );

    struct tm dataHora;

    // Aguarda até 10 segundos pela sincronização.
    if (!getLocalTime(&dataHora, 10000))
    {
        Serial.println("[RELOGIO] Falha na sincronizacao.");
        return;
    }

    Serial.println("[RELOGIO] Sincronizado com sucesso!");

    char horario[25];

    strftime(
        horario,
        sizeof(horario),
        "%Y-%m-%d %H:%M:%S",
        &dataHora
    );

    Serial.print("[RELOGIO] Data/hora: ");
    Serial.println(horario);
}

// ==========================================================
// OBTER DATA E HORA ATUAL
// ==========================================================

String obterDataHora()
{
    struct tm dataHora;

    // Tenta obter a data/hora atual do relógio do ESP32.
    if (!getLocalTime(&dataHora))
    {
        return "";
    }

    char horario[25];

    strftime(
        horario,
        sizeof(horario),
        "%Y-%m-%d %H:%M:%S",
        &dataHora
    );

    return String(horario);
}

// ==========================================================
// CONTROLAR LIMITE DA FILA DE MEDIÇÕES PENDENTES
// ==========================================================

void controlarLimiteFila()
{
    // Se o arquivo ainda não existe, não há nada para controlar.
    if (!LittleFS.exists(arquivoPendentes))
    {
        return;
    }

    File arquivo = LittleFS.open(
        arquivoPendentes,
        FILE_READ
    );

    if (!arquivo)
    {
        Serial.println(
            "[FILA] ERRO ao abrir arquivo para verificar limite."
        );
        return;
    }

    // ------------------------------------------------------
    // CONTA QUANTAS MEDIÇÕES EXISTEM
    // Cada linha do arquivo representa uma medição JSON.
    // ------------------------------------------------------

    int quantidade = 0;

    while (arquivo.available())
    {
        String linha = arquivo.readStringUntil('\n');
        linha.trim();

        if (linha.length() > 0)
        {
            quantidade++;
        }
    }

    arquivo.close();

    // Ainda existe espaço para a próxima medição.
    if (quantidade < limiteMedicoesPendentes)
    {
        return;
    }

    Serial.println(
        "[FILA] Limite de medicoes pendentes atingido."
    );

    Serial.println(
        "[FILA] Removendo registro mais antigo."
    );

    // ------------------------------------------------------
    // CRIA ARQUIVO TEMPORÁRIO
    // ------------------------------------------------------

    arquivo = LittleFS.open(
        arquivoPendentes,
        FILE_READ
    );

    File temporario = LittleFS.open(
        "/medicoes_temp.txt",
        FILE_WRITE
    );

    if (!arquivo || !temporario)
    {
        Serial.println(
            "[FILA] ERRO ao reorganizar fila."
        );

        if (arquivo)
        {
            arquivo.close();
        }

        if (temporario)
        {
            temporario.close();
        }

        return;
    }

    bool primeiraMedicao = true;

    while (arquivo.available())
    {
        String linha = arquivo.readStringUntil('\n');
        linha.trim();

        if (linha.length() == 0)
        {
            continue;
        }

        // Ignora somente o registro mais antigo.
        if (primeiraMedicao)
        {
            primeiraMedicao = false;
            continue;
        }

        temporario.println(linha);
    }

    arquivo.close();
    temporario.close();

    // ------------------------------------------------------
    // SUBSTITUI A FILA ANTIGA PELA NOVA
    // ------------------------------------------------------

    LittleFS.remove(arquivoPendentes);

        if (!LittleFS.rename(
            "/medicoes_temp.txt",
            arquivoPendentes
        ))
        {
            Serial.println(
                "[FILA] ERRO ao atualizar arquivo de pendencias."
            );

            return;
        }

        Serial.println(
            "[FILA] Registro mais antigo removido."
        );
    }

    // ==========================================================
// REENVIAR MEDIÇÕES PENDENTES
// ==========================================================

void reenviarMedicoesPendentes()
{
    // Só tenta reenviar se o MQTT estiver conectado.
    if (!mqttClient.connected())
    {
        return;
    }

    // Se não existe arquivo, não existem pendências.
    if (!LittleFS.exists(arquivoPendentes))
    {
        return;
    }

    File arquivo = LittleFS.open(
        arquivoPendentes,
        FILE_READ
    );

    if (!arquivo)
    {
        Serial.println(
            "[FILA] ERRO ao abrir medicoes pendentes."
        );
        return;
    }

    // Arquivo temporário utilizado para preservar
    // medições que não conseguirmos reenviar.
    File temporario = LittleFS.open(
        "/medicoes_reenvio.tmp",
        FILE_WRITE
    );

    if (!temporario)
    {
        Serial.println(
            "[FILA] ERRO ao criar arquivo temporario."
        );

        arquivo.close();
        return;
    }

    Serial.println();
    Serial.println(
        "[FILA] Verificando medicoes pendentes..."
    );

    int enviadas = 0;
    int mantidas = 0;

    // Indica que ocorreu uma falha durante o reenvio.
    bool falhaEnvio = false;

    while (arquivo.available())
    {
        String registro =
            arquivo.readStringUntil('\n');

        registro.trim();

        if (registro.length() == 0)
        {
            continue;
        }

        // --------------------------------------------------
        // SE JÁ OCORREU UMA FALHA
        // --------------------------------------------------
        // Não tentamos publicar as próximas medições.
        // Apenas preservamos tudo no arquivo temporário.
        // Isso mantém a ordem FIFO da fila.

        if (falhaEnvio)
        {
            temporario.println(registro);
            mantidas++;

            continue;
        }

        // --------------------------------------------------
        // VERIFICA CONEXÃO MQTT
        // --------------------------------------------------

        if (!mqttClient.connected())
        {
            Serial.println(
                "[FILA] MQTT desconectou durante o reenvio."
            );

            temporario.println(registro);
            mantidas++;

            falhaEnvio = true;

            continue;
        }

        Serial.print(
            "[FILA] Reenviando: "
        );

        Serial.println(registro);

        // --------------------------------------------------
        // PUBLICA A MEDIÇÃO
        // --------------------------------------------------

        bool publicado = mqttClient.publish(
            mqttTopicoMedicoes,
            registro.c_str()
        );

        if (publicado)
        {
            enviadas++;

            Serial.println(
                "[FILA] Medicao reenviada com sucesso."
            );
        }
        else
        {
            // Se falhar, mantém esta medição.
            temporario.println(registro);

            mantidas++;

            falhaEnvio = true;

            Serial.println(
                "[FILA] Falha no reenvio."
            );
        }

        // Mantém a comunicação MQTT ativa.
        mqttClient.loop();

        // Pequeno intervalo para não disparar
        // todas as mensagens instantaneamente.
        delay(50);
    }

    arquivo.close();
    temporario.close();

    // ------------------------------------------------------
    // ATUALIZA O ARQUIVO DE PENDÊNCIAS
    // ------------------------------------------------------

    LittleFS.remove(arquivoPendentes);

    if (mantidas > 0)
    {
        if (!LittleFS.rename(
            "/medicoes_reenvio.tmp",
            arquivoPendentes
        ))
        {
            Serial.println(
                "[FILA] ERRO ao atualizar pendencias."
            );

            return;
        }
    }
    else
    {
        // Tudo foi enviado.
        LittleFS.remove(
            "/medicoes_reenvio.tmp"
        );
    }

    Serial.println();
    Serial.print(
        "[FILA] Medicoes reenviadas: "
    );

    Serial.println(enviadas);

    Serial.print(
        "[FILA] Medicoes ainda pendentes: "
    );

    Serial.println(mantidas);

    if (mantidas == 0)
    {
        Serial.println(
            "[FILA] Fila de pendencias vazia."
        );
    }
}

// ==========================================================
// SALVAR MEDIÇÃO PENDENTE NO LITTLEFS
// ==========================================================

void salvarMedicaoPendente(String registro)
{

        // Antes de adicionar uma nova medição,
        // verifica se a fila atingiu o limite.
        controlarLimiteFila();

        File arquivo = LittleFS.open(
            arquivoPendentes,
            FILE_APPEND
        );

        if (!arquivo)
        {
            Serial.println(
                "[FILA] ERRO ao abrir arquivo de pendencias."
            );
            return;
        }

        arquivo.println(registro);
        arquivo.close();

        Serial.println(
            "[FILA] Medicao armazenada no LittleFS."
        );
    }

    // ==========================================================
    // REALIZAR MEDIÇÃO PROGRAMADA
    // ==========================================================

    void realizarMedicaoProgramada()
    {
        Serial.println();
        Serial.println("========================================");
        Serial.println("        MEDICAO PROGRAMADA");
        Serial.println("========================================");

        // Atualiza os valores vindos do PZEM.
        lerPZEM();

        // Verifica se a leitura é válida.
        if (
            isnan(tensao) ||
            isnan(corrente) ||
            isnan(potencia) ||
            isnan(energia) ||
            isnan(frequencia) ||
            isnan(fatorPotencia)
        )
        {
            Serial.println("[MEDICAO] PZEM sem resposta.");
            return;
        }

        String dataHora = obterDataHora();

        if (dataHora == "")
        {
            Serial.println(
                "[MEDICAO] Data/hora indisponivel."
            );
            return;
        }

        // JSON já utilizando os mesmos nomes do backend.
        String registro = "{";

        // Identificação do PZEM responsável pela medição.
        registro += "\"id_pzem\":";
        registro += String(ID_PZEM);
        registro += ",";
        
        // Data e hora original da medição.
        registro += "\"data_hora\":\"";
        registro += dataHora;
        registro += "\",";

        registro += "\"tensao\":";
        registro += String(tensao, 2);
        registro += ",";

        registro += "\"corrente\":";
        registro += String(corrente, 3);
        registro += ",";

        registro += "\"potencia_ativa\":";
        registro += String(potencia, 2);
        registro += ",";

        registro += "\"energia_acumulada\":";
        registro += String(energia, 3);
        registro += ",";

        registro += "\"frequencia\":";
        registro += String(frequencia, 2);
        registro += ",";

        registro += "\"fator_potencia\":";
        registro += String(fatorPotencia, 3);

        registro += "}";

        Serial.print("[MEDICAO] Data/hora: ");
        Serial.println(dataHora);

        Serial.print("[MEDICAO] JSON: ");
        Serial.println(registro);

        // ======================================================
        // DECISÃO ONLINE / OFFLINE
        // ======================================================

    if (
                WiFi.status() == WL_CONNECTED &&
                mqttClient.connected()
            )
            {
                Serial.println("[MEDICAO] MQTT disponivel.");
                Serial.println("[MQTT] Publicando medicao...");

                // Envia o JSON para o broker MQTT.
                bool publicado = mqttClient.publish(
                    mqttTopicoMedicoes,
                    registro.c_str()
                );

                if (publicado)
                {
                    Serial.println(
                        "[MQTT] Medicao publicada com sucesso!"
                    );
                }
                else
                {
                    Serial.println(
                        "[MQTT] Falha ao publicar."
                    );

                    Serial.println(
                        "[FILA] Salvando medicao como pendente..."
                    );

                    salvarMedicaoPendente(registro);
                }
            }
            else
            {
                Serial.println(
                    "[MEDICAO] MQTT indisponivel."
                );

                Serial.println(
                    "[FILA] Salvando medicao como pendente..."
                );

                salvarMedicaoPendente(registro);
            }

        Serial.println("========================================");
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


    // Sincroniza o relógio do ESP32 pela internet.
    sincronizarRelogio();

    // ==========================================
    // INICIALIZAÇÃO MQTT
    // ==========================================

    // Configura o endereço do broker.
    mqttClient.setServer(
        mqttServidor,
        mqttPorta
    );

    // Realiza a primeira tentativa de conexão.
    conectarMQTT();

    // Registra o horário da tentativa.
    ultimaTentativaMQTT = millis();

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

    // ==========================================
    // MANUTENÇÃO DA CONEXÃO MQTT
    // ==========================================

    if (WiFi.status() == WL_CONNECTED)
    {
        // Verifica se precisamos reconectar.
        if (!mqttClient.connected())
        {
            unsigned long agoraMQTT = millis();

            if (
                agoraMQTT - ultimaTentativaMQTT
                >= intervaloReconexaoMQTT
            )
            {
                ultimaTentativaMQTT = agoraMQTT;

                conectarMQTT();
            }
        }
        else
        {
            // Mantém a comunicação MQTT ativa.
            mqttClient.loop();
        }
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


    // ==========================================================
    // MEDIÇÃO PROGRAMADA
    // ==========================================================

    unsigned long agoraMedicao = millis();

    if (
        agoraMedicao - ultimaMedicao
        >= intervaloMedicao
    )
    {
        ultimaMedicao = agoraMedicao;

        realizarMedicaoProgramada();
    }
}