# Mineração livre e direcional

O botão **Mine** alterna **Mining: free** / **Mining: directional**. A mensagem
confirma a escolha; o botão mostra **LIVRE / DIR** e fica marcado quando ligado,
inclusive se a troca for feita pelo gamepad. Começa livre a cada sessão.
É uma escolha manual única, sem perfis por item ou contexto.

A posição, tamanho, rótulo e habilitação são editáveis como nos outros controles.
Comfortable oferece uma posição alternativa sem mover layouts salvos sozinho.
O gamepad permite remapear a ação; nenhum binding existente, incluindo R3, muda.

## Funcionamento

Com a opção direcional, aponte o joystick e use o comando habitual da ferramenta.
A direção seleciona a primeira célula com material, partindo do jogador, na
camada ativa. A mesma direção mantém o primeiro bloco; depois da remoção,
a seleção avança. A intensidade do analógico não precisa posicionar o cursor
sobre cada bloco.

- Suporta os tipos nativos **BeamMiningTool e MiningTool**, incluindo os itens
  de mods carregados como `.beamaxe` e `.miningtool`. Não depende de nomes,
  traduções ou uma lista de mods.
- Preserva a área normal e o bloco único. A ordem é por áreas sucessivas, sem
  obrigar cada bloco a quebrar sozinho. Não abre automaticamente um túnel com
  as dimensões do personagem.
- Frente/fundo seguem os comandos atuais. A outra camada não usa silenciosamente
  o alvo selecionado. A busca verifica centros dentro de toolRadius, com teto
  de 128 blocos / 512 células para limitar o custo de alcances extremos.
- O primeiro alvo protegido interrompe o uso. A área assistida exclui tiles
  protegidos ou fora de alcance; dano, harvestLevel, desgaste, cadência, drops
  e validação do servidor continuam nas rotinas nativas.
- Materiais imunes ou resistentes demais não são ignorados automaticamente.
  O contorno não garante capacidade de quebrar todo material de mod.
- Sem alvo, não minera nem coleta líquidos automaticamente. Para líquidos,
  use livre. O botão não altera as opções existentes de atacar ao mirar.
- Mouse físico e gestos de interface mantêm mira livre. Janelas, chat, rodas,
  cinematics e pausa suspendem assistência. Cancelamento exige nova entrada.
- O estado é transitório; init o limpa nos dois tipos de ferramenta. Não muda
  o formato do save nem a descrição persistente do item.

## Destaque discreto

A prévia adicional usa contornos de 1 pixel, azul claro translúcido, sem
preenchimento verde. Vermelho indica impedimentos conhecidos: alcance,
proteção ou validação nativa de colocação. No mobile/launcher, o realce original
do BeamMiningTool é suprimido para evitar duplicação. Desktop clássico mantém
a prévia nativa. O desenho usa a camada selecionada, sem mostrar a parede do
fundo como alvo de uma ação de frente.

O estado da mineração aparece no botão, deixando de repetir uma legenda sobre
o terreno. Veja também [inventário, atalhos e indicadores](touch-inventory-shortcuts.md).

## Mods informados pelo usuário

A análise consultou código público, sem executar os arquivos `.pak` instalados:

- Frackin' Universe, commit `17868d86c9bb7bc00f92038fb78b48cebd1b41b8`:
  exemplos como carbonpickaxe, bloodstonepickaxe, obsidianpickaxe e hellhammer
  são `.miningtool`; manipuladores `.beamaxe` também recebem a assistência.
- O [Mining Laser do FU](https://github.com/sayterdarkwynd/FrackinUniverse/blob/17868d86c9bb7bc00f92038fb78b48cebd1b41b8/items/active/weapons/ranged/unique/miners/minertest/beamfire.lua)
  já traça um raio desde o cano, usa lineCollision e gera o projétil no impacto.
  Mantém seu comportamento próprio; Mine não troca a lógica desse laser.
- Os [lasers do Arcana](https://github.com/sleepySva/sva_arcana/blob/2b7bc3a0b1e32bd0dcb73e2e4042ff8f04f9c409/items/active/unsorted/laserminer/arcana_beammine.lua)
  usam lineCollision/lineTileCollisionPoint e damageTileArea no impacto.
  Área, energia, alcance e camada continuam definidos no script.
- Enhanced Storage, Shellguard, Voyage An Arcana Addon, FU Extraction Patch
  for Arcana (Fixed) e Patch Project foram identificados na captura; não houve
  auditoria completa desses mods ou patches.

Outros `.activeitem` personalizados precisam de análise individual. Não há
palpite por palavras como laser, weapon ou tool: o FU também contém uma arma
Phaser no diretório miners. As versões públicas podem diferir dos arquivos
instalados; não é garantia de compatibilidade universal.

## Validação e conferência

- Teste standalone C++17, O1, Wall/Wextra/Werror: **734 checks de seleção**
  aprovados, cobrindo direção, intensidade, sequência, alcance, entradas
  inválidas e custo limitado em 360 direções.
- Regressões de toque **8.354**, gamepad **31.695**, trava **148** e escala **102**
  aprovadas. Auditoria pura de toque: 20.000 quadros, zero alocações; não mede
  a engine inteira ou o custo visual.
- Etapa inicial: sintaxe Android arm64/API28/NDK r26d e objetos Windows launcher
  dos cinco C++ alterados; arquivos compartilhados também em Windows clássico.
- Ampliação de mods: nove checks de StarMainInterface, StarTools e
  StarItemDatabase em Android arm64, Windows launcher e desktop clássico.
- `git diff --check` aprovado. As árvores publicadas foram conferidas contra
  as verificadas. Comandos/logs locais ignorados: work/validation-directional-mining
  e work/validation-mining-mods.

Não houve link completo, APK, execução no aparelho/mods, medição de FPS,
compilação iOS/Switch/Linux/macOS ou execução de core_tests/game_tests.
Conferir no celular: livre/direcional, diagonais, movimento, zoom/GUI, bloco
único/upgrades de área, frente/fundo, proteção, ferramentas gastas/de mods,
mouse, menus, foco, troca de equipamento e trava de inimigos.

APK permanece no GitHub Actions: Release, arm64, system_allocator=true,
sem HWASan. Workflow, assinatura, compactação, versões e ABIs não mudaram.
