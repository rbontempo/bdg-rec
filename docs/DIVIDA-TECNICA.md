# Dívida técnica conhecida

Itens levantados na auditoria de agosto/2026 e nos reviews da branch
`fix/criticos-auditoria` que foram **deliberadamente não corrigidos**. Todos os
críticos e importantes foram tratados; o que está aqui é o resto.

Cada item diz o que é, por que ficou de fora, e o que faria a decisão mudar.

---

## Adiado por decisão

### JUCE 8.0.4 → 8.0.15
`CMakeLists.txt`

Onze patches na mesma linha de API. A partir do 8.0.9, `createWriterFor()`
ganhou uma interface nova (`AudioFormatWriterOptions`); os overloads antigos
foram depreciados, não removidos, então o build passa com warnings.
`AudioEngine.cpp` chama isso em quatro pontos.

**Por que ficou:** o caminho de gravação inteiro foi reescrito nesta branch e
ainda não rodou em produção. Subir o framework junto mistura duas fontes de
risco — se aparecer regressão, não se sabe qual foi.

**Quando fazer:** depois que esta branch estiver em uso e estável. Ir para
8.0.15, não para o 9: o JUCE 9 traz implementação nova de CoreAudio no macOS,
código recente, num app cujo trabalho é gravar áudio.

---

## Corretos hoje, frágeis amanhã

### `uiAlive` aplicado de forma inconsistente
`MainComponent.cpp` — vários `callAsync` capturam `this` sem o guard

Não é explorável: o `MainComponent` só é destruído em `shutdown()`, depois que
o message loop parou, então lambdas pendentes nunca executam. Mas o padrão
está inconsistente com os sites que *receberam* o guard.

**Quando vira bug:** no dia em que o app tiver mais de uma janela, ou em
qualquer refactor que permita destruir o `MainComponent` com o loop vivo.

### `contributing == 0` retorna sem contabilizar
`AudioEngine.cpp`

Se todos os ponteiros de entrada vierem nulos, o bloco retorna sem somar nada
a `droppedSamples`. Deliberado: contar como "perdido" dispararia o aviso de
"o disco não acompanhou", que seria mentira. Na prática o CoreAudio não
entrega um callback assim.

### `Strings::currentLanguage` é estado global mutável em header
`Strings.h`

Funciona porque só a UI o toca, sempre no message thread. Ficaria melhor
dentro de um controlador, mas a troca de idioma já foi unificada em
`applyLanguageChange()`, que era o problema real.

### `salvageTruncatedChunk` assume 24-bit mono
`AudioEngine.cpp`

Não lê `bitsPerSample`/`numChannels` do `fmt ` antes de dividir por 3. Vale
para arquivos gerados pelo próprio app, que são sempre 24-bit mono. Um chunk
de outra origem viraria ruído decodificado.

**Quando fazer:** se o app algum dia gravar em outro formato.

### `UpdateChecker`: `appProps` sem null-check e timestamp de 32 bits
`UpdateChecker.cpp`

`forceCheck()` pode iniciar a thread sem `checkIfDue()` ter rodado, e aí
`appProps` seria nulo. Hoje o construtor do `MainComponent` sempre chama
`checkIfDue` antes, então não acontece — mas é granada de refactor. E
`lastUpdateCheck` é gravado como `int` de segundos, que estoura em 2038.

---

## Cosméticos e estruturais

### Menu do Apple não troca de idioma
`MainComponent.cpp`

"Sobre o BDG rec", "Buscar atualizações..." e "Idioma" vivem na cópia do
`PopupMenu` passada a `setMacMainMenu` no construtor. `applyLanguageChange()`
não reconstrói essa cópia, então ficam no idioma do startup até reiniciar.
Pré-existente. Corrigir chamando `setMacMainMenu` de novo com o menu
reconstruído — foi por causa desse congelamento que o toggle de analytics foi
parar no menu Ajuda.

### `MainComponent` ainda concentra papéis
`MainComponent.cpp`

Melhorou nesta branch: os diálogos saíram para `BdgDialog.h` e os quatro
caminhos de parada viraram um `recordingFinished`. Ainda faz layout,
persistência, `MenuBarModel`, ponte de analytics e recuperação de crash.

Extrações com retorno claro, se voltar a crescer: `SettingsStore`
(hoje `loadSettings` conhece a API interna de três painéis) e
`RecordingController` (validações e regra de espaço em disco, que ficariam
testáveis).

### `ToggleRow` com estado duplicado
`OutputPanel.h` / `.cpp`

`normalizeOn` no painel e `normalizeRow.value` no filho guardam o mesmo bool,
sincronizados à mão em quatro setters. Fonte clássica de dessincronização.
Uma fonte de verdade (o row) com getter no painel resolve. Além disso,
`ToggleRow::mouseUp` não checa `contains(e.getPosition())`, então arrastar
para fora e soltar ainda alterna — o `RecordButton` faz essa checagem.

### `DspOverlay::setCurrentStep` com proteção redundante
`DspOverlay.cpp`

Tem `shared_ptr<atomic<bool>>` + `callAsync` interno, mas o único chamador já
faz `callAsync`. A camada extra sugere que o método é seguro para chamar de
qualquer thread, o que não é — a mutação de `steps` não é protegida.
Simplificar e documentar "message thread only".

### Chave de API "ofuscada" com XOR
`AnalyticsReporter.cpp`

Os bytes e o algoritmo estão no repositório público. O comentário
`"prevents plain-text grep"` promete uma proteção que não existe. Tratar como
identificador público e proteger o servidor com rate-limit; o comentário
deveria dizer isso.

### Miudezas
- `HeaderBar.cpp`: `setSize()` no construtor é inútil — o pai chama `setBounds`.
- `AnalyticsReporter.cpp`: `juce::Random` local por chamada funciona, mas
  `juce::Random::getSystemRandom()` é o idiomático.
- `UnitTests.cpp`: uma asserção de `isNewerVersion` repete outra.

---

## Ferramentas não adotadas

**LTO/IPO em Release** — `CheckIPOSupported` + `CMAKE_INTERPROCEDURAL_OPTIMIZATION`.
Binário menor e mais rápido, custo de build maior.

**clang-tidy** — o `.clang-format` foi adicionado; o tidy não. Pegaria parte
desta lista automaticamente.

**Assinatura e notarização** — o app sai com assinatura ad-hoc. Notarizar
custa US$ 99/ano e elimina o atrito de instalação no macOS de vez. No Windows
não há saída barata: o Azure Artifact Signing não atende o Brasil.

---

## Achado que não se confirmou

**`.gitignore` listando arquivos rastreados** — não reproduz. O padrão é
`build-*/`, com barra, que só casa diretórios; `build-macos.sh` e
`build-windows.bat` nunca estiveram sendo ignorados. Verificado com
`git check-ignore -v`.
