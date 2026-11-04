# Minera??o livre e direcional

O bot?o **Mine** no overlay alterna **Mining: free** / **Mining: directional**.
A mensagem confirma a escolha; com um alvo aparece tamb?m Directional mining.
Come?a livre a cada sess?o de jogo. A escolha ? manual e ?nica, sem perfis por
item/contexto. Posi??o/tamanho/r?tulo e desativa??o do bot?o s?o edit?veis como
os demais controles. Comfortable tamb?m fornece uma posi??o para ele; layouts
salvos n?o s?o movidos. Gamepad pode remapear a a??o Free / directional mining;
nenhum bot?o, incluindo R3, ? substitu?do.

No modo direcional, use o joystick de mira e o comando de minera??o habitual.
A dire??o seleciona a primeira c?lula ocupada por material, do jogador para
fora, na camada ativa. N?o ? preciso manter o cursor sobre cada bloco. Ap?s a
remo??o, a sele??o avan?a. O mesmo tra?ado permanece no mesmo primeiro bloco
at? ele desaparecer; mudan?as de dire??o deliberadas mudam o alvo.

- Ferramentas nativas BeamMiningTool e MiningTool, incluindo itens de mods
  carregados como `.beamaxe` e `.miningtool`, recebem a mesma assist?ncia.
  N?o depende do nome do item, tradu??o ou de uma lista de mods.
  Ferramentas `.activeitem` com minera??o Lua personalizada, armas, constru??o
  e trava de inimigos n?o recebem sele??o de minera??o por adivinha??o de nome
  ou tags. Precisam de an?lise do mod para integra??o segura.
  N?o abre automaticamente um t?nel do tamanho do personagem.
- Preserva ?rea normal e bloco ?nico. Alcance por centro de tile usa toolRadius;
  a busca n?o ultrapassa 128 blocos / 512 c?lulas, mesmo com mods de alcance.
- Frente/fundo seguem os comandos j? existentes. Uma a??o simult?nea da outra
  camada n?o usa o alvo da camada selecionada. A assist?ncia n?o troca de camada.
- Prote??o no primeiro alvo interrompe o uso. Tiles protegidos/fora de alcance
  s?o exclu?dos da ?rea assistida. Dano, harvestLevel, cad?ncia, drops e valida??o
  do servidor continuam nas rotinas nativas. N?o h? garantia de quebra para
  todo material de mod: imune/harvestLevel insuficiente permanece bloqueando.
- A ?rea da ferramenta continua podendo atingir v?rios blocos de uma vez. A
  ordem ? por ?reas sucessivas, n?o uma promessa de remover cada tile sozinho.
- Sem alvo n?o minera nem coleta l?quidos automaticamente. Para l?quidos,
  use livre. Atacar ao mirar s? ocorre se a configura??o atual j? o habilita;
  o bot?o Mine n?o altera essas op??es nem mant?m ataques ao soltar.
- Mouse f?sico e gestos de interface mant?m mira livre. Menus, chat, rodas,
  cinematics e pausa suspendem assist?ncia. Cancelamento de input exige nova
  entrada para retom?-la. N?o h? altera??o no save ou descri??o do item.

## Destaque discreto

A pr?via adicional usa contornos de 1 pixel, azul claro transl?cido, sem
preenchimento verde. Vermelho indica impedimentos conhecidos (alcance/prote??o
ou valida??o nativa de coloca??o). O contorno n?o promete que uma ferramenta
consiga vencer toda resist?ncia especial de mod. No caminho mobile/launcher,
o realce original do BeamMiningTool ? suprimido para n?o duplicar o contorno.
Desktop cl?ssico mant?m sua pr?via nativa. A pr?via de minera??o usa a camada
selecionada, sem mostrar a parede do fundo quando a a??o ? na frente.

## Confer?ncia no aparelho

1. Livre: conferir controles antigos, l?quidos, mouse/toque e armas.
2. Mine: apontar o anal?gico at? o m?ximo contra uma parede; segurar uso e
   conferir avan?o, diagonais, vazio, alcance e manter primeiro alvo.
3. Alternar Mine durante uso; conferir mensagem e retorno imediato ? mira livre.
4. Conferir bloco ?nico, upgrades de ?rea, frente/fundo e ambos os gatilhos.
5. Testar prote??o, material resistente e alcance de mods: nunca ignorar regras
   de dano. Conferir edi??o e persist?ncia do layout e remapeamento do gamepad.
6. Conferir pausa, foco, invent?rio, troca de ferramenta e trava de inimigos.
   Com picareta/broca `.miningtool`, conferir dano, desgaste/durabilidade e
   cad?ncia normais, inclusive item gasto; repetir com um item de mod de cada
   tipo. Alternar entre ferramenta compat?vel e `.activeitem`/arma deve deixar
   os ?ltimos com a mira e fun??es originais.
7. Conferir visual em cen?rio claro/escuro e com zoom/GUI diferentes.

APK deve ser gerado no GitHub: Release, arm64, system_allocator=true, sem
HWASan. Nenhuma mudan?a no workflow, assinatura, compacta??o ou vers?es.

## Valida??o desta altera??o

Comandos executados no host Windows, sem gerar APK:

