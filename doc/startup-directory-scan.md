# Reduzir trabalho na inicialização: varredura de pastas

DirectoryAssetSource consultava String::regexMatch para cada combinação de
arquivo e regra de exclusão. Cada chamada recompilava uma expressão RE2.
Há dez regras padrão de pathIgnore e duas regras de metadata adicionadas pela
própria fonte; em pastas com muitos arquivos isso repete a mesma compilação.

A varredura agora mantém um cache local de expressões já alcançadas. Cada
regra é compilada no máximo uma vez por construção da fonte, sendo reutilizada
pelas subpastas. O cache é destruído ao fim da varredura: não cresce durante
o jogo nem persiste entre execuções ou versões de mods.

São preservados RE2, correspondência parcial sem distinguir maiúsculas,
ordem e curto-circuito das regras, mensagem de erro e validação preguiçosa.
Uma expressão inválida nunca alcançada continua sem causar erro; uma expressão
inválida alcançada continua interrompendo o carregamento. Metadata, ordenação
dos caminhos e leitura dos arquivos permanecem iguais.

## Alcance e limites

Esta alteração beneficia fontes em diretórios, incluindo os assets extraídos
do aplicativo e mods descompactados. Não muda o índice ou a leitura dos mods
`.pak`, não reduz o trabalho dos scripts do FU/Arcana e não acelera por si só
a geração de planetas. Também não altera digest/preload, validação de assets,
workers, limites de memória, caches do mundo, saves, assinatura ou workflow.

O launcher Android já pula digest/preload e a leitura do índice dos `.pak` já
usa leituras em bloco. Root::fullyLoad usa um worker nos aparelhos móveis;
esse limite não foi aumentado sem um teste de estabilidade e memória.

A redução de trabalho é de compilações por arquivo/regra para, no máximo,
uma compilação por regra alcançada em cada fonte. Isso é uma propriedade do
código; não é um benchmark de segundos ou uma porcentagem de aceleração.
Não há medição de inicialização ou entrada no mundo no aparelho nesta etapa.

## Verificação

- Novos cenários em AssetsTest.DirectoryIgnorePatternsKeepSemantics verificam
  pasta vazia, subpastas, nomes com extensões maiúsculas, exclusão de metadata,
  leitura do arquivo preservado e regex inválida alcançada/não alcançada.
- Nove checks pontuais aprovados: StarDirectoryAssetSource.cpp, StarAssets.cpp
  e assets_test.cpp com sintaxe Android arm64/API28/NDK r26d e objetos Windows
  launcher e desktop clássico. Usaram os headers RE2 reais já instalados pelo
  projeto. Uma primeira tentativa sem o diretório desses headers falhou; o
  comando foi corrigido e todos os checks passaram.
- O novo teste foi compilado, mas não executado: não foram linkados os runners
  completos core_tests/game_tests ou um aplicativo. Comandos/logs locais
  ignorados ficam em work/validation-startup. `git diff --check` aprovado.

Não houve APK, runtime Android/iOS/Switch/Linux/macOS, benchmark, teste de mods
reais ou comparação antes/depois de carregamentos. Os logs diagnósticos antigos
do HWASan não são uma referência válida de tempo para o Release atual.

## Comparação no aparelho

Usar builds Release/arm64 com system_allocator=true e sem HWASan no GitHub
Actions. Manter os mesmos mods, configuração e mundo nos dois builds. Comparar
separadamente abrir até o menu e selecionar personagem até o mundo; separar
primeira abertura após instalar o APK das aberturas seguintes. Repetir cada
caso e guardar os logs de ambas as versões. Só esses resultados podem mostrar
quanto essa redução representa no carregamento total e indicar o próximo
gargalo a tratar.
