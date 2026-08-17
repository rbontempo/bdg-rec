# Menu bar nativo para BDG rec

## Objetivo

Adicionar menu bar nativo ao BDG rec com itens padrão de app desktop: About, buscar atualizações, idioma, ajuda e quit.

## Estrutura dos menus

### Menu "BDG rec" (macOS: app menu / Windows: primeiro menu)

| Item | Atalho | Ação |
|------|--------|------|
| About BDG rec | — | Dialog com logo, nome, versão, site |
| Buscar atualizações... / Check for updates... | — | Força check via GitHub API |
| ───── | | |
| Idioma / Language → PT | — | `Strings::setLanguage(PT)`, repaint all |
| Idioma / Language → EN | — | `Strings::setLanguage(EN)`, repaint all |
| ───── | | |
| Sair / Quit | ⌘Q (mac) | `JUCEApplication::quit()` |

### Menu "Ajuda" / "Help"

| Item | Ação |
|------|------|
| Website BDG | Abre `www.bichodegoiaba.com.br` |
| Portal do cliente / Client portal | Abre `https://cliente.bichodegoiaba.com.br/` |

## Implementação

### MainComponent (herda MenuBarModel)

- `getMenuBarNames()` → retorna nomes dos menus
- `getMenuBarItemsForIndex()` → popula itens com `PopupMenu`
- `menuItemSelected()` → handler para cada item
- macOS: `MenuBarModel::setMacMainMenu(this)` no constructor, `setMacMainMenu(nullptr)` no destructor
- Windows: `MenuBarComponent` como child component no topo da janela

### About dialog

`AlertWindow` mostrando:
- Ícone do BDG (já disponível em BinaryData)
- "BDG rec"
- Versão dinâmica via `JUCE_APPLICATION_VERSION_STRING`
- "Bicho de Goiaba — www.bichodegoiaba.com.br"

### UpdateChecker — novo método forceCheck()

Método público que bypassa o intervalo de 7 dias para checks manuais do menu. Reutiliza a mesma lógica de `run()`.

### Strings.h — novas strings bilíngues

Strings necessárias para ambos idiomas (PT/EN):
- menuAbout, menuCheckUpdates, menuLanguage, menuQuit
- menuHelp, menuWebsite, menuPortal
- aboutTitle, aboutBody

### Sincronização idioma

Quando idioma muda pelo menu, atualiza os botões PT|EN do HeaderBar. Quando muda pelo HeaderBar, o menu reflete automaticamente (MenuBarModel já re-renderiza).

## Arquivos modificados

- `src/MainComponent.h` — adiciona herança MenuBarModel, declara métodos
- `src/MainComponent.cpp` — implementa menu handlers, about dialog, setup do menu
- `src/Strings.h` — novas strings bilíngues
- `src/UpdateChecker.h/cpp` — novo método `forceCheck()`
