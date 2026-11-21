# Plataforma IoT para Detecção de Situações de Risco com Sensoriamento Multimodal
**Relatório Técnico e Guia de Desenvolvimento**

**Projeto:** Sistema embarcado inteligente para ambientes fechados (banheiros, vestiários) que reconhece palavras-chave de emergência e detecta fumaça, enviando alertas em tempo real via Telegram.
**Autor:** Moisés Lúcio Moreira (Time 05) - Pós-Graduação em Internet das Coisas (IoT) - IFSP Catanduva.

---

## 1. Resumo
O projeto consiste em um dispositivo IoT de baixo custo, encapsulado em um detector de fumaça convencional, projetado para aumentar a segurança em ambientes fechados e vulneráveis. O sistema utiliza um microcontrolador ESP32-S3 e uma arquitetura de software baseada no sistema operacional de tempo real FreeRTOS. 

O fluxo de funcionamento divide o processamento em dois núcleos (Dual-Core): o Núcleo 1 é dedicado à captura de áudio via protocolo I2S e à inferência de um modelo de Inteligência Artificial (Edge Impulse/TinyML) para reconhecer as palavras "Socorro" e "Ajuda", além de monitorar sensores de fumaça e temperatura/umidade (DHT11). O Núcleo 0 gerencia a conectividade Wi-Fi e a comunicação segura (HTTPS) com a API do Telegram. A comunicação entre os núcleos ocorre por meio de filas (Queues). O relatório cobre a arquitetura, as decisões de engenharia, os testes de bancada e as limitações do protótipo.

## 2. Introdução e objetivos
### 2.1 Contexto
Ambientes como banheiros e vestiários públicos são locais de vulnerabilidade. O projeto partiu da necessidade de criar uma camada de segurança discreta que não dependa de câmeras (que geram problemas de privacidade) ou de botões de pânico (que podem ser inacessíveis em uma situação de risco). A versão atual foca no processamento local (Edge Computing) e na resiliência de rede.

### 2.2 Objetivo geral
Desenvolver um protótipo funcional, de baixo custo (abaixo de R$300), capaz de reconhecer pedidos de socorro verbais e situações de incêndio, notificando uma equipe de segurança em tempo real sem armazenar áudio dos usuários.

### 2.3 Objetivos específicos
*   Implementar um modelo de Inteligência Artificial para reconhecimento de palavras-chave (KWS) rodando localmente no ESP32-S3.
*   Integrar sensores de fumaça (via contato seco) e ambientais (DHT11) em um único dispositivo.
*   Desenvolver um sistema de envio de alertas via bot do Telegram com gestão dinâmica de contatos.
*   Garantir que a instabilidade da rede Wi-Fi não cause a perda de alertas críticos (lógica de *retry*).

## 3. Escopo e requisitos
### 3.1 Dentro do escopo
*   **Hardware:** ESP32-S3 N16R8, Microfone I2S INMP441, Sensor de Fumaça Convencional (DFC 421 UN), DHT11.
*   **Software/Firmware:** C++ com FreeRTOS, TensorFlow Lite Micro (via Edge Impulse), API do Telegram (UniversalTelegramBot).
*   **Lógica:** Arquitetura Dual-Core, filas de mensagens e envio resiliente com *backoff* exponencial.

### 3.2 Fora do escopo
*   **Display OLED:** Removido para priorizar o processamento da IA e evitar bloqueios no barramento I2C.
*   **Isolamento por Optoacoplador e Regulador Buck:** O protótipo final utiliza a saída de contato seco ligada diretamente ao GND do ESP32 com *pull-up* interno, e alimentação via USB de 5V.
*   **Nuvem/Backend próprio:** O sistema utiliza a infraestrutura serverless do Telegram.
*   **Armazenamento de áudio:** Por questões de privacidade e limitação de hardware, nenhum áudio é gravado.

