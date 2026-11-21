/*
 * ====================================================================
 *  VISÃO GERAL: COMO ESTE PROGRAMA FUNCIONA (EXPLICAÇÃO)
 * ====================================================================
 *
 *  Imagine uma "babá eletrônica inteligente". Ela fica ouvindo o ambiente
 *  e, se escutar alguém gritando "SOCORRO" ou "AJUDA", manda uma mensagem
 *  de alerta para o Telegram das pessoas cadastradas.
 *
 *  O ESP32-S3 (o "cérebro" da placa) tem DOIS processadores (núcleos),
 *  e cada um cuida de uma tarefa, como dois funcionários trabalhando
 *  ao mesmo tempo:
 *
 *    NÚCLEO 1  =  "O OUVINTE"
 *                 Escuta o microfone o tempo todo, e usa uma Inteligência
 *                 Artificial (treinada no Edge Impulse) para reconhecer
 *                 as palavras "socorro" e "ajuda".
 *
 *    NÚCLEO 0  =  "O CARTEIRO"
 *                 Cuida da internet: envia os alertas ao Telegram e
 *                 recebe comandos (adicionar ou remover pessoas).
 *
 *    NÚCLEO 1  =  "O SENTINELA"  (novo na v4)
 *                 Vigia o sensor de fumaça e o sensor de temperatura e
 *                 umidade (DHT11) do projeto antigo. Se detectar fumaça,
 *                 também coloca um aviso na caixa de correio.
 *
 *  Como os dois se comunicam? Por uma "CAIXA DE CORREIO" (a fila).
 *  O Ouvinte coloca o aviso na caixa, o Carteiro pega e entrega.
 *  Assim, o Ouvinte nunca precisa parar de ouvir para esperar a internet.
 *
 *  FLUXO RESUMIDO:
 *    Microfone -> IA reconhece a palavra -> aviso vai para a fila
 *              -> Carteiro pega o aviso -> envia pelo Telegram
 *
 *  O QUE HÁ DE NOVO NESTA VERSÃO (o Carteiro "insistente"):
 *    Antes, se a internet estivesse ruim e o envio falhasse 3 vezes, o
 *    alerta era JOGADO FORA. Agora o Carteiro SEGURA o alerta na mão e
 *    fica tentando de novo, com pausas cada vez maiores, até:
 *      a) conseguir entregar para TODAS as pessoas cadastradas; ou
 *      b) passarem 5 minutos (aí o alerta já está velho demais e é
 *         descartado).
 *    Se houver várias pessoas e o envio falhar só para algumas, as
 *    tentativas seguintes são só para QUEM AINDA NÃO RECEBEU (ninguém
 *    recebe o mesmo alerta duas vezes).
 *
 *  >>> O QUE MUDOU NESTA VERSÃO (v4) <<<
 *    1) NOVO "FUNCIONÁRIO": O SENTINELA (TaskSentinela). Traz do projeto
 *       antigo os dois sensores, com os MESMOS pinos:
 *          DHT11 (temperatura/umidade) = pino 7
 *          Sensor de fumaça            = pino 15 (alarme = pino em nível BAIXO)
 *       - Fumaça: lida a cada 0,2 s. Quando o estado MUDA (normal -> alarme
 *         ou alarme -> normal) e a mudança se confirma em 2 leituras seguidas,
 *         um aviso vai para a caixa de correio.
 *       - Temperatura/umidade: lidas a cada 4 s. Se uma leitura falhar, vale
 *         a última boa. Os avisos de fumaça trazem esses valores.
 *    2) Os avisos de fumaça usam a MESMA caixa de correio e o MESMO Carteiro
 *       dos alertas de voz, então também ganham o reenvio insistente (até
 *       5 minutos). No projeto antigo, se o envio falhasse, o aviso se perdia.
 *    3) A caixa de correio agora comporta 10 avisos (antes eram 5).
 *    4) A tela OLED do projeto antigo NÃO foi incluída nesta versão.
 *
 *  >>> MUDANÇAS DA v3 (mantidas) <<<
 *    1) A configuração do Wi-Fi e da conexão segura voltou a ser IGUAL à do
 *       projeto antigo (DHT + OLED), que enviava ao Telegram sem problemas:
 *       só WiFi.begin(), sem os ajustes extras que eu tinha acrescentado
 *       (modo, economia de energia, reconexão, limite de 30 s na conexão
 *       segura). O limite de 30 s podia cortar conexões lentas que ainda
 *       dariam certo.
 *    2) Existe uma chave de teste (HABILITAR_IA, logo abaixo das configurações):
 *       1 = funcionamento normal (microfone + IA ligados);
 *       0 = só o Telegram, com uma mensagem de teste a cada 30 segundos.
 *    3) O Monitor Serial mostra a MEMÓRIA livre a cada envio.
 *
 *  ATENÇÃO: este arquivo contém senha do Wi-Fi e token do bot em texto.
 *  Não compartilhe este arquivo (GitHub, fórum, grupo) sem trocar esses
 *  dados por outros.
 * ====================================================================
 */

// ============================================================
// BIBLIOTECAS (são "caixas de ferramentas" prontas que usamos)
// ============================================================
#include <WiFi.h>                // Conexão com o Wi-Fi
#include <WiFiClientSecure.h>    // Conexão segura (https) com a internet
#include <UniversalTelegramBot.h>// Facilita falar com o Telegram
#include <ArduinoJson.h>         // Entende o formato de dados que o Telegram usa
#include <Preferences.h>         // Guarda dados na memória que NÃO apaga ao desligar
#include <time.h>                // Funções de data e hora
#include <DHT.h>                 // Sensor de temperatura e umidade (DHT11)

#include <Socorro_inferencing.h> // O modelo de IA treinado no Edge Impulse
#include "freertos/FreeRTOS.h"   // Sistema que permite rodar várias tarefas ao mesmo tempo
#include "freertos/task.h"       // Criação das tarefas (os "funcionários")
#include "freertos/queue.h"      // Criação da fila (a "caixa de correio")
#include "driver/i2s.h"          // Comunicação com o microfone digital (I2S)

