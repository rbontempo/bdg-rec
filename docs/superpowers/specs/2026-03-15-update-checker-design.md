# Update checker — BDG rec

## Objetivo

O app verifica semanalmente se existe uma versão mais recente no GitHub e exibe um diálogo modal convidando o usuário a baixar a atualização.

## Comportamento

1. Ao abrir o app, `MainComponent` chama `UpdateChecker::checkIfDue()`
2. A classe lê `lastUpdateCheck` do `ApplicationProperties` (timestamp Unix)
3. Se menos de 7 dias se passaram, retorna sem fazer nada
4. Caso contrário, dispara uma thread em background que faz GET para:
   ```
   https://api.github.com/repos/rbontempo/bdg-rec-juce/releases/latest
   ```
5. Parseia `tag_name` do JSON com `juce::JSON::parse()`
6. Compara com `JUCE_APPLICATION_VERSION_STRING` (versão compilada)
7. Salva o timestamp atual em `lastUpdateCheck`
8. Se versão remota > versão local, chama callback na main thread
9. Callback exibe `juce::AlertWindow` modal com:
   - Título: "Atualização disponível" / "Update available"
   - Corpo: "Nova versão {version} disponível. Versão atual: {current}."
   - Botão primário: "Baixar" / "Download" → abre `https://rec.bdg.fm` no navegador via `juce::URL::launchInDefaultBrowser()`
   - Botão secundário: "Ignorar" / "Ignore" → fecha o diálogo

## Arquitetura

### Novo componente: `UpdateChecker`

**Arquivos:** `src/UpdateChecker.h`, `src/UpdateChecker.cpp`

**Interface pública:**
```cpp
class UpdateChecker : private juce::Thread
{
public:
    UpdateChecker();
    ~UpdateChecker() override;

    // Checa se passaram 7+ dias desde a última verificação.
    // Se sim, dispara thread de verificação.
    void checkIfDue(std::function<void(juce::String newVersion)> onUpdateAvailable);

private:
    void run() override;  // executa a request HTTP

    std::function<void(juce::String)> callback;
    static constexpr int CHECK_INTERVAL_DAYS = 7;
};
```

**Dependências:** Apenas módulos JUCE já incluídos (`juce_core`, `juce_gui_basics`). Usa `juce::URL::readEntireTextStream()` para HTTP GET. Sem bibliotecas externas.

### Integração em `MainComponent`

```cpp
// No construtor de MainComponent:
updateChecker.checkIfDue([this](juce::String newVersion) {
    juce::MessageManager::callAsync([this, newVersion]() {
        showUpdateDialog(newVersion);
    });
});
```

### Comparação de versão

Remove prefixo `v` do `tag_name` (ex: `v1.1.0` → `1.1.0`) e compara com `JUCE_APPLICATION_VERSION_STRING` como string simples. Se forem diferentes e a remota for maior (comparação semver simplificada por segmentos numéricos), considera como atualização disponível.

### Persistência

Usa `ApplicationProperties` (já existente no app) para salvar:
- `lastUpdateCheck` — timestamp Unix (int64) da última verificação

### Bilíngue

Usa o sistema `Strings` existente. Adiciona entradas em `Strings.h`:
- `atualizacaoDisponivel` / "Update available"
- `novaVersao` / "New version {v} available. Current: {c}."
- `baixar` / "Download"
- `ignorar` / "Ignore"

## Edge cases

| Cenário | Comportamento |
|---------|--------------|
| Sem internet | Request falha silenciosamente, tenta na próxima semana |
| GitHub rate limit (403) | Falha silenciosa |
| JSON inválido | Falha silenciosa |
| Versão igual | Não mostra diálogo |
| Versão remota menor | Não mostra diálogo (dev/beta local) |
| Primeiro uso (sem timestamp) | Verifica imediatamente |

## Não inclui

- Download automático do instalador
- Instalação automática
- Notificações push
- Changelog na UI
