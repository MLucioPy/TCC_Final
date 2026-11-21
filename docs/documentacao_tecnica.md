\# \[Plataforma IoT para Detecção de Situações de Risco com Sensoriamento Multimodal] · Arquitetura, Pinagem e Documentação Técnica



\*\*Projeto:\*\* Plataforma IoT para Detecção de Situações de Risco com Sensoriamento Multimodal (Time 05)

\*\*Autor do projeto:\*\* Moisés Lúcio Moreira



\## 1. Visão geral da arquitetura

O sistema é baseado em um microcontrolador ESP32-S3 que atua como um dispositivo de borda (Edge Computing). Ele captura áudio ambiente via protocolo I2S, processa um modelo de Inteligência Artificial (TinyML) localmente para reconhecer palavras de emergência ("Socorro" e "Ajuda"), monitora um sensor de fumaça convencional e um sensor de temperatura/umidade (DHT11). Ao detectar um evento crítico, o sistema envia alertas formatados via Wi-Fi (HTTPS) para a API do Telegram.



\### 1.1 Decisões de arquitetura

| Decisão | Motivo |

| :--- | :--- |

| \*\*Arquitetura Dual-Core com FreeRTOS\*\* | O envio de mensagens via HTTPS (criptografia SSL) bloqueia o processador por segundos. Separar a captura de áudio/sensores (Core 1) do envio de rede (Core 0) garante que a janela de amostragem do microfone nunca seja interrompida, mantendo a precisão da IA. |

| \*\*Uso do Edge Impulse (TFLite)\*\* | O treinamento e extração de features (MFCC) via script Python puro apresentou divergências matemáticas ao rodar no ESP32. O Edge Impulse gerou a biblioteca otimizada e idêntica ao treinamento, garantindo estabilidade. |

| \*\*Retenção de Alertas (Backoff Exponencial)\*\* | Para evitar a perda de alertas em caso de instabilidade do Wi-Fi, o sistema retém a mensagem na memória e tenta reenviá-la por até 5 minutos, aumentando o tempo entre as tentativas, antes de descartá-la. |



\## 2. Hardware e pinagem física



\### 2.1 Lista de materiais

| # | Item | Especificação usada no protótipo | Observação |

| :--- | :--- | :--- | :--- |

| 1 | Placa principal | ESP32-S3 N16R8 (16MB Flash, 8MB PSRAM) | Necessário habilitar a OPI PSRAM na IDE. |

| 2 | Sensor de Áudio | Microfone INMP441 (I2S) | Pino L/R deve ser ligado ao GND para usar o canal esquerdo. |

| 3 | Sensor de Fumaça | Detector Convencional DFC 421 UN | Utiliza saída de contato seco (NA/NF). |

| 4 | Sensor Ambiental | DHT11 | Requer resistor de pull-up de 10kΩ (se for o sensor solto). |

| 5 | Alimentação | Fonte 5V (USB / Powerbank / Carregador) | Alimentação direta da placa via porta USB. |



\### 2.2 Tabela de pinagem

| Módulo | Sinal do módulo | Pino da placa principal | Direção | Observações |

| :--- | :--- | :--- | :--- | :--- |

| INMP441 | SCK / WS / SD | GPIO 4 / 5 / 6 | Entrada | Comunicação I2S |

| DHT11 | DATA | GPIO 7 | Bidirecional | Requer Pull-up |

| Sensor de Fumaça | Contato Seco | GPIO 15 | Entrada | `INPUT\_PULLUP` (LOW = Alarme) |

| Debug (Hardware) | Jumper | GPIO 10 | Entrada | Ligar ao GND para ativar logs no Monitor Serial |



\### 2.3 Diagrama de ligações

\*\[Inserir o diagrama elétrico do projeto apenas com os componentes listados acima]\*



\### 2.4 Cuidados elétricos e operacionais

\*   \*\*Sensor de Fumaça:\*\* Como a ligação é feita diretamente no GPIO 15 (sem optoacoplador), é fundamental garantir que o contato seco do detector esteja isolado e não injete tensão de 12V no pino do ESP32. O contato deve apenas fechar o circuito para o GND (terra), acionando o `INPUT\_PULLUP`.

\*   \*\*Níveis Lógicos:\*\* O INMP441 e o DHT11 operam estritamente em 3.3V. A aplicação de 5V nestes pinos pode queimar os componentes.

\*   \*\*Alimentação:\*\* O ESP32-S3 consome picos de corrente durante a transmissão Wi-Fi. A fonte USB utilizada deve ser estável e fornecer pelo menos 1A.

\*   \*\*Cuidados Operacionais:\*\* Para garantir a assertividade da IA, o dispositivo não deve ser instalado próximo a fontes geradoras de ruído contínuo e alto (como secadores de cabelo, exaustores) ou em locais onde ocorram aglomerações com conversas simultâneas ininterruptas.



\### 2.5 Fotos do protótipo

\*\[Inserir fotos da montagem interna, da placa com os fios e do protótipo encapsulado no detector de fumaça]\*



\## 3. Firmware / Software embarcado



\### 3.1 Componentes

| Componente | Responsabilidade |

| :--- | :--- |

| \*\*TaskCore1\_IA (O Ouvinte)\*\* | Roda no Núcleo 1. Captura o áudio I2S, executa o modelo de IA e coloca os alertas de voz na Fila. |