// ============================================================
// CREDENCIAIS E CONFIGURAÇÕES
// (são os "dados de acesso": rede Wi-Fi e a identidade do bot)
// ============================================================
const char* ssid = "S23_Moises";          // Nome da rede Wi-Fi
const char* password = "maria2108";       // Senha da rede Wi-Fi
#define BOT_TOKEN "9055987351:AAGsqW8hpkg1GE_XustRxhHSxCJGrWzhLAM" // "Senha" do bot do Telegram
#define MASTER_CHAT_ID "8665578181"       // Conversa do "administrador" (quem manda nos comandos)

// Servidor que informa a hora certa pela internet
const char* ntpServer = "pool.ntp.org";
// Fuso horário do Brasil: 3 horas a menos que o horário de Greenwich
const long  gmtOffset_sec = -3 * 3600;
// Horário de verão (0 = não usamos)
const int   daylightOffset_sec = 0;

// Os fios (pinos) onde o microfone está ligado na placa
#define I2S_SCK  4   // Relógio (marca o ritmo dos dados)
#define I2S_WS   5   // Seleção de canal (esquerdo/direito)
#define I2S_SD   6   // Dados (o som propriamente dito)

// De quanto em quanto tempo o Carteiro olha se chegaram comandos novos
// no Telegram (5000 milissegundos = 5 segundos)
#define INTERVALO_POLLING_MS 5000
// Tamanho máximo de cada mensagem de alerta (em caracteres)
#define TAM_MSG 256

// ----- Sensores (os MESMOS pinos do projeto antigo) -----
#define DHTPIN 7          // Sensor de temperatura e umidade (DHT11)
#define FUMACA_PIN 15     // Sensor de fumaça (digital): nível BAIXO (LOW) = fumaça detectada
// De quanto em quanto tempo o Sentinela lê cada sensor:
#define INTERVALO_FUMACA_MS 200UL   // Fumaça: a cada 0,2 s
#define INTERVALO_DHT_MS    4000UL  // Temperatura/umidade: a cada 4 s (igual ao projeto antigo)
// Quantas leituras SEGUIDAS iguais são necessárias para confirmar uma mudança
// no sensor de fumaça (evita falso alarme por um "pulso" isolado)
#define LEITURAS_CONFIRMACAO 2

DHT dht(DHTPIN, DHT11);     // O objeto que conversa com o sensor DHT11

// ----- CHAVE DO MODO DE TESTE -----
// 1 = funcionamento normal (microfone + IA ligados)
// 0 = TESTE: IA e microfone DESLIGADOS; a placa manda uma mensagem de
//     teste ao Telegram a cada 30 segundos (INTERVALO_TESTE_MS)
#define HABILITAR_IA 1
#define INTERVALO_TESTE_MS 30000UL

// ----- Regras do Carteiro "insistente" -----
// Por quanto tempo o Carteiro continua tentando entregar um alerta
// (300000 ms = 5 minutos). Depois disso o alerta é considerado velho.
#define VALIDADE_ALERTA_MS 300000UL
// Pausa entre as rodadas de tentativa: começa em 5 s e aumenta 5 s a cada
// rodada, até o máximo de 30 s (5 s, 10 s, 15 s, ... 30 s, 30 s, ...)
#define ESPERA_BASE_MS 5000UL
#define ESPERA_MAX_MS  30000UL

// ============================================================
// OBJETOS GLOBAIS (coisas que várias partes do código compartilham)
// ============================================================
WiFiClientSecure client;                    // A "linha telefônica" segura para a internet
UniversalTelegramBot bot(BOT_TOKEN, client);// O bot, que usa essa linha para falar com o Telegram
Preferences preferences;                    // A "gaveta" de memória permanente
QueueHandle_t filaMensagens;                // A "caixa de correio" entre Ouvinte e Carteiro

int qtdIDs = 0;                             // Quantas pessoas estão cadastradas para receber alertas
String listaChatIds = MASTER_CHAT_ID;       // Lista das pessoas cadastradas (separadas por vírgula)
unsigned long tempoUltimoAlertaIA = 0;      // Quando foi o último alerta (evita enviar vários seguidos)
static bool horaConfigurada = false;        // Já pedimos a hora certa para a internet?

// ============================================================
// VARIÁVEIS DO EDGE IMPULSE (a parte da Inteligência Artificial)
// ============================================================

// Para ouvir sem perder nada, usamos DOIS "baldes" de áudio (buffers):
// enquanto um está sendo enchido pelo microfone, a IA analisa o outro.
typedef struct {
    signed short *buffers[2];   // Os dois baldes de áudio
    unsigned char buf_select;   // Qual balde está sendo enchido agora (0 ou 1)
    unsigned char buf_ready;    // 1 = um balde ficou cheio e está pronto para análise
    unsigned int buf_count;     // Quantas amostras de som já entraram no balde atual
    unsigned int n_samples;     // Tamanho do balde (quantas amostras cabem)
} inference_t;

static inference_t inference;                       // Nossa estrutura de baldes
static const uint32_t sample_buffer_size = 2048;    // Quantidade de amostras lidas por vez
static int32_t sampleBuffer32[sample_buffer_size];  // Áudio "bruto" do microfone (32 bits)
static signed short sampleBuffer16[sample_buffer_size]; // Áudio convertido para 16 bits (o que a IA entende)
static bool debug_nn = false;                       // Mensagens técnicas extras da IA (desligado)
// Contador para só avaliar o resultado após juntar várias fatias de áudio
static int print_results = -(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW);
static bool record_status = true;                   // true = continua gravando
static bool debugMode = true;                       // true = escreve mensagens no Monitor Serial (para testes)

