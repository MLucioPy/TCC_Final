# Lista de materiais — Plataforma IoT de Sensoriamento Multimodal

**Projeto:** Plataforma IoT para Detecção de Situações de Risco com Sensoriamento Multimodal (Time 05)  
**Autor:** Moisés Lúcio Moreira  
**Preços consultados em:** 21/11/2026 · valores em reais (BRL)

## Materiais para uma unidade

| # | Material | Qtd. | Especificação / observação | Preço de referência | Compra |
|---:|---|---:|---|---:|---|
| 1 | Placa de desenvolvimento ESP32-S3 | 1 | **ESP32-S3 DevKitC-1 N16R8**, 16 MB Flash e 8 MB PSRAM. Conferir a variante N16R8 e a compatibilidade da PSRAM OPI com a placa anunciada. | **R$ 98,71** no boleto; preço normal exibido: R$ 103,90. [1] | [Blutu — ESP32-S3 N16R8](https://www.blutu.com.br/modulos-e-sensores/placa-devkitc-1-esp32-s3-n16r8-16mb-flash-8mb-psram) |
| 2 | Microfone digital | 1 | **INMP441**, interface I²S. Ligar VDD a 3,3 V, GND a GND e L/R a GND para selecionar o canal esquerdo. SCK → GPIO 4, WS → GPIO 5 e SD → GPIO 6. [2] | **R$ 15,13** no Pix/boleto. [2] | [UsinaInfo — INMP441](https://www.usinainfo.com.br/sensor-de-som/microfone-omnidirecional-inmp441-para-projetos-5485.html) |
| 3 | Detector convencional de fumaça | 1 | **Intelbras DFC 421 UN**. Opera em 12/24 Vcc e disponibiliza contato seco NA/NF. O contato informado é limitado a 30 V / 100 mA. A instalação deve ser feita conforme o fabricante e por técnico qualificado. [3] | **R$ 71,90**; frete à parte. [3] | [Eletrônica Santana — DFC 421 UN](https://www.eletronicasantana.com.br/detector-de-fumaca-convencional-dfc-421-un-intelbras/p) |
| 4 | Sensor de temperatura e umidade | 1 | **Módulo DHT11** com saída digital. A página consultada descreve módulo com cabo. Se usar o componente DHT11 avulso, comprar também o resistor pull-up de 10 kΩ da linha 9. [4] | **R$ 7,92** no Pix; outra condição exibida: R$ 8,34. [4] | [Eletrogate — módulo DHT11](https://www.eletrogate.com/modulo-sensor-umidade-temperatura-dht11-com-led) |
| 5 | Fonte USB para o ESP32-S3 | 1 | Saída USB de **5 V / 2 A**, adequada à recomendação do projeto de fonte estável de 5 V com pelo menos 1 A. Não usar esta fonte de 5 V para alimentar o DFC 421 UN. [5] | **R$ 5,73**; a página também mostrava R$ 4,58 por depósito/boleto. [5] | [Depósito FL — fonte USB 5 V / 2 A](https://www.distribuidoradecftv.com.br/deposito-fl/carregadores-de-celular/fonte-5v-2a-usb-carregador-de-celular-universal) |
| 6 | Fonte separada para o detector | 1 | Fonte estabilizada **12 V / 1 A** para o DFC 421 UN, cuja especificação operacional é 12/24 Vcc. Confirmar plugue, polaridade e requisitos da instalação do detector. [3] [6] | **R$ 18,05** no Pix; R$ 19,00 em outra condição exibida. [6] | [Central Cabos — fonte 12 V / 1 A](https://www.centralcabos.com.br/fonte-chaveada-12v-1a-bivolt---plug-5-5mm-x-2-1mm/p) |
| 7 | Cabo USB-A para USB-C | 1 | Cabo de dados e carga, 1 m, para alimentar/programar a placa com conector USB-C. Verificar se o carregador da linha 5 já inclui cabo; se incluir, este item pode não ser necessário. [7] | **R$ 9,43** no Pix/boleto. [7] | [Fortrek — cabo USB-A para USB-C](https://loja.fortrek.com.br/cabo-usb-a-para-usb-c-fortrek-u111-1-metro-preto) |
| 8 | Jumpers de prototipagem | 1 kit | Kit com 40 fios Dupont macho-macho, 20 cm. Verificar o tipo de terminais necessário para os módulos e para a placa; para alguns sensores será preciso cabo macho-fêmea ou borne/conector apropriado. [8] | **R$ 9,41** por kit. [8] | [Saravati — jumpers macho-macho](https://www.saravati.com.br/cabos-jumper-premium-macho-macho-20cm-40-vias.html) |
| 9 | Resistor pull-up para DHT11 avulso | 1 | **10 kΩ, 1/4 W, 5%**. Condicional: necessário se o DHT11 for usado sem módulo/pull-up integrado; não é necessário comprar separadamente quando o módulo já possuir o resistor. [9] | **R$ 0,08** por unidade. [9] | [MegaMaker — resistor 10 kΩ](https://www.megamaker.com.br/resistor-10k-5-14w-10-unidades) |

## Estimativa de custo

- **Subtotal dos itens 1–8:** **R$ 236,28**.
- **Subtotal incluindo o resistor opcional (item 9):** **R$ 236,36**.
- Se o carregador USB de 5 V for comprado pela condição promocional de R$ 4,58 indicada pela loja, o subtotal dos itens 1–8 fica em **R$ 235,13**.

Os valores são referências observadas nas páginas dos vendedores na data acima. Não incluem frete, impostos eventualmente calculados por região, variações de pagamento ou custos de instalação. A placa, o detector e as fontes devem ser conferidos novamente no checkout antes da compra.

## Ligações e compatibilidade importantes

- **Não ligar 12 V ao GPIO 15.** O detector DFC 421 UN precisa da sua alimentação de 12/24 V, mas o ESP32-S3 trabalha com GPIO de 3,3 V. Somente o contato seco isolado, se confirmado na configuração instalada, pode fechar entre GPIO 15 e GND; o firmware usa `INPUT_PULLUP` e interpreta `LOW` como alarme. Se a saída do detector não estiver eletricamente isolada da alimentação, não a ligue diretamente à placa: use interface de isolamento/proteção adequada e valide o circuito com técnico qualificado. [3]
- **Manter fontes separadas:** a fonte USB de 5 V alimenta a placa ESP32-S3; a fonte de 12 V alimenta o detector. Não unir a linha positiva de 12 V ao ESP32 ou aos sensores.
- **INMP441 e DHT11:** alimentar a partir de 3,3 V da placa e não aplicar 5 V aos sinais/GPIOs. No INMP441, ligar L/R a GND para o canal esquerdo.
- **Debug:** o jumper de GPIO 10 para GND, descrito na documentação técnica, pode ser feito com um fio jumper do kit; não é necessário um módulo adicional.
- **Detector de fumaça:** a página do fabricante informa que o produto deve ser configurado por técnico qualificado. Esta lista de materiais não substitui a central, o cabeamento, as regras de instalação ou os ensaios exigidos para um sistema de alarme de incêndio.

## Referências

[1]: https://www.blutu.com.br/modulos-e-sensores/placa-devkitc-1-esp32-s3-n16r8-16mb-flash-8mb-psram "Placa ESP32-S3 DevKitC-1 N16R8 — Blutu"
[2]: https://www.usinainfo.com.br/sensor-de-som/microfone-omnidirecional-inmp441-para-projetos-5485.html "Microfone INMP441 — UsinaInfo"
[3]: https://www.eletronicasantana.com.br/detector-de-fumaca-convencional-dfc-421-un-intelbras/p "Detector de fumaça DFC 421 UN — Eletrônica Santana"
[4]: https://www.eletrogate.com/modulo-sensor-umidade-temperatura-dht11-com-led "Módulo DHT11 — Eletrogate"
[5]: https://www.distribuidoradecftv.com.br/deposito-fl/carregadores-de-celular/fonte-5v-2a-usb-carregador-de-celular-universal "Fonte USB 5 V / 2 A — Depósito FL"
[6]: https://www.centralcabos.com.br/fonte-chaveada-12v-1a-bivolt---plug-5-5mm-x-2-1mm/p "Fonte 12 V / 1 A — Central Cabos"
[7]: https://loja.fortrek.com.br/cabo-usb-a-para-usb-c-fortrek-u111-1-metro-preto "Cabo USB-A para USB-C — Fortrek"
[8]: https://www.saravati.com.br/cabos-jumper-premium-macho-macho-20cm-40-vias.html "Jumpers Dupont macho-macho — Saravati"
[9]: https://www.megamaker.com.br/resistor-10k-5-14w-10-unidades "Resistor 10 kΩ 1/4 W — MegaMaker"
