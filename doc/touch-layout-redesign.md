# Layout touch por zonas

O overlay é C++/ImGui: `source/application/mobile/StarMobileControls.cpp/.hpp`.
O editor/persistência ficam em `StarMobilePlatform.cpp`; medidas e restrições
sem SDL ficam em `source/core/StarTouchLayout.hpp`. O host Android encaminha
ponteiros em `android/host/app/src/main/java/org/libsdl/app/SDLSurface.java`.
Não há XML de botões ou dependência de Quickbar para este layout.

## Adoção e preservação

Instalações novas recebem o padrão por zonas. Instalações existentes mantêm
IDs, ações, nomes, posições e visibilidade salvos. Para adotar o redesenho:
launcher → Controles/Overlay → Layout preset → Padrão → Aplicar novo layout →
Salvar. A aplicação reorganiza os controles padrão e sua visibilidade; mantém
ações/rótulos e guarda botões personalizados em Mais. Não altera saves, mods,
bindings de gamepad, regras de alcance/dano ou escala da GUI do jogo.

Os quatro presets são manuais e exclusivamente visuais:

| Layout | Visibilidade |
|---|---|
| Padrão | Dois sticks e sete botões principais; trava opcional é o oitavo |
| Minimalista | Movimento, mira, pulo, interação e pausa |
| Construção | Padrão mais alternância de mineração e bloco único na lateral esquerda |
| Gamepad Touch | Pausa e Mais, com atalhos na tela; sticks touch ocultos |

Não existe troca automática de esquema entre combate/mineração/construção.
A trava continua desligada por padrão; habilite a opção existente para mostrar
Travar/Próximo. Os ícones de mineração e mão trocam o estado já existente.
Um ponto/borda azul sinaliza mineração direcional, Shift ativo ou alvo
efetivamente travado. Os rótulos LIVRE/DIR e MÃO L/R não ficam fora do círculo.

## Medidas e posições

Coordenadas são centros, em porcentagem da **área segura**, origem superior
esquerda. O cálculo usa dp convertidos pela densidade SDL, nunca a resolução
2400×1080 como constante. A tabela é a saída de `touch_layout_tests --dump`
para uma área de referência **874×393 dp**, escala 100%, guias top 16% e coluna
direita top 32%, com trava visível. Não é uma captura do aparelho.

| Controle / ID | X (%) | Y (%) | Diâmetro (dp) | Zona / acesso |
|---|---:|---:|---:|---|
| Movimento / joystick | 10,1 | 79,6 | 128 | A, base de raio 64 |
| Pular / jump | 10,1 | 54,2 | 56 | A, acima do movimento |
| Agachar/descer / crouch | 16,7 | 54,2 | 44 | A |
| Mira / aimJoystick | 89,9 | 79,6 | 128 | B, base de raio 64 |
| Interagir / interact | 77,3 | 79,6 | 48 | B, arco de 110 dp |
| Usar mão secundária / rightHand | 79,0 | 65,6 | 48 | B, arco |
| Tecnologia / tech | 83,6 | 55,4 | 48 | B, arco |
| Travar alvo / targetLock | 89,9 | 51,7 | 48 | B, somente com trava habilitada |
| Pausa / pause | 4,1 | 25,0 | 48 | C, abaixo/ao lado do HUD |
| Mais / more | 95,9 | 40,7 | 48 | D, abaixo da coluna do jogo |
| Mineração livre/direcional / miningMode | 27,0 | 30,0 | 48 | D no Construção; Mais no Padrão |
| Bloco único / singleBlock | 27,0 | 46,0 | 48 | D no Construção; Mais no Padrão |

O menu de pausa também tem alvo de 48 dp, em vez de 36 dp, para respeitar o
mínimo solicitado de 44 dp. Não existem botões de troca de hotbar sobre ela:
toque e deslize usam o comportamento já implementado na hotbar.