// "Aviso" ao compilador: a função i2s_init existe e é definida mais abaixo.
// Sem isso ele reclamaria, pois a usamos antes de ela aparecer no arquivo.
static int i2s_init(uint32_t sampling_rate);

// ============================================================
// WI-FI
// ============================================================

// Esta função é um "fofoqueiro": sempre que o Wi-Fi conecta ou desconecta,
// ela escreve no Monitor Serial o que aconteceu e o MOTIVO.
// Códigos úteis: 201 = rede não encontrada | 202, 15 ou 204 = senha errada
void onWiFiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    Serial.printf("WiFi desconectado. Motivo (codigo): %d\n", info.wifi_sta_disconnected.reason);
  } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    Serial.print("WiFi obteve IP: ");
    Serial.println(WiFi.localIP());
  }
}

// Pede a hora certa para a internet (só uma vez).
// Precisamos disso para carimbar data e hora nos alertas.
void sincronizarHora() {
  if (horaConfigurada) return;   // Se já fez, não repete
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  horaConfigurada = true;
}

// Verifica se o Wi-Fi está funcionando.
// Se caiu, apenas ESPERA (até 10 s) o Wi-Fi voltar sozinho.
// Importante: NÃO mandamos reconectar manualmente, pois isso
// atrapalhava a reconexão automática e gerava erros.
bool garantirWiFi() {
  if (WiFi.status() == WL_CONNECTED) {   // Tudo certo, está conectado
    sincronizarHora();
    return true;
  }
  if (debugMode) Serial.println("Core 0: Aguardando WiFi reconectar...");
  // Espera até 20 vezes meio segundo (= 10 segundos)
  for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  if (WiFi.status() == WL_CONNECTED) {
    sincronizarHora();
    return true;
  }
  return false;   // Não voltou: desistimos por enquanto
}

// ============================================================
// FUNÇÕES AUXILIARES (pequenas ajudantes usadas em vários lugares)
// ============================================================

// Devolve a data e hora atuais em texto, ex: "13/11/2026 14:30:05".
// Se a hora ainda não foi obtida da internet, devolve um aviso.
String obterDataHora() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 50)) return "Horario indisponivel";
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
  return String(buffer);
}

// Conta quantas pessoas estão na lista de cadastrados.
// A lista é um texto tipo "111,222,333". Contamos as vírgulas e somamos 1.
int contarIDs() {
  if (listaChatIds.length() == 0) return 0;
  int count = 1;
  for (int i = 0; i < (int)listaChatIds.length(); i++) {
    if (listaChatIds.charAt(i) == ',') count++;
  }
  return count;
}

// Guarda a lista de cadastrados na memória permanente.
// Assim, mesmo desligando a placa, a lista não se perde.
void salvarLista() {
  preferences.begin("telegram", false);        // Abre a gaveta (false = pode escrever)
  preferences.putString("ids", listaChatIds);  // Guarda a lista
  preferences.end();                           // Fecha a gaveta
}

// Envia uma mensagem para UMA pessoa. Tenta até 2 vezes seguidas.
// (Se mesmo assim falhar, quem chamou esta função decide tentar mais tarde.)
// Escreve no Monitor Serial se deu certo ou errado.
bool enviarUm(const String &id, const String &mensagem) {
  // Mostra a situação da memória ANTES de enviar (para investigar falhas):
  //  livre = memória total livre | maior bloco = o maior pedaço contínuo livre
  //  (a conexão segura precisa de um bom pedaço contínuo) | mínimo = o pior
  //  momento desde que a placa ligou
  if (debugMode) {
    Serial.printf("Core 0: Memoria -> livre: %d | maior bloco: %d | minimo ja visto: %d\n",
                  ESP.getFreeHeap(), ESP.getMaxAllocHeap(), ESP.getMinFreeHeap());
  }
  for (int tentativa = 1; tentativa <= 2; tentativa++) {
    // "Markdown" permite usar *negrito* na mensagem
    if (bot.sendMessage(id, mensagem, "Markdown")) {
      if (debugMode) Serial.printf("Core 0: Envio OK para %s (tentativa %d)\n", id.c_str(), tentativa);
      return true;   // Deu certo, terminou
    }
    if (debugMode) Serial.printf("Core 0: Envio FALHOU para %s (tentativa %d)\n", id.c_str(), tentativa);
    client.stop();   // Falhou de verdade: "desliga o telefone" para recomeçar a ligação limpa
    vTaskDelay(500 / portTICK_PERIOD_MS);   // Espera meio segundo e tenta de novo
  }
  return false;   // Falhou as 2 vezes
}

// ===== NOVA FUNÇÃO (substitui a antiga "enviarParaTodos") =====
// Tenta entregar o alerta para as pessoas que AINDA NÃO RECEBERAM.
//
//   mensagem = o texto do alerta
//   faltando = a lista (separada por vírgulas) de quem ainda não recebeu.
//              Ao final, esta lista é ATUALIZADA: quem recebeu sai dela,
//              quem falhou continua nela para a próxima rodada.
//
// Devolve true se TODOS já receberam (lista vazia), false se ainda falta alguém.
bool enviarParaFaltantes(const String &mensagem, String &faltando) {
  if (!garantirWiFi()) {   // Sem internet, nem adianta tentar
    if (debugMode) Serial.println("Core 0: Sem WiFi nesta rodada.");
    return false;
  }

  String restante = "";   // Vai juntar quem continuou sem receber
  int inicio = 0;         // Onde começa o número atual na lista

  while (true) {
    int fim = faltando.indexOf(',', inicio);   // Procura a próxima vírgula
    // Recorta o número atual (se não há mais vírgula, é o último da lista)
    String id = (fim == -1) ? faltando.substring(inicio) : faltando.substring(inicio, fim);
    id.trim();   // Remove espaços sobrando

    if (id != "") {
      if (!enviarUm(id, mensagem)) {
        // Falhou: este número continua na lista de "quem falta"
        if (restante.length() > 0) restante += ",";
        restante += id;
      }
      vTaskDelay(200 / portTICK_PERIOD_MS);   // Pequena pausa entre pessoas
    }

    if (fim == -1) break;   // Era o último número: terminou
    inicio = fim + 1;       // Avança para o próximo número
  }

  faltando = restante;               // Atualiza a lista de quem ainda falta
  return faltando.length() == 0;     // true = todo mundo recebeu
}

