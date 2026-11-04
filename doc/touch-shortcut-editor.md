# Ícones de atalhos e edição de controles

## No jogo

Botões com a ação Quick bar shortcut mostram o ícone nativo do item da mão
principal na posição escolhida do grupo atual da barra. Quantidades maiores
que uma unidade aparecem abreviadas, como na roda de itens; slot vazio mostra
`--`. O rótulo personalizado fica abaixo do botão. O uso continua igual:
apenas equipar ou usar enquanto segura, conforme a configuração anterior.

Trocar o item, consumi-lo ou mudar de grupo atualiza o desenho na próxima
renderização. Os ícones de mods vêm dos drawables do próprio item, com camadas,
diretivas e tamanho ajustado para caber no botão. Não há cópia de pixels para
texturas ImGui nem lista de nomes de mods. Um mod sem ícone válido pode não
mostrar imagem; isso não cria ou escolhe um substituto.

O desenho usa o renderizador da engine depois do render do cliente, quando o
worker de simulação do Switch já foi aguardado. Não altera framebuffer,
projeção, shaders ou assinatura de itens. As posições seguem a mesma geometria
física do overlay, incluindo insets, conversão para resolução do jogo e eixo Y.
Opacidade individual/global e fade ocioso acompanham o botão; não é desenhado
em menus, janelas, cinematics ou com HUD oculto. A moldura não preenche/inverte
o centro por cima da imagem. Se o cliente não aceitar o desenho, o overlay
mantém o rótulo simples como fallback.

## Criar e editar no launcher

- **Add shortcut** cria imediatamente um botão habilitado, seleciona-o e abre
  a prévia. Não existe limite fixo de três botões. A área disponível da tela
  continua sendo o limite prático; os avisos de sobreposição existentes ajudam
  a posicioná-los.
- Na prévia, selecione/arraste o botão e ajuste tamanho, opacidade, habilitação,
  rótulo e ação. Atalhos oferecem posição de 1 a 10 da barra principal e a
  opção Use while held. O painel é rolável e ocupa no máximo 48% da altura.
- A prévia oferece **Save layout**. **Done** apenas fecha a prévia; mudanças
  não salvas ainda exigem Save no gerenciador, como antes.
- No gerenciador, **Duplicate button** copia ação, parâmetros, tamanho,
  opacidade e rótulo para um ID livre, com pequeno deslocamento da posição.
  O novo botão fica habilitado e selecionado para edição.
- **Delete...** só aparece para botões personalizados e pede confirmação.
  Controles embutidos podem ser ocultados por Enabled, preservando IDs e
  evitando que a mesclagem das configurações os recrie em outro lugar.
- **Undo layout switch** no gerenciador restaura o snapshot anterior inteiro,
  inclusive após criar, duplicar ou remover um botão. A restauração exata
  remove os botões recém-criados e recupera os removidos. Salve depois de
  desfazer para persistir essa escolha.

New Button, macros, layouts nomeados, presets e remapeamentos permanecem
disponíveis. As alterações só são aplicadas ao arquivo de configuração com
Save. Não há migração forçada de layouts, novos perfis por contexto ou mudança
de save, ABI, identidade, assinatura, compactação, dependências ou workflow.

## Validação executada

- Testes standalone C++17/O1/Wall/Wextra/Werror: **435 checks de UX** aprovados,
  incluindo 16 novos casos de alinhamento com quatro escalas, cantos da tela
  e insets. O helper testado é o usado para posicionar os ícones.
- Regressões: **734** mineração, **31.695** gamepad, **8.354** toque, **148** trava
  e **102** escala aprovadas. Auditoria pura de toque: 20.000 quadros/zero
  alocações, sem medir custo da engine, dos ícones ou do editor.
- **10 checks de produção aprovados**: StarMobileControls e StarMobilePlatform
  com sintaxe Android arm64/API28/NDK r26d e objetos Windows launcher;
  StarMainInterface e StarClientApplication também com objetos Windows
  clássico. Controles/launcher foram recompilados depois dos ajustes finais.
- `git diff --check` aprovado. Comandos/logs locais ignorados ficam em
  work/validation-shortcut-editor. Nenhum APK foi compilado no PC.

Não houve link completo, execução de UI no aparelho, validação dos mods
instalados, medição de FPS ou teste iOS/Switch/Linux/macOS/core_tests/game_tests.
A ordenação de render do Switch foi inspecionada; não representa build ou
execução nessa plataforma.

## Conferência manual

1. Criar mais de três atalhos, duplicar botão remapeado e conferir IDs distintos,
   parâmetros preservados, seleção, edição e arrasto. Cancelar e confirmar uma
   remoção, desfazer criação/duplicação/remoção e salvar/reiniciar.
2. Conferir os layouts antigos e o ciclo de layouts nomeados/presets. Apagar
   um botão personalizado não deve apagar itens nem bindings de gamepad.
3. Equipar comida, cura, ferramentas e itens de mods: conferir imagem, contagem,
   pilha acabando, slot vazio, mudança de grupo e troca do item no mesmo slot.
4. Testar janelas, foco, orientação, insets, resolução reduzida, escala da GUI,
   opacidade e fade. Não deve haver ícone atrás de menus ou desenho desalinhado
   com o círculo do botão.
5. Repetir seleção e uso segurado: soltar, abrir menu, morrer ou trocar o item
   devem manter os cancelamentos já existentes. A imagem não altera consumo,
   dano, energia ou escolha do item.

APK no GitHub Actions: branch deste PR, Release, arm64,
system_allocator=true, sem HWASan. Publicação para revisão, sem merge automático.