### 3.3 Requisitos principais
| ID | Requisito | Onde é tratado |
| :--- | :--- | :--- |
| R1 | Detecção de palavras-chave ("Socorro" e "Ajuda") com alta confiança. | `TaskCore1_IA` / Modelo Edge Impulse |
| R2 | Detecção de fumaça e temperatura/umidade. | `TaskSentinela` |
| R3 | Envio de alertas via Telegram para múltiplos usuários. | `TaskCore0_Telegram` |
| R4 | Tolerância a falhas de rede sem perda de alertas. | Lógica de Fila + `enviarParaFaltantes` |
| R5 | Gestão remota de destinatários (Adicionar/Remover). | Comandos `add:` e `del:` via Telegram |
| R6 | Interface de usuário via aplicativo Telegram. | App nativo do Telegram |
| R7 | Privacidade (processamento de áudio local). | Extração de features e inferência no ESP32 |

## 4. Arquitetura
O sistema foi construído sobre uma arquitetura de três camadas lógicas principais: o Dispositivo (Hardware), o Firmware Embarcado e a Interface de Comunicação (Telegram).

| Camada | Componentes |
| :--- | :--- |
| **Hardware/Dispositivo** | ESP32-S3, Microfone INMP441, Sensor de Fumaça (Contato Seco), DHT11. |
| **Firmware (Dual-Core)** | **Core 1:** `TaskCore1_IA` (Inferência de Áudio) e `TaskSentinela` (Leitura de Sensores).<br>**Core 0:** `TaskCore0_Telegram` (Comunicação HTTPS).<br>**Comunicação Interna:** FreeRTOS Queue (Fila de mensagens). |
| **Comunicação/Interface** | Wi-Fi (HTTPS), API do Telegram, Bot interativo. |

*Detalhes de pinagem, diagramas e protocolo de dados estão no arquivo [docs/Documentacao_Tecnica.md](docs/Documentacao_Tecnica.md).*

## 5. Implementação
### 5.1 Firmware (Camada do Dispositivo)
*   **Arquitetura Dual-Core:** O envio de mensagens via SSL/TLS no Telegram pode travar o processador por até 3 segundos. Para evitar perda de frames de áudio, o Core 1 fica exclusivamente dedicado à IA e aos sensores (tempo real), enquanto o Core 0 lida com a latência da internet.
*   **Filtro de Falsos Positivos (Limiar Assimétrico):** A `TaskCore1_IA` exige uma margem de confiança assimétrica para disparar alertas: 80% para "Socorro" e 65% para "Ajuda", garantindo que a palavra vencedora seja pelo menos 50% maior que a segunda colocada.
*   **Debounce do Sensor de Fumaça:** A `TaskSentinela` lê o sensor a cada 200ms, mas só confirma uma mudança de estado após 2 leituras consecutivas idênticas, evitando alarmes falsos por picos elétricos.
*   **Privacidade:** O áudio bruto (`sampleBuffer32`) é convertido para 16 bits, processado para extração de MFCC e descartado da memória imediatamente após a inferência.

### 5.2 Comunicação e Interface
*   **Envio Resiliente:** Se a conexão Wi-Fi falhar durante um alerta de emergência, a mensagem não é perdida. A `TaskCore0_Telegram` mantém o alerta na memória e tenta reenviá-lo com atrasos progressivos (5s, 10s, 15s...) por até 5 minutos.
*   **Bot do Telegram:** O bot é capaz de receber comandos administrativos em tempo real (`add:ID`, `del:ID`, `/list`) e salva a lista de contatos na memória NVS (não volátil) do ESP32.

## 6. Metodologia de testes
A validação do sistema ocorreu em diferentes níveis:
*   **Testes de IA (PC):** Treinamento no Edge Impulse com dataset sintético (Piper TTS) e dados reais. Validação com matriz de confusão.
*   **Testes de Bancada (Hardware):** Leitura dos sensores via Monitor Serial, verificação de clipping do microfone I2S e injeção de falhas no contato seco da fumaça.
*   **Teste de Estresse de Rede:** Desligamento proposital do roteador Wi-Fi durante um alerta para validar a lógica de retenção de mensagens e o *backoff* exponencial.

## 7. Resultados
### 7.1 O que foi validado
*   **Acurácia da IA:** O modelo DS-CNN treinado no Edge Impulse atingiu estabilidade de detecção, disparando alertas com alta confiança em testes controlados.
*   **Estabilidade do Sistema:** A arquitetura Dual-Core eliminou os erros de *Watchdog Reset* (reinicializações inesperadas) que ocorriam quando a IA e o Wi-Fi rodavam no mesmo núcleo.
*   **Entrega Garantida:** A fila de mensagens + rotina de *retry* garantiu que nenhum alerta fosse perdido durante oscilações da rede.