// Lê as mensagens recebidas no Telegram e executa os comandos.
// SÓ o administrador (MASTER_CHAT_ID) pode dar comandos:
//    add:123456  ->  cadastra a pessoa 123456 para receber alertas
//    del:123456  ->  remove a pessoa 123456 da lista
void processarMensagens(int numNovasMensagens) {
  static String ultimoTexto = "";          // Lembra a última mensagem (evita processar duplicada)
  static unsigned long ultimoTempo = 0;

  for (int i = 0; i < numNovasMensagens; i++) {
    String chat_id = String(bot.messages[i].chat_id);  // Quem mandou
    String text = bot.messages[i].text;                // O que escreveu
    text.replace(" ", "");                             // Remove espaços, para "add: 123" funcionar

    // Se for igual à mensagem anterior e chegou há menos de 2 s, ignora (duplicata)
    if (text == ultimoTexto && (millis() - ultimoTempo < 2000)) continue;
    ultimoTexto = text;
    ultimoTempo = millis();

    // Só aceita comandos do administrador
    if (chat_id == MASTER_CHAT_ID) {

      // ---------- COMANDO "add:" (adicionar pessoa) ----------
      if (text.startsWith("add:")) {
        String novoId = text.substring(4);   // Pega o que vem depois de "add:"
        // Só adiciona se não estiver vazio e se ainda não estiver na lista
        if (novoId.length() > 0 && listaChatIds.indexOf(novoId) == -1) {
          listaChatIds += "," + novoId;      // Coloca no fim da lista
          salvarLista();                     // Guarda na memória permanente
          qtdIDs = contarIDs();
          bot.sendMessage(chat_id, "✅ ID " + novoId + " adicionado! Total: " + String(qtdIDs), "");
        }
      }

      // ---------- COMANDO "del:" (remover pessoa) ----------
      else if (text.startsWith("del:")) {
        String idRemover = text.substring(4);
        // O administrador nunca pode ser removido
        if (idRemover != MASTER_CHAT_ID) {
          // Monta uma lista nova copiando todo mundo, MENOS quem será removido
          String novaLista = "";
          int inicio = 0;
          int fim = listaChatIds.indexOf(',');
          while (fim != -1) {
            String idAtual = listaChatIds.substring(inicio, fim);
            if (idAtual != idRemover) novaLista += idAtual + ",";
            inicio = fim + 1;
            fim = listaChatIds.indexOf(',', inicio);
          }
          String idFinal = listaChatIds.substring(inicio);
          if (idFinal != idRemover) novaLista += idFinal;
          // Se sobrou uma vírgula sobrando no fim, tira
          if (novaLista.endsWith(",")) novaLista.remove(novaLista.length() - 1);
          listaChatIds = novaLista;
          salvarLista();
          qtdIDs = contarIDs();
          bot.sendMessage(chat_id, "🗑️ ID " + idRemover + " removido! Total: " + String(qtdIDs), "");
        }
      }
    }
  }
}

// ============================================================
// FUNÇÕES DO EDGE IMPULSE E I2S (microfone + inteligência artificial)
// ============================================================

// Pega o áudio convertido e coloca nos "baldes".
// Quando um balde enche, avisa que está pronto e passa a encher o outro.
static void audio_inference_callback(uint32_t n_bytes) {
    for (int i = 0; i < n_bytes >> 1; i++) {   // ">> 1" = divide por 2 (cada amostra tem 2 bytes)
        // Coloca uma amostra de som no balde atual
        inference.buffers[inference.buf_select][inference.buf_count++] = sampleBuffer16[i];
        // Se o balde encheu...
        if (inference.buf_count >= inference.n_samples) {
            inference.buf_select ^= 1;   // Troca de balde (0 vira 1, 1 vira 0)
            inference.buf_count = 0;     // Balde novo começa vazio
            inference.buf_ready = 1;     // Avisa: "tem áudio pronto para a IA analisar!"
        }
    }
}

// Tarefa que roda em segundo plano lendo o microfone sem parar.
static void capture_samples(void* arg) {
  const int32_t i2s_bytes_to_read = (uint32_t)arg;
  size_t bytes_read = i2s_bytes_to_read;

  while (record_status) {
    // Lê um pedaço de áudio do microfone (espera no máximo 100 ms)
    i2s_read((i2s_port_t)1, (void*)sampleBuffer32, i2s_bytes_to_read, &bytes_read, 100);

    if (bytes_read > 0) {
        int num_samples = bytes_read / 4;   // Cada amostra tem 4 bytes (32 bits)
        for (int x = 0; x < num_samples; x++) {
            // O microfone entrega números grandes (32 bits). Dividimos por 2^14
            // (">> 14") para caberem em 16 bits, que é o formato da IA.
            int32_t val = sampleBuffer32[x] >> 14;
            // Trava os limites para não "estourar" o número (som distorcido)
            if (val > 32767) val = 32767;
            if (val < -32768) val = -32768;
            sampleBuffer16[x] = (int16_t)val;
        }
        // Entrega o áudio convertido para os baldes
        if (record_status) audio_inference_callback(num_samples * 2);
        else break;
    }
    vTaskDelay(1);   // Pequena pausa para os outros processos respirarem
  }
  vTaskDelete(NULL);   // Se a gravação parar, esta tarefa se encerra
}

