# Testes

```bash
cmake -B build -DBDG_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Os testes são opt-in (`BDG_BUILD_TESTS=OFF` por padrão), então um build normal
não é afetado.

## Dois grupos

**`unit`** — não precisam de placa de áudio nem de tela. São os que o CI roda.

```bash
ctest --test-dir build -L unit
```

- comparação de versões do `UpdateChecker` (inclui `1.1.10 > 1.1.9`, que
  falharia numa comparação de string)
- integridade das duas tabelas de idioma: nenhum campo vazio, e os tokens
  `%NEW%`/`%CUR%` preservados
- DSP: normalize (teto de ganho, limiter), noiseReduce nos dois caminhos de
  taxa, compressor e de-esser
- render: desenha o VU meter e o waveform fora da tela e inspeciona os pixels

**`device`** — gravam alguns segundos do microfone padrão. Só rodam numa
máquina com entrada de áudio e permissão concedida; um runner de CI não tem
microfone.

```bash
ctest --test-dir build -L device
```

| Teste | O que garante |
|---|---|
| `RotationTest` | a rotação de chunk não perde amostras nas fronteiras |
| `SampleRateTest` | mudança de taxa no meio da gravação encerra e salva na taxa correta |
| `EmptyTakeTest` | gravação vazia não é reportada como falha de disco |
| `RecoveryTest` | chunk com header zerado (assinatura de crash) é recuperado |

Os testes de dispositivo usam `BDG_CHUNK_SECONDS=2.0` para exercitar em
segundos um caminho que em produção leva 5 minutos por fronteira.

## Por que existe um teste de render

Três rodadas de code review leram o `VuMeter` e o `WaveformDisplay` e não
pegaram dois defeitos reais: o waveform desenhava amplitude linear (fala
normal virava uma linha pontilhada de 2px) e o "0" da régua era cortado pela
borda. Leitura pega lógica; escala e layout só aparecem olhando a saída.

O `RenderTest` desenha os componentes num `juce::Image` e mede os pixels.
Verificado que ele reprova quando os dois bugs são reintroduzidos.

## DiskFullTest

Fica fora do `ctest` porque precisa de um volume pequeno preparado à mão.
Ele verifica o comportamento mais crítico do app: com o disco cheio, os
chunks são preservados em vez de apagados.

```bash
hdiutil create -size 3m -fs HFS+ -volname BDGTEST -quiet /tmp/bdgtest.dmg
hdiutil attach /tmp/bdgtest.dmg -quiet

# deixa ~1 MB livre: cabem os chunks, não cabe uma segunda cópia
FREE_KB=$(df -k /Volumes/BDGTEST | tail -1 | awk '{print $4}')
dd if=/dev/zero of=/Volumes/BDGTEST/ballast.bin bs=1024 count=$((FREE_KB - 1000)) 2>/dev/null

./build/tests/BDG_DiskFullTest_artefacts/Debug/BDG_DiskFullTest.app/Contents/MacOS/BDG_DiskFullTest

hdiutil detach /Volumes/BDGTEST -quiet && rm -f /tmp/bdgtest.dmg
```

## Nota sobre includes

Os testes incluem por caminho relativo (`../src/AudioEngine.h`) de propósito.
Colocar `src/` no include path faz o `Strings.h` do projeto sombrear o
`<strings.h>` do sistema em filesystem case-insensitive (macOS), e o build
quebra de formas confusas.