- `zig c++ -std=c++17 -O1 -Wall -Wextra -Werror -I source/core source/test/directional_mining_test.cpp -o <tempor?rio>` e execu??o: **734 checks aprovados**. Exercita intensidade independente, sequ?ncia ap?s remo??o, primeiro obst?culo, eixos/diagonais, centro/limite de alcance, entradas inv?lidas e custo limitado em 360 dire??es.
- Mesmo comando para gamepad_input_test, touch_input_test e target_lock_test:
  **31.695**, **8.354** e **148 checks aprovados**. Auditoria pura de toque:
  **20.000 quadros / zero aloca??es**, sem medir a engine ou o novo desenho.
- interface_scale_test com include de source/windowing: **102 checks aprovados**.
- Matriz pontual com headers reais: StarMobileControls.cpp e
  StarMobilePlatform.cpp; Android arm64/API 28/NDK r26d (`-fsyntax-only`) e
  Windows launcher/Zig C++17 (`-c`). Ambos aprovados.
- StarMainInterface.cpp, StarTools.cpp e StarClientApplication.cpp:
  Android arm64/API 28 (`-fsyntax-only`), Windows launcher e desktop cl?ssico
  (`-c`). Valida??o final ap?s ajustes de camada/cancelamento registrada nos
  logs locais ignorados work/validation-directional-mining.
- `git diff --check`: aprovado. Workflow/Gradle/assinatura e op??es Android
  permanecem iguais ? main de base f48d54d.

A matriz n?o ? link do jogo nem build de APK. Nenhuma execu??o no aparelho,
medi??o de FPS, compila??o iOS/Switch/Linux/macOS ou execu??o dos runners
core_tests/game_tests foi realizada. Conforto, legibilidade, sequenciamento em
multiplayer e ferramentas/mods reais ainda precisam da confer?ncia acima.

## Compatibilidade com os mods informados

A amplia??o usa a capacidade nativa TileMiningTool em MiningTool e
BeamMiningTool. ItemDatabase carrega `.miningtool` e `.beamaxe` de qualquer
mod por esses tipos: n?o depende de nome traduzido, itemTags ou uma lista de
modifica??es. A sele??o direcional e o filtro de ?rea s?o compartilhados.
MiningTool continua calculando tileDamage/tileDamageBlunted, harvestLevel,
durabilityPerUse, sons e cad?ncia pelo c?digo anterior; o ramo livre mant?m a
?rea anterior. init limpa o estado de assist?ncia nos dois tipos.

C?digo p?blico consultado, sem executar ou redistribuir os assets dos mods:

- Frackin' Universe `17868d86c9bb7bc00f92038fb78b48cebd1b41b8`: inclui, por exemplo,
  `items/tools/miningtools/carbonpickaxe.miningtool`,
  `bloodstonepickaxe.miningtool`, `obsidianpickaxe.miningtool`,
  `hellhammer.miningtool` e manipuladores `.beamaxe`. Esses tipos recebem a
  amplia??o sem editar os assets do mod.
- O [Mining Laser do FU](https://github.com/sayterdarkwynd/FrackinUniverse/blob/17868d86c9bb7bc00f92038fb78b48cebd1b41b8/items/active/weapons/ranged/unique/miners/minertest/beamfire.lua)
  j? calcula um raio desde a posi??o do cano, usa lineCollision e gera o
  proj?til de minera??o no impacto. N?o recebe sele??o adicional da engine;
  seu funcionamento pr?prio por dire??o fica preservado.
- Os [lasers do Arcana](https://github.com/sleepySva/sva_arcana/blob/2b7bc3a0b1e32bd0dcb73e2e4042ff8f04f9c409/items/active/unsorted/laserminer/arcana_beammine.lua)
  usam lineCollision/lineTileCollisionPoint e damageTileArea no impacto, com
  ?rea, energia, alcance e camada definidos no script. Tamb?m permanecem com
  seu comportamento original. O bot?o Mine n?o altera esse script nem cria
  um contorno de ?rea que possa divergir dele.
- Enhanced Storage, Shellguard, Voyage An Arcana Addon, FU Extraction Patch
  for Arcana (Fixed) e Patch Project foram identificados pelo nome na captura;
  n?o houve auditoria completa desses mods nem de seus patches.

A consulta usou o c?digo p?blico dos projetos, n?o os `.pak` do aparelho; as
vers?es instaladas e patches podem diferir. N?o ? garantia de compatibilidade
com toda ferramenta ou com todos os mods. N?o foi aplicada detec??o gen?rica
por palavras como laser/weapon/tool: FU tamb?m tem uma arma Phaser no
mesmo diret?rio de miners, o que faria esse tipo de palpite afetar combate.

Valida??o da amplia??o: as mesmas **734 / 31.695 / 8.354 / 148 / 102**
verifica??es de sele??o/gamepad/toque/trava/escala passaram novamente.
StarMainInterface.cpp, StarTools.cpp e StarItemDatabase.cpp passaram em
**nove checks**: sintaxe Android arm64/API28/NDK r26d e compila??o de objetos
Windows launcher e desktop cl?ssico, C++17. `git diff --check` passou.
Logs locais ignorados: work/validation-mining-mods. Sem link, APK ou teste dos
mods/aparelho; iOS/Switch/Linux/macOS e runners completos n?o foram validados.
