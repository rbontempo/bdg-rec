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

---

## Cosméticos e estruturais

### `MainComponent` ainda concentra papéis
`MainComponent.cpp`

Melhorou nesta branch: os diálogos saíram para `BdgDialog.h` e os quatro
caminhos de parada viraram um `recordingFinished`. Ainda faz layout,
persistência, `MenuBarModel`, ponte de analytics e recuperação de crash.

Extrações com retorno claro, se voltar a crescer: `SettingsStore`
(hoje `loadSettings` conhece a API interna de três painéis) e
`RecordingController` (validações e regra de espaço em disco, que ficariam
testáveis).

---

## Ferramentas não adotadas

**LTO/IPO em Release** — `CheckIPOSupported` + `CMAKE_INTERPROCEDURAL_OPTIMIZATION`.
Binário menor e mais rápido, custo de build maior.

**clang-tidy** — o `.clang-format` foi adicionado; o tidy não.

**Assinatura e notarização** — o app sai com assinatura ad-hoc. Notarizar
custa US$ 99/ano e elimina o atrito de instalação no macOS de vez. No Windows
não há saída barata: o Azure Artifact Signing não atende o Brasil.

---

## Achado que não se confirmou

**`.gitignore` listando arquivos rastreados** — não reproduz. O padrão é
`build-*/`, com barra, que só casa diretórios; `build-macos.sh` e
`build-windows.bat` nunca estiveram sendo ignorados. Verificado com
`git check-ignore -v`.

---

## Corrigido depois desta lista ter sido escrita

Ficam registrados porque a lista original os classificava como aceitáveis, e
o uso real mostrou o contrário:

- **`InlineWarning` com countdown residual** — era "cosmético" até derrubar o
  aviso de amostras perdidas, que precisa ficar na tela.
- **Permissão pedida várias vezes no startup**, para a pasta e para o
  microfone — não estava nesta lista porque nenhuma auditoria de leitura o
  encontrou. Só apareceu abrindo o app com o TCC limpo. A pasta era sondada
  em quatro pontos independentes; o microfone, em três (o `InputPanel` chegava
  a enumerar dispositivos antes de o engine existir). Medido com contadores:
  microfone foi de 3 acessos para 1.
- **Waveform em escala linear** e **`0` da régua cortado** — mesma causa raiz
  (amplitude linear onde precisava ser dB), em dois lugares. A escala agora
  vive em `src/LevelScale.h`, compartilhada, e há teste de render que reprova
  se voltar.
- `uiAlive` inconsistente, `UpdateChecker` (null-check e Y2038), menu do Apple
  sem troca de idioma, `ToggleRow` com estado duplicado, `DspOverlay` com
  proteção redundante, comentário enganoso da chave XOR e as miudezas.
