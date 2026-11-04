# Inventário, atalhos e indicadores de toque

## Inventário

No caminho mobile/desktop launcher, o botão **Ações**, na lateral do inventário,
abre as opções abaixo. O painel começa recolhido e não ocupa a hotbar:

- **Mover:** toque para pegar/colocar e arraste para transferir, como antes.
- **Info:** toque para mostrar o tooltip nativo sem pegar nem usar o item.
- **1 item:** toque para o clique direito nativo, retirando/depositando uma
  unidade quando permitido pelos slots e pelas pilhas.
- **Metade:** com cursor vazio, retira meia pilha pelo Shift+clique direito
  nativo e volta a Mover; o próximo toque coloca a pilha separada no destino.

Segurar um slot por 450 ms agora mostra detalhes sem dividir a pilha. Este
gesto substitui a divisão implícita por clique direito. Após a inspeção,
arrastar ou soltar esse contato não move o item. Dois dedos para transferência
rápida continuam disponíveis em Mover. Soltar arrasto fora de um slot mantém
o item no cursor, sem descarte no mundo. Mouse e navegação por gamepad seguem
seus eventos originais.

Os alvos de toque dos slots recebem 15% de margem adicional sobre o mínimo
já existente, sem ampliar os desenhos dos slots ou a janela inteira. O alvo
visual tem prioridade sobre as margens dos vizinhos; os alvos ficam dentro da
janela, respeitam recortes de rolagem e enviam cliques ao centro da parte
visível. As ações usam uma coluna à direita, dentro da altura original da
janela; nenhum botão é acrescentado acima dela. Pegar um item recolhe essa
coluna e retorna para Mover, deixando a hotbar livre como destino do arrasto.
Fechar as ações e reabrir o inventário também retorna para Mover.
Modos de divisão ficam indisponíveis com um augment
no cursor, evitando ativar a aplicação de augment pelo clique direito.

Esses gestos reutilizam as rotinas dos inventários/baús: não criam cópias de
pilhas nem alteram diretamente contagens. Skins e callbacks de mods ainda
podem definir ações próprias; precisam da conferência no jogo.

## Atalhos Q1 / Q2 / Q3

Os três novos botões começam desativados. No launcher, em Controls:

1. Encontre Q1, Q2 ou Q3, habilite o botão e escolha **Quick bar shortcut**.
2. Escolha **Bar slot (main hand)**, de 1 a 10. O atalho acompanha essa posição
   no grupo atual da barra rápida. Coloque o item desejado na posição da mão
   principal/esquerda (L) desse slot no jogo.
3. Desmarcado **Use while held**: o toque apenas equipa o item. Marcado:
   equipa e usa a mão principal enquanto segura; soltar encerra o uso.
4. Renomeie, por exemplo, para Cura, Comida ou Luz, ajuste a posição e salve.
   Comfortable fornece posições alternativas; não habilita os botões sozinho.

O item continua equipado depois de soltar. Um toque liberado antes do próximo
update apenas equipa; para usar, mantenha pressionado. Não há temporizador de
auto consumo nem restauração automática do equipamento anterior. Consumíveis
mantêm bloqueios de efeitos, alimentação e quantidade; armas/ferramentas
mantêm energia, carga e cadência. Scripts de mods seguem seu ciclo normal de
uso. Um slot vazio informa que está vazio, sem escolher outro item sozinho.

Não inicia durante ataques manuais ativos, com item no cursor, menus, chat,
rodas, pausa ou cinematics. Soltura, foco perdido, cancelamento, mudança do
item selecionado, morte/saída do mundo e fim da pilha encerram o uso. Novo ataque manual assume o
comando e cancela o atalho. O atalho não retoma sozinho depois disso. A mesma
ação está disponível para remapeamento de gamepad, sem substituir bindings.

## Indicadores

O botão de mineração mostra **LIVRE / DIR** e fica marcado quando ligado.
Ele lê o estado real do jogo, incluindo mudanças pelo gamepad. O botão de
troca da mão de mira mostra **MAO L / MAO R** conforme a mão de ataque escolhida
para o joystick de toque. Rótulos personalizados são preservados; as marcas
ficam abaixo deles e acompanham a opacidade do controle. A legenda repetida
sobre o terreno foi removida, mantendo os contornos discretos.

## Conferência no aparelho

1. Inventário e baú: pegar, colocar, rolar listas, arrastar, inspecionar por
   toque/pressão longa, cancelar, transferir com dois dedos e soltar fora.
2. Pilhas pares/ímpares, cursor vazio/ocupado, pilha máxima, slots vazios,
   equipamento, lixo e augment. Metade deve voltar a Mover após uma divisão.
3. Q1 com cura, Q2 com comida e Q3 com luz/ferramenta. Testar seleção simples,
   uso segurado, toque rápido, pilha acabando e slot vazio; conferir contagens.
4. Soltar durante carga/animação, abrir inventário/chat/roda/pausa, mudar grupo
   da barra, desconectar controle e alternar aplicativos. Nenhuma ação deve
   ficar presa ou retomar sem nova pressão.
5. Ativar Mine por toque e gamepad; conferir LIVRE/DIR. Trocar a mão e conferir
   MAO L/R com ambos os ataques e com as opções de ataque do joystick.
6. Conferir tamanho da GUI/zoom, opacidade, recortes, orientação e os layouts
   antigos. Testar Enhanced Storage e os itens dos mods realmente instalados.

Nenhum formato de save, assinatura, ABI, versão de ferramenta, compactação ou
workflow foi alterado. APK continua no GitHub Actions: Release, arm64,
system_allocator=true, sem HWASan.

## Verificações executadas

- `zig c++ -std=c++17 -O1 -Wall -Wextra -Werror -I source/core source/test/touch_ux_test.cpp -o <temporário>` e execução: **419 checks aprovados**. Exercita políticas de inspeção/divisão/arrasto e o estado de uso segurado, inclusive soltura antes do primeiro quadro, cancelamentos repetidos e proibição de retomada automática.
- Regressões executadas novamente: seleção de mineração **734**, gamepad
  **31.695**, toque **8.354**, trava **148**, escala **102**. A auditoria de
  lógica pura de toque registrou **20.000 quadros / zero alocações**; isso não
  mede o desenho, as ações novas ou o desempenho da engine inteira.
- StarMobileControls.cpp e StarMobilePlatform.cpp: sintaxe Android
  arm64/API28/NDK r26d e objetos Windows launcher aprovados.
- StarMainInterface.cpp, StarInventory.cpp, StarPaneManager.cpp e
  StarClientApplication.cpp: sintaxe Android arm64 e objetos Windows launcher
  e desktop clássico aprovados. Foram **16 checks iniciais**; os arquivos
  afetados foram recompilados depois dos ajustes finais. Comandos e logs
  locais ignorados ficam em work/validation-touch-ux.
- `git diff --check` aprovado. As verificações foram executadas no host
  Windows, sem link ou APK; não representam execução de interface real.

Não houve teste no aparelho, nos mods instalados, multiplayer ou medição de
FPS. iOS/Switch/Linux/macOS e os runners completos core_tests/game_tests não
foram executados. Legibilidade, altura da faixa nas skins de inventário,
persistência dos atalhos e cancelamento com SDL real precisam da conferência
no aparelho antes de afirmar usabilidade ou estabilidade finais.