### 7.2 Problemas encontrados e tratados
| Item | Problema | Causa e correção | Status |
| :--- | :--- | :--- | :--- |
| 1 | Falsos positivos para "Socorro" em ambiente silencioso | Ruídos transitórios. Correção: Aumento do limiar de confiança para 80% e exigência de margem sobre a segunda classe. | Aprovado |
| 2 | Erro `Bad file number` no SSL | Socket anterior não era fechado. Correção: Inserção de `client.stop()` antes de novas tentativas de envio. | Aprovado |
| 3 | Estouro de memória (Crash) | Concorrência entre Wi-Fi e Áudio. Correção: Separação das tarefas via FreeRTOS em núcleos distintos. | Aprovado |
| 4 | **Falsos positivos em ambientes com muita conversa** | **Alto volume de vozes sobrepostas e ruído de fundo.** O modelo confunde fonemas com "Socorro". | **Parcial / Em aberto** |

## 8. Problemas pendentes de solução
### 8.1 Falsos Positivos em Ambientes com Alto Volume de Conversa
Durante os testes de campo, foi observado que em ambientes onde há um fluxo contínuo e elevado de conversação (muitas pessoas falando simultaneamente), a rede neural pode gerar falsos positivos para a palavra "Socorro".
*   *Causa:* O dataset de treinamento foi construído majoritariamente com vozes sintéticas (Piper TTS) e complementado com algumas gravações reais isoladas. Esse dataset não contempla a complexidade acústica de um ambiente real com sobreposição de vozes, eco e ruídos de fundo característicos de locais públicos.
*   *Solução Proposta:* A próxima etapa do projeto consistirá em coletar amostras de áudio diretamente no ambiente de uso final (in loco), capturando o ruído real e o murmúrio de conversas cruzadas. Esses dados serão inseridos no Edge Impulse para retreinar o modelo, ensinando a IA a classificar especificamente esse tipo de ruído como "Noise" (Ruído) ou "Unknown" (Desconhecido), aumentando a robustez do sistema.

## 9. Limitações
*   **Sensibilidade Acústica:** O modelo atual apresenta limitações em ambientes com altíssimo volume de conversação simultânea, podendo gerar falsos alertas (vide seção 8.1).
*   **Dependência de Energia:** O protótipo atual depende de uma fonte USB contínua de 5V, não possuindo bateria interna.
*   **Palavras-chave Restritas:** O modelo foi treinado especificamente para "Socorro" e "Ajuda". Outras palavras de emergência não são reconhecidas.
*   **Dependência de Rede:** Sem conexão Wi-Fi, o sistema detecta o evento e retém a mensagem, mas não consegue entregá-la até que a rede retorne.

## 10. Aspectos éticos e legais
O projeto foi desenhado em conformidade com os princípios da LGPD. **O dispositivo não realiza a gravação, armazenamento ou transmissão de áudio bruto.** O microfone é utilizado exclusivamente para extração matemática de coeficientes (MFCCs) para a Inteligência Artificial local. Nenhum dado biométrico ou de voz é enviado para servidores de terceiros; apenas o texto do alerta de emergência é transmitido via API do Telegram.

## 11. Conclusão e trabalhos futuros
O projeto cumpriu o objetivo central de desenvolver um protótipo de segurança multimodal de baixo custo. A união de Inteligência Artificial Embarcada (TinyML) com uma arquitetura de software resiliente (FreeRTOS Dual-Core) provou ser viável e robusta.

**Próximos passos:**
*   Coletar um dataset real de "ruído de fundo" (conversas cruzadas) no ambiente final de uso e retreinar a IA (Versão 2.0).
*   Implementar uma bateria LiPo com circuito de carregamento para autonomia.
*   Adicionar um botão físico de pânico (hardware) como redundância.

## 12. Referências do projeto
*   Repositório Oficial: [Link do GitHub a ser inserido]
*   Edge Impulse Documentation: https://docs.edgeimpulse.com/
*   Espressif ESP32-S3 Technical Reference Manual.
*   Telegram Bot API Documentation.