// Prepara tudo para começar a ouvir: reserva memória para os baldes,
// liga o microfone e inicia a tarefa de leitura.
static bool microphone_inference_start(uint32_t n_samples) {
    // Reserva memória para o balde 0 (se faltar memória, desiste)
    inference.buffers[0] = (signed short *)malloc(n_samples * sizeof(signed short));
    if (inference.buffers[0] == NULL) return false;
    // Reserva memória para o balde 1 (se faltar, libera o primeiro e desiste)
    inference.buffers[1] = (signed short *)malloc(n_samples * sizeof(signed short));
    if (inference.buffers[1] == NULL) { ei_free(inference.buffers[0]); return false; }
    // Zera os contadores
    inference.buf_select = 0; inference.buf_count = 0; inference.n_samples = n_samples; inference.buf_ready = 0;

    // Liga o microfone na velocidade que a IA espera
    if (i2s_init(EI_CLASSIFIER_FREQUENCY)) {
        if (debugMode) Serial.println("Failed to start I2S!");
    }
    ei_sleep(100);
    record_status = true;

    // Cria a tarefa de gravação, fixada no núcleo 1, com prioridade alta (10)
    xTaskCreatePinnedToCore(capture_samples, "CaptureSamples", 1024 * 32, (void*)sample_buffer_size, 10, NULL, 1);
    return true;
}

// Fica esperando até algum balde encher. Quando enche, libera a IA para analisar.
static bool microphone_inference_record(void) {
    while (inference.buf_ready == 0) { delay(1); }   // Espera...
    inference.buf_ready = 0;                         // Marca como "já peguei"
    return true;
}

// A IA chama esta função para "buscar" o áudio do balde que ficou cheio.
// Ela converte os números inteiros do áudio para decimais (float).
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
    // "^ 1" escolhe o balde que NÃO está sendo enchido agora (o que já está completo)
    numpy::int16_to_float(&inference.buffers[inference.buf_select ^ 1][offset], out_ptr, length);
    return 0;
}

// Configura o microfone digital (protocolo I2S): velocidade, formato e pinos.
static int i2s_init(uint32_t sampling_rate) {
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),  // Placa comanda e apenas RECEBE som
      .sample_rate = sampling_rate,                          // Velocidade (amostras por segundo)
      .bits_per_sample = (i2s_bits_per_sample_t)32,          // Cada amostra tem 32 bits
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,           // Usa só o canal esquerdo (mono)
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,     // Padrão de comunicação I2S
      .intr_alloc_flags = 0,
      .dma_buf_count = 8, .dma_buf_len = 512,                // Memória intermediária do microfone
      .use_apll = false, .tx_desc_auto_clear = false, .fixed_mclk = -1,
  };
  // Diz em quais pinos o microfone está ligado
  i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_SCK, .ws_io_num = I2S_WS, .data_out_num = -1, .data_in_num = I2S_SD,
  };
  esp_err_t ret = 0;
  ret = i2s_driver_install((i2s_port_t)1, &i2s_config, 0, NULL);  // Instala o "driver" do microfone
  ret = i2s_set_pin((i2s_port_t)1, &pin_config);                  // Configura os pinos
  ret = i2s_zero_dma_buffer((i2s_port_t)1);                       // Limpa a memória do microfone
  return int(ret);
}

// ============================================================
// TAREFA DO NÚCLEO 1 = "O OUVINTE" (IA e microfone)
// (esta parte NÃO mudou em relação à versão anterior)
// ============================================================
void TaskCore1_IA(void * pvParameters) {
  if (debugMode) Serial.println("Core 1: Iniciando IA e Microfone...");

  run_classifier_init();   // "Acorda" a inteligência artificial
  ei_sleep(1000);

  // Tenta ligar o microfone. Se não conseguir (falta de memória), encerra esta tarefa.
  if (microphone_inference_start(EI_CLASSIFIER_SLICE_SIZE) == false) {
    if (debugMode) Serial.println("ERR: Falha ao alocar buffer de audio.");
    vTaskDelete(NULL);
  }

  if (debugMode) Serial.println("Core 1: Gravando...");
  
  // --- NOVAS VARIÁVEIS PARA EVITAR FALSO POSITIVO ---
  int contador_socorro = 0;
  int contador_ajuda = 0;
  
  // LOOP INFINITO: ouvir -> analisar -> decidir, para sempre
  while (true) {
    // 1) Espera o áudio ficar pronto
    bool m = microphone_inference_record();
    if (!m) continue;

    // 2) Entrega o áudio para a IA analisar
    signal_t signal;
    signal.total_length = EI_CLASSIFIER_SLICE_SIZE;
    signal.get_data = &microphone_audio_signal_get_data;
    ei_impulse_result_t result = {0};   // Aqui a IA coloca a resposta

    EI_IMPULSE_ERROR r = run_classifier_continuous(&signal, &result, debug_nn);
    if (r != EI_IMPULSE_OK) continue;   // Se a IA deu erro, pula e tenta no próximo áudio

    // 3) A cada janela completa de análise, avalia o resultado
    if (++print_results >= (EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW)) {
        print_results = 0;

        // A IA responde com uma "nota" de 0 a 1 para cada palavra
        // (0 = certeza que NÃO ouviu / 1 = certeza que ouviu)
        float prob_socorro = 0;
        float prob_ajuda = 0;
        float prob_noise = 0; // <-- ADICIONADO AQUI
        float prob_unknown = 0;

        // Procura, na resposta da IA, a nota de cada palavra
        for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
            String label = String(result.classification[ix].label);
            label.toLowerCase();   // Converte para minúsculas para comparar sem erro
            if (label == "socorro") prob_socorro = result.classification[ix].value;
            else if (label == "ajuda") prob_ajuda = result.classification[ix].value;
            else if (label == "noise") prob_noise = result.classification[ix].value; // <-- ADICIONADO AQUI
            else if (label == "unknown") prob_unknown = result.classification[ix].value; // <-- ADICIONADO AQUI
        }

        // Mostra as notas no Monitor Serial (para acompanhar os testes)
        if (debugMode) {
            Serial.printf("IA Percebeu -> Socorro: %.2f | Ajuda: %.2f | Noise: %.2f | Unknown: %.2f\n", prob_socorro, prob_ajuda, prob_noise, prob_unknown);
        }

        // =======================================================
        // REGRA DE DECISÃO REFINADA (ANTI-FALSO POSITIVO)
        // =======================================================
        
        // 1. Limiar sobe para 0.80 (80% de certeza)
        // 2. Margem de vitória sobe para 1.5 (50% maior que a outra palavra)
        // 3. Filtro de ruído: "Noise" tem que ser menor que 0.50
        bool vitoria_socorro = (prob_socorro > 0.98) && (prob_socorro > (prob_ajuda * 1.5)) && (prob_noise < 0.50);
        bool vitoria_ajuda   = (prob_ajuda > 0.55) && (prob_ajuda > (prob_socorro * 1.5));// && (prob_noise < 0.50);

        // --- MÉDIA MÓVEL: Exige detecção em 2 janelas consecutivas ---
        if (vitoria_socorro) {
            contador_socorro++;
            contador_ajuda = 0; // Zera o da ajuda
        } else if (vitoria_ajuda) {
            contador_ajuda++;
            contador_socorro = 0; // Zera o do socorro
        } else {
            // Se não houve vitória (foi ruído ou silêncio), zera os dois
            contador_socorro = 0;
            contador_ajuda = 0;
        }

        // Só confirma se o contador chegou a 2 (ou seja, 2 janelas seguidas)
        bool confirmar_socorro = (contador_socorro >= 1);
        bool confirmar_ajuda   = (contador_ajuda >= 1);

        if (confirmar_socorro || confirmar_ajuda) {
            // Trava de segurança de 10 segundos
            if (millis() - tempoUltimoAlertaIA > 10000) {
                String palavra = confirmar_socorro ? "SOCORRO" : "AJUDA";
                float confianca = confirmar_socorro ? prob_socorro : prob_ajuda;

                char mensagem[TAM_MSG];
                snprintf(mensagem, sizeof(mensagem),
                         "🗣️ *ALERTA DE VOZ* 🗣️\nPalavra: *%s*\nConfiança: %.2f\n📅 %s",
                         palavra.c_str(), confianca, obterDataHora().c_str());

                if (xQueueSend(filaMensagens, mensagem, 0) == pdTRUE) {
                    tempoUltimoAlertaIA = millis();
                    if (debugMode) Serial.println("Core 1: ALERTA CONFIRMADO E ENVIADO PARA A FILA.");
                } else {
                    if (debugMode) Serial.println("Core 1: Fila cheia, alerta descartado.");
                }
                
                // RESETA OS CONTADORES APÓS O DISPARO
                contador_socorro = 0;
                contador_ajuda = 0;
            }
        }
    }
  }
}

