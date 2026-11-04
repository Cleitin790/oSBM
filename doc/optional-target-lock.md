# Trava automática de alvo opcional

No launcher, em Controles/Overlay, ative **Ativar trava automática de alvo** e
salve. A opção permanece desligada por padrão e usa a mesma configuração
`touch.lockOnEnabled` existente. Quando habilitada, não é necessário pressionar
Travar para cada inimigo: equipar uma arma elegível permite adquirir um alvo
hostil visível automaticamente. No Android isso já funciona ao entrar no mundo;
no desktop launcher é necessário usar toque/gamepad. Mouse físico e teclado
suspendem a assistência até uma nova entrada de toque/gamepad.

## Comportamento

- A aquisição automática considera inimigos próximos em toda a tela, incluindo
  atrás do personagem. Distância tem prioridade entre alvos comuns; alinhamento
  da mira só acrescenta uma preferência pequena. Empates usam EntityId.
- Alvo morto, fora da tela/alcance ou atrás de parede é liberado. Sem alvo, a
  busca se repete no máximo quatro vezes por segundo e encontra outro sem botão.
- Uma criatura mantida não é trocada por outro inimigo comum mais próximo.
  O marcador laranja e a mira acompanham o mesmo objeto/EntityId válido.
- **Preferir chefes** usa a barra especial nativa. Se um chefe reconhecido
  aparecer enquanto a mira acompanha um inimigo comum, ocorre uma busca de
  promoção no máximo a cada 750 ms. Depois de travar no chefe, os minions não
  roubam o foco. Desligar a preferência remove essa promoção/prioridade.
- O botão de trava passa a **pausar/retomar a aquisição automática** durante
  a sessão. Pausar libera o alvo e não o readquire no quadro seguinte.
  **Próximo** ainda escolhe outro alvo explicitamente e retoma a assistência;
  essa escolha é respeitada enquanto válida, mesmo se houver um chefe próximo.
  Nada foi remapeado para R3 nem para outro botão já utilizado do gamepad.
- Inventário, chat, pausa, roda radial, cursor com item, cinematic e perda de
  foco suspendem a trava. Ao voltar ao jogo, a busca retoma sem pressionar
  Travar novamente. Ela também é atualizada antes de calcular a mira do jogador,
  evitando um quadro de mira antiga ao trocar para uma ferramenta.
- São exigidos alvo vivo, visível, sem obstáculo, tipo Monster/Npc, time Enemy,
  comportamento agressivo (ou barra especial) e permissão de dano entre times.
  Aliados, pets, neutros e jogadores de PvP continuam excluídos.
- Alcance de seleção: 8–80 blocos, padrão 40. Isso não altera alcance/dano da
  arma, física de projéteis, energia ou chance de acertar. Não existe predição
  de movimento e **a trava não dispara sozinha**. O gatilho/botão/gesto normal
  de ataque continua necessário.

## Ferramentas e armas de mods

A elegibilidade segue a tag nativa `weapon`, com exclusão adicional de
`TileMiningTool` e `MaterialItem`. Uma ferramenta/bloco na mão principal
mantém a mira livre, mesmo com arma na outra mão; atirar explicitamente com a
arma secundária permite assistência para essa ação. Usar uma mão sem arma
suspende a trava enquanto o botão dessa mão estiver segurado.
O ataque alternativo de uma arma de duas mãos é tratado como uso dessa mesma
arma, sem exigir um segundo item equipado.

Não há lista fechada de nomes de bosses, palpite baseado em HP, nem detecção
de ferramentas por nome de arquivo. A preferência usa
`DamageBarEntity::damageBar() == DamageBarType::Special`; bosses novos que
exponham esse padrão recebem prioridade. Sem essa marca, podem entrar como
inimigos comuns se passarem nos mesmos filtros. Uma troca de fase que recria
a entidade é tratada como uma aquisição nova, nunca como transferência cega
de um ID antigo. Projéteis, objetos ou entidades invisíveis não são alvos.

Armas sem a tag `weapon` ficam fora da assistência. Um ActiveItem de mod
marcado como arma, mas usado como minerador por scripts próprios, não pode
ser identificado genericamente pela engine; pause/desative a trava nesse caso.
Não foram fornecidos/testados os .pak reais dos mods do usuário nesta etapa.

## Validação e teste

`target_lock_tests` continua sendo NoAssets. Os testes cobrem seleção automática
sem cone, proximidade, prioridade opcional de chefes, mãos com arma/ferramenta,
pausa/retomada da busca, limites de frequência, manutenção de alvo sem nova
consulta, desempates/ciclo e todos os filtros de retenção existentes.
Testes unitários e compilações pontuais não substituem teste no aparelho.

1. Opção desligada: confira toque, gamepad, mouse e ferramentas como antes.
2. Ative/salve e equipe arma: aproxime-se de inimigos sem apertar Travar.
   Mate o alvo e confirme a aquisição seguinte; teste um inimigo atrás do jogador.
3. Trave um chefe reconhecido e espere minions: o foco deve ficar no chefe.
   Deixe um chefe entrar depois de um inimigo comum; teste preferência ligada
   e desligada e uma escolha explícita com Próximo.
4. Parede, limite de alcance, fuga da tela, morte e troca de fase: o alvo
   antigo deve ser liberado; outro alvo elegível pode ser adquirido sozinho.
5. Pause a trava pelo botão, espere perto de inimigos e confirme mira livre.
   Retome, abra inventário/chat/roda, perca foco e volte: não pode disparar sozinho.
6. Teste ferramenta na principal/arma na secundária, bloco, minerador nativo,
   arma de mod, mouse físico e retorno ao gamepad. A mineração deve ficar livre.
7. Meça FPS em Release com os mesmos mods/configuração; a busca é periódica,
   nunca uma consulta global de inimigos por quadro.
