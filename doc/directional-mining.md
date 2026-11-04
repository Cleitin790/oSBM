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

- Implementa??o inicial: BeamMiningTool, incluindo o Manipulador de Mat?ria.
  Picaretas SwingableItem, ferramentas Lua personalizadas, armas, constru??o e
  trava de inimigos n?o recebem sele??o de minera??o. N?o abre automaticamente
  um t?nel do tamanho do personagem.
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