| \*\*TaskSentinela (O Vigia)\*\* | Roda no Núcleo 1. Faz o polling do DHT11 (4s) e do Sensor de Fumaça (0.2s com debounce), enviando alertas ambientais à Fila. |

| \*\*TaskCore0\_Telegram (O Carteiro)\*\* | Roda no Núcleo 0. Lê a Fila, gerencia a conexão Wi-Fi, criptografia SSL e envia/recebe mensagens do Telegram. |

| \*\*Queue (Fila)\*\* | Mecanismo thread-safe do FreeRTOS para comunicação entre as tarefas (tamanho de 10 mensagens). |



\### 3.2 Partições / Uso de memória

\*   \*\*PSRAM:\*\* O modelo de IA quantizado (INT8) e o Tensor Arena do TensorFlow Lite são alocados na memória PSRAM (OPI) para liberar a RAM interna.

\*   \*\*Heap (RAM interna):\*\* Utilizado pelos buffers de áudio (I2S) e pela conexão TLS (SSL) do Telegram.



\### 3.3 Opções de configuração

| Opção configurável | Valor padrão | Função |

| :--- | :--- | :--- |

| `HABILITAR\_IA` | `1` | 1 = Modo normal. 0 = Desliga a IA para testar apenas o Telegram. |

| `LEITURAS\_CONFIRMACAO` | `2` | Número de leituras consecutivas para confirmar o alarme de fumaça (debounce). |

| `LIMIAR\_SOCORRO` | `0.80` | Confiança mínima (80%) para validar a palavra Socorro. |

| `LIMIAR\_AJUDA` | `0.65` | Confiança mínima (65%) para validar a palavra Ajuda. |



\### 3.4 Compilar e gravar

1\. Utilizar a \*\*Arduino IDE (v2.x)\*\*.

2\. Instalar o pacote de placas \*\*esp32 by Espressif Systems\*\*.

3\. Instalar as bibliotecas: `UniversalTelegramBot`, `ArduinoJson`, `DHT sensor library` e o `.zip` gerado pelo Edge Impulse.

4\. \*\*Atenção:\*\* Em \*Ferramentas\*, selecionar a placa "ESP32S3 Dev Module" e habilitar a opção \*\*PSRAM: OPI PSRAM\*\*.

5\. Realizar o upload via porta USB.



\## 4. Formato de dados / registro

\*Não implementado.\* O sistema não utiliza armazenamento de dados em formato binário ou banco de dados local. As mensagens são geradas em formato de texto (string) com formatação Markdown e enviadas diretamente para a API do Telegram.



\## 5. Protocolo de comunicação

A comunicação ocorre entre o ESP32-S3 e os servidores do Telegram (via Wi-Fi/HTTPS).



\### 5.1 ESP32-S3 → Telegram (Envio de Alertas)

| Mensagem | Quando | Significado |

| :--- | :--- | :--- |

| `ALERTA DE VOZ` | Quando a IA detecta "Socorro" ou "Ajuda". | Informa a palavra detectada, a confiança (0.0 a 1.0) e a data/hora do evento. |

| `ALERTA DE FUMAÇA` | Quando o sensor de fumaça muda de estado (Normal para Alarme). | Informa a detecção de fumaça, temperatura, umidade e a hora do evento. |

| `Mensagem de Normalização` | Quando o sensor de fumaça volta ao estado normal. | Informa que o perigo cessou. |

| `Sistema Iniciado` | Quando a placa é ligada e conecta ao Wi-Fi. | Informa que o dispositivo está online. |



\### 5.2 Telegram → ESP32-S3 (Comandos Remotos)

| Linha/Comando | Efeito |

| :--- | :--- |

| `/list` | O bot responde com a lista de IDs cadastrados. |

| `add:\[ID]` | Adiciona o ID numérico à lista de destinatários de alertas. |

| `del:\[ID]` | Remove o ID numérico da lista de alertas. |



\### 5.3 Garantias e recuperação

Para garantir a entrega, foi implementada uma Fila (Queue) no FreeRTOS. Se o Wi-Fi cair, a mensagem fica retida na "mão do Carteiro". O sistema tenta reenviar a mensagem com um atraso progressivo (5s, 10s, 15s... até 30s) por até \*\*5 minutos\*\*. Se houver múltiplos destinatários e apenas alguns receberem, as tentativas seguintes ocorrem apenas para quem ainda não recebeu a mensagem.



\## 6. Aplicativo / Software cliente

\*Não implementado.\* O projeto não possui um aplicativo mobile ou desktop próprio. A interface do usuário (cliente) é o \*\*próprio aplicativo do Telegram\*\*.



\## 7. Nuvem / Backend (opcional)

\*Não implementado.\* O escopo do projeto prevê o processamento de borda (Edge Computing) e a comunicação direta com a API do Telegram (Serverless), eliminando a necessidade de servidores intermediários e bancos de dados.



\## 8. Estrutura do repositório

```text

/Plataforma-IoT-Sensoriamento

│

├── README.md            (Guia de instalação e visão geral)

├── /firmware            (Código fonte .ino final - Dual-Core)

│   └── TCC\_Final\_DualCore.ino

├── /hardware            (Diagramas elétricos e BOM detalhada)

│   ├── diagrama\_esquematico.png

│   └── lista\_materiais.md

└── /docs                (Documentação técnica e slides da apresentação)

&#x20;   ├── Documentacao\_Tecnica.md

&#x20;   ├── Manual\_de\_Uso.md

&#x20;   └── Apresentacao\_Defesa.pptx

