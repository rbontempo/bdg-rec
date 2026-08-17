// Tests that need neither an audio device nor a display, so they are the ones
// CI can actually run. The recording tests alongside this file all capture
// from a real input and only run on a developer machine.
//
// Note the relative includes: putting src/ on the include path makes the
// project's Strings.h shadow the system <strings.h> on a case-insensitive
// filesystem, which breaks the build in confusing ways.
#include "../src/UpdateChecker.h"
#include "../src/Strings.h"
#include <cstdio>

static int failures = 0;

static void check(bool cond, const juce::String& what)
{
    std::printf("%s  %s\n", cond ? "PASS" : "FAIL", what.toRawUTF8());
    if (! cond) ++failures;
}

static void testVersionComparison()
{
    std::printf("\n-- UpdateChecker::isNewerVersion --\n");
    using U = UpdateChecker;

    check(U::isNewerVersion("1.2.0", "1.1.9"),   "1.2.0 is newer than 1.1.9");
    check(U::isNewerVersion("1.1.10", "1.1.9"),  "1.1.10 is newer than 1.1.9 (not string order)");
    check(U::isNewerVersion("2.0.0", "1.9.9"),   "2.0.0 is newer than 1.9.9");

    check(! U::isNewerVersion("1.1.9", "1.1.9"), "the same version is not newer");
    check(! U::isNewerVersion("1.1.8", "1.1.9"), "an older version is not newer");
    check(! U::isNewerVersion("1.0.0", "1.1.9"), "an older minor is not newer");

    // Differing component counts, which GitHub tags do produce.
    check(U::isNewerVersion("1.2", "1.1.9"),     "1.2 is newer than 1.1.9");
    check(! U::isNewerVersion("1.1", "1.1.9"),   "1.1 is not newer than 1.1.9");

    // Garbage parses as zeros. Documenting the behaviour rather than
    // asserting it is correct — the point is that it must not report an
    // update and must not crash.
    check(! U::isNewerVersion("abc", "1.1.9"),   "non-numeric input reports no update");
    check(! U::isNewerVersion("", "1.1.9"),      "empty input reports no update");
}

static void testStringTables()
{
    std::printf("\n-- Strings tables --\n");

    // Catches the classic failure of adding a field and forgetting one table:
    // aggregate initialisation leaves the missing entries default-constructed.
    struct Entry { const char* name; juce::String pt; juce::String en; };
    const Entry entries[] = {
        #define F(x) { #x, Strings::pt.x, Strings::en.x }
        F(entrada), F(gravacao), F(saida), F(dispositivo), F(nivel), F(volume),
        F(formato), F(pastaDestino), F(tratamento), F(normalizar), F(reducaoRuido),
        F(compressor), F(deEsser), F(normalizacao), F(reducaoRuidoStep),
        F(processandoAudio), F(salvandoArquivo), F(espacoLivre), F(prontoGravar),
        F(selecioneMic), F(configurePasta), F(falhaIniciar), F(discoInsuficiente),
        F(liberarEspaco), F(discoBaixo), F(gravacaoParadaDisco),
        F(dispositivoDesconectado), F(audioDesconectado), F(gravacaoAnterior),
        F(falhaRecuperacao), F(recuperar), F(descartar), F(ignorar), F(recuperado),
        F(descartado), F(salvo), F(salvoNaPasta), F(erroProcessamento),
        F(erroDesconhecido), F(updateAvailableTitle), F(updateAvailableBody),
        F(updateDownload), F(updateIgnore), F(menuBdgRec), F(menuHelp), F(menuAbout),
        F(menuCheckUpdates), F(menuLanguage), F(menuQuit), F(menuWebsite),
        F(menuPortal), F(aboutBody), F(areaRestrita), F(portalCliente),
        F(editePodcast), F(selecionarPasta), F(falhaSalvar),
        F(gravacaoParadaDispositivo), F(consentTitulo), F(consentCorpo),
        F(consentAceitar), F(consentRecusar), F(menuAnalytics)
        #undef F
    };

    int emptyPt = 0, emptyEn = 0;
    for (const auto& e : entries)
    {
        if (e.pt.isEmpty()) { std::printf("     pt.%s is empty\n", e.name); ++emptyPt; }
        if (e.en.isEmpty()) { std::printf("     en.%s is empty\n", e.name); ++emptyEn; }
    }

    check(emptyPt == 0, "every pt-BR string is filled in");
    check(emptyEn == 0, "every English string is filled in");

    // The update dialog substitutes these; losing one would show a raw token.
    check(Strings::pt.updateAvailableBody.contains("%NEW%")
          && Strings::pt.updateAvailableBody.contains("%CUR%"),
          "pt update body keeps both substitution tokens");
    check(Strings::en.updateAvailableBody.contains("%NEW%")
          && Strings::en.updateAvailableBody.contains("%CUR%"),
          "en update body keeps both substitution tokens");
}

int main()
{
    testVersionComparison();
    testStringTables();

    std::printf("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "FAILED",
                failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