| Botão secundário | Tamanho (dp) | Posição no Padrão |
|---|---:|---|
| Usar mão principal / leftHand | 44 | Mais, primeira célula disponível |
| Chat / chat | 44 | Mais, próxima célula |
| Roda de inventário / inventoryWheel | 44 | Mais, próxima célula |
| Roda de ações / actionWheel | 44 | Mais, próxima página/célula |
| Próximo alvo / targetNext | 44 | Mais, somente com trava habilitada |
| Q1 / quick1 | 44 | Mais; item do slot principal 1 |
| Q2 / quick2 | 44 | Mais; item do slot principal 2 |
| Q3 / quick3 | 44 | Mais; item do slot principal 3 |
| Minerar / miningMode | 44 | Mais |
| Bloco único / singleBlock | 44 | Mais |
| Trocar mão da mira / swapAimHand | 44 | Mais |
| Shift, Ctrl e giroscópio | 44 | Desligados; se habilitados, Mais |
| D-pad | 128 | Desligado; substituição manual do movimento |
| Contador de desempenho | Passivo | Desligado; não captura toques |
| Atalhos personalizados | ≥44 | Mais por padrão; quantidade sem limite fixo |

As células de Mais não têm posição fixa por ação: a paginação depende dos
controles habilitados e do espaço disponível. Na referência acima, as primeiras
células são **(84,7%;34,1%)**, **(84,7%;47,3%)**, **(96,6%;60,6%)**, todas de
44 dp. Toque em Mais novamente para avançar; após a última página, fecha.
Usar/soltar um atalho fecha o painel; sem interação, fecha após 3 segundos.
Abrir janelas ou perder foco também o fecha. Rodas continuam funcionando por
segurar, arrastar e soltar; soltar no centro cancela.

O cálculo adapta âncoras e escala às proporções 16:9/20:9 e aos insets. A escala
global aceita 50–150%; o ajuste automático limita o tamanho efetivo se necessário
para caber na lateral sem invadir o centro. Botões nunca encolhem abaixo de
44 dp. Desenho e hit-test usam o mesmo círculo, sem as antigas áreas invisíveis
maiores que o desenho. As margens/intervalos padrão são pelo menos 8 dp.

`MainInterface::touchOverlayHud` informa limites reais de hotbar, vida/status e
barra de ícones direita após aplicar a escala da interface. O overlay afasta
os controles adaptativos verticalmente, mantendo a zona lateral. Se um HUD
modificado ocupar todo o espaço disponível, o controle sem lugar é ocultado,
em vez de cobrir o HUD. É necessário ajustar um layout nessa situação extrema.
Painéis de mods que desenham fora desses limites não são descobertos
automaticamente; a conferência no aparelho ainda é necessária.

## Comportamento e aparência

- Fundo azul/preto a 35% em repouso, cerca de 65% ao pressionar; borda fina,
  ícones vetoriais claros e sombra. Pressão reduz o desenho a 92%, mantendo o
  alvo de toque. Fade após cerca de 3 segundos; contato restaura a opacidade.
- Um único atlas ImGui existente para formas/fontes, sem blend de inversão,
  callbacks GL ou criação de texturas por quadro. Ícones reais dos atalhos
  continuam no renderer nativo do jogo, em passagem separada já existente.
- Rótulos personalizados usam medida de glifos e reticências em limites UTF-8;
  não podem sair do círculo. Os controles padrão usam ícones.
- Movimento fixo/flutuante é configurável. A zona flutuante ocupa o terço
  inferior esquerdo; não captura o HUD, botões ou a área central do mundo.
  A base não se desloca sobre outro controle ou o HUD.
- Instalações novas usam **Usar enquanto toca no joystick de mira**. Segurar
  mantém a mão principal; soltar/cancelar libera. Usuários antigos conservam
  a opção anterior; podem escolher esse comportamento no launcher. Mirar sem
  usar, atacar fora da deadzone e atacar ao soltar continuam disponíveis.
- **Enable direct screen touch gestures** mantém a opção de mirar tocando no
  mundo. Mineração, armas e ferramentas continuam passando pelo uso normal
  do item equipado; não existe dano ou alcance especial.
- Cada fingerID mantém sua função até up/cancel. Cancelar um dedo não cancela
  os outros. Orientação, foco e janelas cancelam o conjunto com liberação de
  todas as ações. Nada de vibração foi acrescentado.
- O host Android ignora contatos novos com eixo maior que 48 dp, quando o
  aparelho informa essa medida. A rejeição dura até soltar; mouse e caneta não
  passam por esse filtro. Há também rejeição de borda no gesto do mundo.

## Editor e JSON