// ============================================================
// TAREFA DO NÚCLEO 0 = "O CARTEIRO" (Telegram)
// Esta é a parte que MUDOU: agora ele segura o alerta e insiste.
// ============================================================
void TaskCore0_Telegram(void * pvParameters) {
  // Ao iniciar, recupera da memória permanente a lista de pessoas cadastradas
  // (se não houver nada salvo, usa apenas o administrador)
  preferences.begin("telegram", true);   // true = somente leitura
  listaChatIds = preferences.getString("ids", MASTER_CHAT_ID);
  preferences.end();
  qtdIDs = contarIDs();

  // Se a internet estiver ok, avisa o administrador que o sistema ligou
  if (garantirWiFi()) {
    String msgInicio = "✅ Sistema TCC Iniciado (Dual-Core)!\n📅 " + obterDataHora();
    enviarUm(MASTER_CHAT_ID, msgInicio);
  }

  unsigned long tempoUltimoBot = millis();   // Quando foi a última checagem de comandos

  // ----- A "MÃO" DO CARTEIRO: o alerta que ele está tentando entregar agora -----
  char msgPendente[TAM_MSG];            // O texto do alerta que ele está segurando
  bool temPendente = false;             // true = há um alerta na mão, ainda não entregue
  String idsFaltando = "";              // Quem AINDA NÃO recebeu este alerta
  unsigned long inicioPendente = 0;     // Quando ele começou a tentar entregar
  unsigned long proximaTentativa = 0;   // Quando fará a próxima tentativa
  int rodada = 0;                       // Quantas rodadas de tentativa já fez

#if !HABILITAR_IA
  unsigned long tempoUltimoTeste = millis();   // Quando foi a última mensagem de teste
#endif

  // LOOP INFINITO: o Carteiro repete o expediente para sempre
  while (true) {

#if !HABILITAR_IA
    // MODO DE TESTE: sem IA, a própria placa coloca uma mensagem de teste na
    // caixa de correio a cada 30 s (só se o Carteiro estiver livre).
    if (!temPendente && (millis() - tempoUltimoTeste >= INTERVALO_TESTE_MS)) {
      tempoUltimoTeste = millis();
      char teste[TAM_MSG];
      snprintf(teste, sizeof(teste), "🧪 Mensagem de TESTE (IA desligada)\n📅 %s",
               obterDataHora().c_str());
      xQueueSend(filaMensagens, teste, 0);
      if (debugMode) Serial.println("Core 0: [TESTE] Mensagem de teste colocada na fila.");
    }
#endif

    // PASSO 1: a mão está vazia? Então pega o próximo alerta da caixa de correio.
    if (!temPendente) {
      if (xQueueReceive(filaMensagens, msgPendente, 0) == pdTRUE) {
        temPendente = true;
        idsFaltando = listaChatIds;        // No começo, TODOS ainda faltam receber
        inicioPendente = millis();         // Marca a hora em que começou
        proximaTentativa = millis();       // A primeira tentativa é imediata
        rodada = 0;
        if (debugMode) {
          Serial.printf("Core 0: Pegou alerta da fila. Heap: %d\n", ESP.getFreeHeap());
        }
      }
    }

    // PASSO 2: tem alerta na mão e já está na hora de tentar entregar?
    if (temPendente && millis() >= proximaTentativa) {
      rodada++;
      if (debugMode) Serial.printf("Core 0: Rodada %d de entrega...\n", rodada);

      bool todosReceberam = enviarParaFaltantes(String(msgPendente), idsFaltando);

      if (todosReceberam) {
        // Missão cumprida: solta o alerta e fica livre para o próximo
        temPendente = false;
        if (debugMode) Serial.println("Core 0: Alerta entregue a todos!");
      }
      else if (millis() - inicioPendente > VALIDADE_ALERTA_MS) {
        // Já passaram 5 minutos tentando: o alerta está velho, desiste
        temPendente = false;
        if (debugMode) Serial.println("Core 0: Alerta expirou (5 min). Descartado.");
      }
      else {
        // Ainda falta gente e ainda há tempo: espera um pouco e tenta de novo.
        // A espera cresce a cada rodada (5 s, 10 s, 15 s... até no máximo 30 s).
        unsigned long espera = ESPERA_BASE_MS * rodada;
        if (espera > ESPERA_MAX_MS) espera = ESPERA_MAX_MS;
        proximaTentativa = millis() + espera;
        if (debugMode) {
          Serial.printf("Core 0: Ainda faltam: %s | nova tentativa em %lu s\n",
                        idsFaltando.c_str(), espera / 1000);
        }
      }
    }

    // PASSO 3: a cada 5 segundos, vê se alguém mandou comandos (add:/del:).
    // Só faz isso se NÃO houver alerta pendente (o alerta tem prioridade total).
    if (!temPendente && (millis() - tempoUltimoBot >= INTERVALO_POLLING_MS)) {
      tempoUltimoBot = millis();

      if (WiFi.status() == WL_CONNECTED) {
          // Pergunta ao Telegram: "tem mensagens novas?"
          int numNovasMensagens = bot.getUpdates(bot.last_message_received + 1);

          if (numNovasMensagens < 0) {
              client.stop();   // Número negativo = erro de leitura: reinicia a conexão
          } else {
              // Enquanto houver mensagens novas, processa
              while (numNovasMensagens > 0) {
                processarMensagens(numNovasMensagens);
                numNovasMensagens = bot.getUpdates(bot.last_message_received + 1);
                if (numNovasMensagens < 0) {
                    client.stop();
                    break;
                }
              }
          }
      }
    }

    // Descansa 100 ms antes de repetir (para não sobrecarregar o processador)
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// ============================================================
// TAREFA DO NÚCLEO 1 = "O SENTINELA" (fumaça + temperatura/umidade)
// Reproduz a lógica do projeto antigo, mas numa tarefa própria. Assim, um
// problema de internet NUNCA atrasa a vigilância da fumaça: o Sentinela só
// coloca o aviso na caixa de correio, e quem entrega é o Carteiro.
// ============================================================

// Coloca um texto na caixa de correio. Devolve true se coube, false se estava cheia.
// (A caixa copia sempre TAM_MSG bytes, por isso usamos um espaço desse tamanho.)
static bool enfileirarAviso(const char *texto) {
  char buf[TAM_MSG];
  snprintf(buf, sizeof(buf), "%s", texto);
  return xQueueSend(filaMensagens, buf, 0) == pdTRUE;
}

void TaskSentinela(void * pvParameters) {
  pinMode(FUMACA_PIN, INPUT_PULLUP);   // Pino de fumaça: "puxado" para nível alto; o sensor o leva a baixo no alarme
  dht.begin();                         // Liga o sensor de temperatura/umidade
  if (debugMode) Serial.println("Sentinela: vigiando fumaca e DHT11...");

  float temperatura = 0.0;             // Última temperatura boa lida
  float umidade = 0.0;                 // Última umidade boa lida
  bool leituraValida = false;          // Já tivemos pelo menos UMA leitura boa do DHT?

  bool alarmeFumaca = false;           // Estado CONFIRMADO da fumaça (true = alarme)
  bool alarmeAnterior = false;         // Último estado que JÁ foi avisado ao Telegram
  int contagemDiferente = 0;           // Quantas leituras seguidas discordam do estado confirmado
  unsigned long tempoUltimoDHT = 0;    // Quando foi a última leitura do DHT
  unsigned long ultimoLogFila = 0;     // Para não encher o Monitor Serial de avisos repetidos

  while (true) {

    // PASSO 1: a cada 4 s, lê temperatura e umidade.
    // Se a leitura falhar (acontece com o DHT11), mantém a última boa.
    if (millis() - tempoUltimoDHT >= INTERVALO_DHT_MS) {
      tempoUltimoDHT = millis();
      float h = dht.readHumidity();
      float t = dht.readTemperature();
      if (!isnan(h) && !isnan(t)) {
        umidade = h;
        temperatura = t;
        leituraValida = true;
      } else if (debugMode) {
        Serial.println("Sentinela: falha ao ler o DHT11 (mantendo a ultima leitura).");
      }
    }

    // PASSO 2: lê o sensor de fumaça (LOW = fumaça) e CONFIRMA a mudança.
    // O estado só muda depois de LEITURAS_CONFIRMACAO leituras seguidas diferentes.
    bool lido = (digitalRead(FUMACA_PIN) == LOW);
    if (lido != alarmeFumaca) {
      contagemDiferente++;
      if (contagemDiferente >= LEITURAS_CONFIRMACAO) {
        alarmeFumaca = lido;
        contagemDiferente = 0;
      }
    } else {
      contagemDiferente = 0;
    }

    // PASSO 3: o estado confirmado mudou em relação ao último aviso? Então avisa.
    if (alarmeFumaca != alarmeAnterior) {
      String dataHora = obterDataHora();
      char msg[TAM_MSG];

      if (alarmeFumaca) {
        // Mensagem de ALARME (com temperatura e umidade, se já tivermos uma leitura)
        if (leituraValida) {
          snprintf(msg, sizeof(msg),
                   "🚨 ALERTA DE FUMAÇA 🚨\n📅 Data/Hora: %s\n🌡️ Temp: %.1f C\n💧 Umid: %.0f%%\nStatus: ALARME",
                   dataHora.c_str(), temperatura, umidade);
        } else {
          snprintf(msg, sizeof(msg),
                   "🚨 ALERTA DE FUMAÇA 🚨\n📅 Data/Hora: %s\n🌡️ Temp/Umid: indisponivel\nStatus: ALARME",
                   dataHora.c_str());
        }
        if (debugMode) Serial.println("Sentinela: FUMACA detectada! Colocando alerta na fila...");
      } else {
        // Mensagem de NORMALIZAÇÃO
        snprintf(msg, sizeof(msg),
                 "✅ O alarme de fumaça foi normalizado.\n📅 Data/Hora: %s",
                 dataHora.c_str());
        if (debugMode) Serial.println("Sentinela: fumaca normalizada. Colocando aviso na fila...");
      }

      if (enfileirarAviso(msg)) {
        alarmeAnterior = alarmeFumaca;   // Aviso guardado na caixa: não precisa repetir
      } else if (debugMode && (millis() - ultimoLogFila > 5000)) {
        // Caixa cheia: NÃO damos o aviso como feito; tentamos de novo no próximo ciclo
        ultimoLogFila = millis();
        Serial.println("Sentinela: fila cheia, vou tentar de novo.");
      }
    }

    vTaskDelay(INTERVALO_FUMACA_MS / portTICK_PERIOD_MS);   // Descansa 0,2 s
  }
}

// ============================================================
// SETUP PRINCIPAL (roda UMA vez, quando a placa liga)
// É a "preparação do ambiente" antes de o trabalho começar.
// ============================================================
void setup() {
  Serial.begin(115200);   // Liga o Monitor Serial (onde aparecem as mensagens de teste)
  delay(1000);

  // Cria a caixa de correio: cabe até 10 mensagens de 256 caracteres cada
  // (compartilhada por alertas de voz e de fumaça)
  filaMensagens = xQueueCreate(10, TAM_MSG);
  if (filaMensagens == NULL) {
    Serial.println("Erro: Falha ao criar a fila.");
    while (1);   // Sem a fila o sistema não funciona: trava aqui de propósito
  }

  // ----- Conectando ao Wi-Fi -----
  // ATENÇÃO: esta parte ficou IGUAL à do projeto antigo (DHT + OLED), que
  // enviava ao Telegram sem problemas. Os ajustes extras que eu tinha
  // acrescentado antes (modo, economia de energia, reconexão, limite de 30 s
  // na conexão segura) foram REMOVIDOS de propósito.
  Serial.println("Conectando ao WiFi...");
  WiFi.onEvent(onWiFiEvent);     // Só "escuta" e escreve o motivo das quedas; não muda o comportamento
  WiFi.begin(ssid, password);    // Começa a conectar

  // Espera até 20 segundos (40 x 0,5 s) pela conexão
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 40) {
    delay(500); tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi Conectado!");
    sincronizarHora();   // Já pede a hora certa à internet
    // Como no projeto antigo: espera a hora chegar (até 5 s) antes de seguir
    struct tm timeinfo;
    getLocalTime(&timeinfo);
  } else {
    // Não conectou, mas o programa segue (o Wi-Fi continua tentando sozinho)
    Serial.printf("WiFi NAO conectou no setup (status=%d). Seguindo; a reconexao automatica continua tentando.\n",
                  (int)WiFi.status());
  }

  // Conexão segura sem checar o certificado do servidor (mais simples).
  // Igual ao projeto antigo: configurada DEPOIS da tentativa de conectar ao
  // Wi-Fi, e sem limite de tempo próprio (usa o padrão da biblioteca).
  client.setInsecure();

  // ----- Contratando os dois "funcionários" -----

  // Mostra a memória livre ANTES de contratar os funcionários (para comparar depois)
  Serial.printf("Setup: memoria livre antes das tarefas: %d | maior bloco: %d\n",
                ESP.getFreeHeap(), ESP.getMaxAllocHeap());

#if HABILITAR_IA
  // O Ouvinte (IA + microfone) vai trabalhar no núcleo 1
  xTaskCreatePinnedToCore(TaskCore1_IA, "TaskIA", 1024 * 32, NULL, 2, NULL, 1);
#else
  // MODO DE TESTE: o Ouvinte NÃO é contratado (microfone e IA ficam desligados)
  Serial.println("*** MODO DE TESTE: IA e microfone DESLIGADOS (HABILITAR_IA = 0) ***");
#endif

  // O Sentinela (fumaça + DHT11) também trabalha no núcleo 1, como o loop() do
  // projeto antigo. Ele roda mesmo com a IA desligada (HABILITAR_IA = 0).
  // Pilha de 8 KB e prioridade 1 (a mais baixa): não atrapalha o microfone.
  xTaskCreatePinnedToCore(TaskSentinela, "TaskSentinela", 1024 * 8, NULL, 1, NULL, 1);

  // O Carteiro (Telegram) vai trabalhar no núcleo 0
  delay(1000);
  xTaskCreatePinnedToCore(TaskCore0_Telegram, "TaskTelegram", 1024 * 32, NULL, 1, NULL, 0);
}

// ============================================================
// LOOP PRINCIPAL
// Normalmente é onde o Arduino faz seu trabalho, mas aqui todo o trabalho
// está nas tarefas dos dois núcleos. Então o loop só "dorme".
// ============================================================
void loop() {
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}

// Trava de segurança do Edge Impulse: se o modelo de IA não for de
// microfone (áudio), o programa nem compila, evitando erros estranhos.
#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_MICROPHONE
#error "Invalid model for current sensor."
#endif
