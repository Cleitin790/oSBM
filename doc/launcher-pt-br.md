# Launcher em português do Brasil

O launcher oferece **Português (Brasil)** em Configurações → Idioma e usa
`pt_BR` como padrão para configurações novas. Configurações antigas com idioma
explícito são respeitadas; selecione Português (Brasil) e Salvar para mudar.
Padrão do sistema continua seguindo o aparelho. A variante `pt` sem região
passa a resolver para `pt_BR`; outras regiões explícitas não são sobrescritas.

O catálogo contém **389 entradas**, cobrindo o catálogo inglês inteiro e os
textos recentes de atalhos, edição de layout, trava de alvo, desempenho,
armazenamento, mods, saves, erros e estados do launcher. Inglês continua sendo
o fallback técnico e recebeu as chaves que antes só existiam no código.

Textos diretos dos widgets novos passaram para o sistema de tradução. Os
nomes de ações e elementos produzidos por launcherTextStatic recebem o mesmo
catálogo ativo do launcher, com cópia sincronizada ao trocar o idioma. Não há
leitura de arquivos de idioma por quadro. O seletor de comportamento de botão
agora guarda Strings próprias, evitando ponteiros para temporários traduzidos.

Rótulos personalizados salvos, nomes de mods/itens, arquivos como packed.pak,
teclas, botões e nomes internos de imagens de controles permanecem iguais.
Isso não traduz as conversas ou os assets do Starbound e de seus mods, nem as
mensagens técnicas internas dos componentes externos. Seletores nativos do
sistema operacional continuam usando o idioma definido pelo próprio sistema.

O arquivo usa UTF-8 e os acentos portugueses ficam no intervalo latino padrão
das fontes ImGui. Não houve troca de fonte, dependência, assinatura, versão,
ABI, formato de save, lógica de controle ou workflow. Os caminhos de embalagem
existentes copiam a pasta lang inteira para os destinos de cada plataforma.

## Verificação

- `python scripts/check_launcher_pt_br.py`: aprovado; **389 entradas PT-BR e
  389 inglesas**, **494 referências no código**, sem chaves faltando, duplicadas,
  vazias ou incompatibilidades de placeholders. Também verifica rótulos ImGui
  diretos nos dois arquivos de interface.
- Correção posterior à execução Actions `41859437116`: quatro mensagens dos
  atalhos agora usam `TextWrapped("%s", texto)` / `TextDisabled("%s", texto)`.
  Antes, o texto traduzido era passado diretamente como formato e o Android
  rejeitava a compilação com `-Werror=format-security`. Os quatro erros foram
  reproduzidos com NDK r26d; o arquivo corrigido passou em sintaxe Android
  arm64 e compilação de objeto Windows launcher com `-Wformat` e
  `-Werror=format-security`. O script de tradução também rejeita essas chamadas
  incorretas (falhou antes da correção e passou depois). Isso não substitui
  uma nova geração completa do APK no GitHub ou teste no aparelho.
- Seis checks pontuais: StarMobilePlatform.cpp, StarMobileControls.cpp e
  StarMobileLauncherSupport.cpp em sintaxe Android arm64/API28/NDK r26d e
  objetos Windows launcher. Todos aprovados após corrigir a composição de um
  rótulo dinâmico; controles recompilados na forma final.
- `git diff --check`: aprovado. Comandos/logs locais ignorados ficam em
  work/validation-launcher-pt-br. Não foi gerado APK no PC.

Não houve link completo, execução de UI ou APK, teste no celular, medição de
layout/truncamento, iOS/Switch/Linux/macOS ou runners completos. Conferir no
aparelho as telas de configurações, controle físico, toque/prévia, macros,
mods e saves, incluindo modais, teclado e mensagens de importação/exportação.
Trocar PT-BR → Inglês → PT-BR, salvar e reiniciar; os remapeamentos e rótulos
personalizados devem permanecer iguais. Traduções longas podem precisar de
ajuste visual depois dessa conferência.

Build de teste no GitHub Actions: branch deste PR, Release, arm64,
system_allocator=true, sem HWASan. PR separado para revisão, sem mescla automática.