Abra a prévia pelo menu de controles do launcher. Selecione um círculo, arraste
ou use sliders; ajuste tamanho, opacidade, ação, texto e visibilidade. A opção
**Guardar no painel Mais** é editável também na lista de elementos, permitindo
editar botões de páginas que não estão visíveis na prévia. Arrastar um botão
do painel o torna permanente; a posição passa a ser relativa à área segura.
O toolbar recebe seus próprios toques; um controle atrás dele não rouba o arrasto.

A grade de 8 dp encaixa na soltura; desligue-a para posicionamento livre.
Guias proibidas e sobreposições aparecem em vermelho. As guias do launcher
são estimativas: não carregam os assets/mods do jogo. No mundo, a proteção usa
os limites reais descritos acima. Restaurar padrão pede confirmação; Desfazer
recupera a última alteração estrutural antes de salvar.

O arquivo JSON existente do launcher conserva `touch.elements` por ID.
Novos campos: `diameterDp`, `adaptive`, `panel`; ausência deles mantém a
geometria antiga. `floatingMovement`, `panelLeft`, `snapToGrid` são opções
globais. Há fallback para configurações antigas e campos desconhecidos são
preservados pelo serviço de persistência existente.

Layouts nomeados em `touch.layoutProfiles` guardam `name`, `orientation`,
`aspect`, `elements`; `touch.activeLayout` identifica o escolhido. Ao iniciar
ou mudar o tamanho/orientação, escolhe a variante de mesmo nome/orientação e
proporção mais próxima. Se não existir, mantém os elementos atuais de fallback.
Os presets adaptativos também recalculam âncoras sem precisar de variantes.
Não há leitura de arquivo/JSON no desenho por quadro.

## Validação e teste no Redmi Note 13

Testes puros: `touch_layout_tests` cobre 16:9/20:9, densidades 1–3, altura
360–720 dp, escalas 50–150%, recorte de 24 dp, padrão/construção, alvo mínimo,
separação, HUD, centro livre, painel, fade/estado de entrada, UTF-8 e conversão
de coordenadas com viewport normal/fullscreen e resolução reduzida.
O cálculo de geometria não aloca. Os testes não provam ausência de alocação
no restante do motor, nos ícones de itens ou na integração SDL.

O PR executa testes sem assets e a tradução no GitHub. Depois reutiliza
`android.yml` para o APK **Debug ARM64 com alocador do sistema**, via CMake
`android_apk`, que compila `libmain.so` e chama `:app:assembleDebug`. Não basta
rodar Gradle sobre bibliotecas nativas antigas. Nenhum APK é compilado no PC.
Assinatura, versão, ABI/SDK/NDK e compactação permanecem os existentes.

Roteiro no aparelho:

1. Instale o artefato do PR em ambiente de teste. Debug pode ter assinatura
   diferente do Release instalado; preserve seus saves antes de reinstalar.
   Para medir desempenho, gere depois Release/arm64/system_allocator=true.
2. Aplique Padrão e Salvar; abra um mundo com os mods habituais. Confira vida,
   hotbar e coluna direita nas escalas de GUI que costuma usar.
3. Mova e mire continuamente; com outros dedos pressione pulo, interação e
   mão secundária. Solte/cancele cada contato separadamente. Nenhuma ação deve
   ficar presa ou assumir o dedo de outra ação.
4. Compare movimento fixo/flutuante e mira direta. Segure ferramenta de
   mineração/construção e confirme a liberação ao tirar o dedo/abrir inventário.
5. Abra Mais, percorra todas as páginas, use Q1/Q2/Q3, rodas e chat. Teste o
   fechamento por uso, por 3 segundos sem interação e ao perder foco.
6. Edite nomes longos com acentos, posições, tamanho/opacidade, salve e reinicie.
   Compare os quatro presets, restauração/desfazer e duas variantes nomeadas
   com orientações diferentes.
7. Gire a tela, inverta o lado do notch e retorne de background. Teste também
   fullscreen rendering ligado e resolução de renderização reduzida.
8. Conecte gamepad; confirme os bindings anteriores e o layout Gamepad Touch.
   Meça FPS em Release no mesmo local/mundo/configuração. A meta de 60 FPS é
   de custo do overlay; a simulação e os mods podem limitar o FPS total.

Compilação pontual e testes automatizados não substituem instalação, revisão
visual e medição no Snapdragon 685. iOS/Switch/Linux/macOS e execução real do
launcher desktop precisam de validação própria.
