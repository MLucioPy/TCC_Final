

\---



\### 3. Arquivo: `docs/Manual\_de\_Uso.md`



```markdown

\# \[Plataforma IoT para Detecção de Situações de Risco com Sensoriamento Multimodal] · Manual de Uso



\*\*Projeto:\*\* Plataforma IoT para Detecção de Situações de Risco com Sensoriamento Multimodal (Time 05)

\*\*Autor do projeto:\*\* Moisés Lúcio Moreira

\*\*Subtítulo:\*\* Como ligar, configurar e interpretar os alertas do sistema de monitoramento.



\## 1. Antes de começar



\### 1.1 O que o aparelho/sistema faz

O sistema funciona como uma "babá eletrônica inteligente" para ambientes fechados (como banheiros e vestiários). Ele fica ouvindo o ambiente e, caso reconheça palavras de emergência ("Socorro" ou "Ajuda") ou detecte a presença de fumaça, envia imediatamente uma mensagem de alerta para o aplicativo Telegram das pessoas responsáveis (segurança ou gerência).



\### 1.2 Uso responsável

\*\*Leia antes de usar:\*\* Este sistema processa áudio continuamente no ambiente. Ele \*\*não grava, armazena ou transmite o áudio bruto\*\* para nenhum servidor; a Inteligência Artificial atua apenas localmente no dispositivo (Edge Computing) para identificar a palavra-chave. O dispositivo \*\*não substitui\*\* sistemas de segurança oficiais, câmeras de monitoramento ou a ação humana. Em caso de emergência real, as autoridades devem ser acionadas imediatamente. 

Além disso, para o pleno funcionamento da Inteligência Artificial, o dispositivo deve ser instalado em locais onde o ruído de conversas simultâneas não seja excessivamente alto. Em locais com muitas pessoas falando ao mesmo tempo, o sistema pode gerar alertas falsos por confundir o ruído com a palavra de socorro.



\### 1.3 O que você precisa

\*   O protótipo do dispositivo (encapsulado com o ESP32-S3, microfone INMP441, DHT11 e sensor de fumaça).

\*   Uma fonte de alimentação USB de 5V (carregador de celular ou powerbank).

\*   Uma rede Wi-Fi de 2.4 GHz disponível e estável no local.

\*   Um smartphone com o aplicativo \*\*Telegram\*\* instalado e conectado à internet.



\## 2. Primeira vez: instalar e configurar



\### 2.1 Instalar o aplicativo/software

O projeto não possui um aplicativo próprio. O usuário final utilizará o aplicativo oficial do \*\*Telegram\*\*. Baixe-o na Google Play Store (Android) ou App Store (iOS).



\### 2.2 Conectar o dispositivo

\*   \*\*Passo 1:\*\* Ligue o dispositivo na tomada USB. 

\*   \*\*Passo 2:\*\* O firmware do ESP32 já deve estar configurado com o nome e senha do Wi-Fi local (via código fonte). Ao ligar, ele se conectará automaticamente.

\*   \*\*Passo 3:\*\* Abra o Telegram no celular e procure pelo bot do projeto (ex: `@NomeDoSeuBot`). Envie a mensagem `/start`. 

\*   \*\*Passo 4:\*\* Se tudo estiver correto, o bot responderá com a mensagem: `✅ Sistema TCC Iniciado (Dual-Core)!`.



\### 2.3 Permissões do aplicativo

\*Não implementado.\* Como o sistema utiliza o aplicativo nativo do Telegram, as permissões são as mesmas já utilizadas pelo aplicativo no dia a dia do usuário. O sistema não solicita acesso a microfone ou câmera do celular.



\## 3. Usando em campo / em operação



\### 3.1 Roteiro de uso

\*   \*\*Passo 1:\*\* Instale o dispositivo no teto ou em uma parede alta do ambiente a ser monitorado (o alcance do microfone depende da acústica do local).

\*   \*\*Passo 2:\*\* Certifique-se de que a mensagem de inicialização no Telegram confirmou que o sistema está online.

\*   \*\*Passo 3:\*\* Durante o uso, o dispositivo fará a leitura da temperatura, umidade e fumaça a cada poucos segundos de forma silenciosa.

\*   \*\*Passo 4:\*\* O sistema não precisa ser desligado diariamente. Ele foi projetado para operar ininterruptamente.



\*\*Importante sobre a internet:\*\* O dispositivo depende da rede Wi-Fi para enviar os alertas. Se a internet cair, o sistema continuará tentando enviar a mensagem por até 5 minutos após a detecção do evento.



\### 3.2 Lendo a interface

A interface do usuário é a própria conversa no Telegram. Você receberá mensagens nos seguintes formatos:



| Indicador/Mensagem | Significado |

| :--- | :--- |

| `🚨 ALERTA DE FUMAÇA 🚨` | O sensor detectou fumaça. A mensagem virá acompanhada da temperatura e umidade do momento. |

| `✅ O alarme de fumaça foi normalizado.` | O ar foi limpo e o sensor de fumaça voltou ao normal. |

| `🗣️ ALERTA DE VOZ 🗣️` | A IA reconheceu a palavra "Socorro" ou "Ajuda". A mensagem mostrará qual palavra foi e a confiança (0.0 a 1.0). |

| `✅ Sistema TCC Iniciado` | O dispositivo ligou, conectou ao Wi-Fi e está pronto para uso. |



\## 4. Sincronizar / Transferir dados

\*Não implementado.\* O sistema não possui armazenamento local (como cartão SD ou banco de dados embarcado). Os dados são transmitidos em tempo real para o Telegram. 



\## 5. Usando a interface (Telegram)



| Recurso | Como usar |

| :--- | :--- |

| \*\*Receber alertas automáticos\*\* | Não requer ação. O dispositivo enviará as mensagens assim que detectar um evento. |

| \*\*Listar usuários ativos\*\* | Envie a mensagem `/list`. O bot responderá com os IDs das pessoas que estão recebendo os alertas. |

| \*\*Adicionar um novo responsável\*\* | Envie o comando `add:123456789` (substitua pelo ID do Telegram da pessoa). Todos os alertas futuros irão para ela. |

| \*\*Remover um responsável\*\* | Envie o comando `del:123456789`. A pessoa deixará de receber os alertas. |



\## 6. Envio a serviço externo / nuvem

\*Não implementado.\* A arquitetura do projeto foi pensada para ser direta (Serverless) entre o dispositivo (ESP32) e a API do Telegram, sem a necessidade de servidores ou bancos de dados em nuvem.



\## 7. Desligar e guardar

\*   \*\*Passo 1:\*\* Não há botão de "desligar" no software. Para encerrar a operação, basta remover o cabo de alimentação USB da tomada.

\*   \*\*Passo 2:\*\* Caso vá transportar o dispositivo, certifique-se de que os fios do microfone e sensores não sejam desconectados.

\*   \*\*Lembrete de segurança:\*\* O histórico de mensagens com alertas de ocorrência ficará salvo no aplicativo do Telegram no celular dos usuários. Não compartilhe capturas de tela desses alertas publicamente sem autorização.



\## 8. Problemas comuns



| O que acontece | O que fazer |

| :--- | :--- |

| \*\*O bot não responde ao comando `/start`\*\* | Verifique se o nome e a senha do Wi-Fi na rede não foram alterados. Desligue e ligue o dispositivo novamente. |

| \*\*O alerta de fumaça disparou sozinho\*\* | O sensor pode ter recebido um pico de energia. Aguarde 1 segundo. O próprio sistema possui um filtro de confirmação para ignorar picos isolados. Se persistir, verifique se há fumaça real. |

| \*\*Muitos alertas de voz falsos em horários de pico\*\* | O ambiente pode estar com excesso de conversas cruzadas (ruído). Aguarde o fluxo de pessoas diminuir ou reduza a sensibilidade do sistema ajustando o limiar de confiança no código fonte. O modelo será atualizado futuramente para ignorar este tipo de ruído. |

| \*\*Mensagem de erro no dispositivo\*\* | Conecte o dispositivo a um computador e abra o Monitor Serial (Arduino IDE) para verificar os logs de rede e diagnóstico. |

