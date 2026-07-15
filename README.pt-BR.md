# Color Equalizer

[English](README.md)

Color Equalizer permite ajuste seletivo de matiz, saturação e brilho em dez
regiões contínuas de cor.

O plugin foi inspirado no fluxo *scene-referred* do módulo Color Equalizer do
darktable. A adaptação para OFX mantém a ideia central: alterar cores a partir
da cor original do pixel, com resposta contínua entre regiões vizinhas e menor
tendência a bordas artificiais em gradientes.

Color Equalizer é distribuído pelo
[MCNexus](https://github.com/ciqueira/MCNexus). O Nexus fornece distribuição,
licenciamento, entrega de atualizações e suporte ao produto. O MCNexus é o
aplicativo desktop usado para ativar, instalar, atualizar e gerenciar o plugin.

## Plugins Incluídos

| Plugin | Versão | Distribuição | Chave Gratuita | Apoie o Projeto |
| --- | --- | --- | --- | --- |
| Color Equalizer | Atual | OpenKey | [Obter Chave](https://bridge.magnociqueira.com.br/github/claim?t=colorequalizer-oss&tmpl=bf1b283c-c8ed-4608-91a9-348a342a55a4&sig=67251aabd72f21ba) | [Torne-se um Apoiador](https://bridge.magnociqueira.com.br/commerce/start?t=colorequalizer-oss&offer=color-equalizer-supporter) |

## Color Equalizer

Color Equalizer expande o fluxo convencional de seis cores para dez regiões
conectadas:

```text
Red · Orange · Yellow · Lime · Green
Teal · Cyan · Blue · Purple · Magenta
```

Cada região possui controles independentes para:

- `Hue`: desloca a matiz em direção a tons vizinhos.
- `Saturation`: aumenta ou reduz a intensidade de cor.
- `Brightness`: altera a presença luminosa da região sem criar um key separado.

Os grupos `Hue Equalizer`, `Saturation Equalizer` e `Brightness Equalizer`
possuem controles por cor e um controle master para escalar o efeito do grupo.
Matiz, saturação e brilho são avaliados a partir da mesma posição cromática,
mantendo continuidade entre bandas adjacentes.

Nos modelos `RGB Spherical` e `OKLCH`, o plugin faz uma conversão para o modelo
selecionado, aplica os três deltas combinados e retorna para RGB. No modo
`RGB Direct`, a correção acontece diretamente na relação entre canais RGB.

O controle `Model / Space Type` define como a posição de cor é interpretada:

- `RGB Direct`: trabalha diretamente com a relação entre canais RGB.
- `RGB Spherical`: usa uma leitura esférica ao redor do eixo neutro, com
  direção de cor, distância do cinza e intensidade no mesmo modelo.
- `OKLCH`: usa uma leitura perceptual baseada em Oklab, útil para separar
  matiz, croma e luminosidade.

Presets de entrada disponíveis:

- ACES AP1 / ACEScct
- DaVinci Wide Gamut / Intermediate
- ARRI Wide Gamut 3 / LogC3
- ARRI Wide Gamut 4 / LogC4

## Modelos de Processamento

O modelo de processamento é paralelo. Os grupos Hue, Saturation e Brightness
não formam uma pilha serial em que um ajuste alimenta o próximo. Os três grupos
são pré-calculados em uma LUT única, amostrada a partir da cor original do
pixel, e aplicados juntos no mesmo passo de processamento.

```text
Input RGB -> posição de cor original

posição original -> Hue Equalizer        -> delta de matiz
posição original -> Saturation Equalizer -> ganho de saturação
posição original -> Brightness Equalizer -> delta de brilho

Input RGB + deltas combinados -> Output RGB
```



## Suporte de Plataforma

Os builds atuais suportam:

- macOS, Apple Silicon e Macs Intel compatíveis
- Windows x64

Backends de processamento suportados:

- Metal no macOS
- CUDA no Windows

## Instalação

1. Use o link `Obter Chave` acima para gerar a licença OpenKey com uma conta
   GitHub.
2. Abra o MCNexus.
3. Ative o Color Equalizer com a chave emitida.
4. Instale ou atualize o plugin pelo MCNexus.

Perda de chave: o mesmo link de solicitação, aberto com a mesma conta GitHub,
recupera a licença já emitida.

## Apoie o Projeto

O Color Equalizer permanece disponível gratuitamente com todos os recursos do
plugin publicados atualmente. Se ele for útil no seu trabalho, você pode apoiar
opcionalmente sua manutenção e a continuidade do desenvolvimento.

O benefício Color Equalizer Supporter inclui:

- suporte prioritário e privado por e-mail por 12 meses; e
- comunicações operacionais por e-mail sobre releases, compatibilidade,
  manutenção, segurança e alterações relevantes do Color Equalizer.

A compra não adiciona recursos exclusivos ao plugin. Para entregar e vincular
o benefício Supporter, o Nexus poderá emitir uma nova chave técnica ou associar
e atualizar uma chave existente. Não é necessário obter a chave gratuita antes
do checkout; usuários existentes devem usar a mesma conta GitHub e o mesmo
e-mail verificado.

[Comprar Color Equalizer Supporter](https://bridge.magnociqueira.com.br/commerce/start?t=colorequalizer-oss&offer=color-equalizer-supporter)

Antes de comprar, leia os
[Termos de Supporter](https://legal.magnociqueira.com.br/pt-BR/products/color-equalizer/terms/),
a [Política de Reembolso](https://legal.magnociqueira.com.br/pt-BR/products/color-equalizer/refunds/),
a [Política de Privacidade](https://legal.magnociqueira.com.br/pt-BR/products/color-equalizer/privacy/)
e a [Política de Suporte](https://legal.magnociqueira.com.br/pt-BR/products/color-equalizer/support/).
Cópias Markdown permanecem em [`legal/`](legal/pt-BR/README.md) para consulta
no repositório. Mensagens sobre outros produtos não são incluídas
automaticamente e exigirão escolha separada de marketing se esse recurso for
oferecido no futuro.

## Licença

Color Equalizer é *source-available* para revisão, documentação e transparência
técnica. O acesso público a este repositório não torna o projeto software open
source.

Consulte:

- [LICENSE.md](LICENSE.md)
- [BINARY_LICENSE.md](BINARY_LICENSE.md)
- [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
- [Documentos legais do produto](legal/pt-BR/README.md)

## Releases Binários

Os releases binários oficiais são distribuídos pelo Nexus e instalados com o
MCNexus. Use apenas canais oficiais do MCNexus ou do projeto para binários,
atualizações e ativação.
