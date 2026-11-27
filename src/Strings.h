#pragma once
#include <juce_core/juce_core.h>

// Bilingual string system (pt-BR / en).
// All user-visible strings go through Strings::get().xxx
// Language can be switched at runtime via Strings::setLanguage().

enum class Language { PT, EN };

struct StringTable
{
    juce::String entrada, gravacao, saida;
    juce::String dispositivo, nivel, volume;
    juce::String formato, pastaDestino, tratamento;
    juce::String normalizar, reducaoRuido, compressor, deEsser;
    juce::String normalizacao, reducaoRuidoStep, processandoAudio, salvandoArquivo;
    juce::String espacoLivre, prontoGravar;
    juce::String selecioneMic, configurePasta, falhaIniciar;
    juce::String discoInsuficiente, liberarEspaco, discoBaixo;
    juce::String gravacaoParadaDisco, dispositivoDesconectado, audioDesconectado;
    juce::String gravacaoAnterior, falhaRecuperacao;
    juce::String recuperar, descartar, ignorar;
    juce::String recuperado, descartado, recuperando;
    juce::String salvo, salvoNaPasta;
    juce::String erroProcessamento, erroDesconhecido;
    juce::String updateAvailableTitle, updateAvailableBody, updateDownload, updateIgnore;
    // Menu bar
    juce::String menuBdgRec, menuHelp;
    juce::String menuAbout, menuCheckUpdates, menuLanguage, menuQuit;
    juce::String menuWebsite, menuPortal;
    juce::String aboutBody;
    // InputPanel — BDG links
    juce::String areaRestrita, portalCliente, editePodcast;
    // OutputPanel
    juce::String selecionarPasta;
    // Recording save failure (chunks preserved)
    juce::String falhaSalvar;
    // Recording stopped because the audio device changed sample rate
    juce::String gravacaoParadaDispositivo;
    // Analytics consent (first run)
    juce::String consentTitulo, consentCorpo, consentAceitar, consentRecusar;
    juce::String menuAnalytics;
    // Shown when the write path could not keep up with the disk
    juce::String amostrasPerdidas;
    // UI state that has to match what the engine is really doing
    juce::String salvando, nadaGravado, takeSilencioso, micSilencioso, micSemResposta, falhaAbrirDispositivo, pastaIndisponivel, recuperacaoParcial, atualizado, falhaVerificarAtualizacao, gravacaoParadaEscrita, erroLeitura, erroArquivoGrande, erroMemoria, erroCancelado, erroEscritaDsp;
};

namespace Strings
{
    // Portuguese table
    inline const StringTable pt
    {
        // Panel headers
        .entrada = juce::CharPointer_UTF8 ("ENTRADA"),
        .gravacao = juce::CharPointer_UTF8 ("GRAVA\xc3\x87\xc3\x83" "O"),
        .saida = juce::CharPointer_UTF8 ("SA\xc3\x8d" "DA"),
        // Input panel
        .dispositivo = juce::CharPointer_UTF8 ("DISPOSITIVO"),
        .nivel = juce::CharPointer_UTF8 ("N\xc3\x8d" "VEL"),
        .volume = juce::CharPointer_UTF8 ("VOLUME"),
        // Output panel
        .formato = juce::CharPointer_UTF8 ("FORMATO"),
        .pastaDestino = juce::CharPointer_UTF8 ("PASTA DE DESTINO"),
        .tratamento = juce::CharPointer_UTF8 ("TRATAMENTO"),
        .normalizar = juce::CharPointer_UTF8 ("Normalizar"),
        .reducaoRuido = juce::CharPointer_UTF8 ("Redu\xc3\xa7\xc3\xa3" "o de ru\xc3\xad" "do"),
        .compressor = juce::CharPointer_UTF8 ("Compressor"),
        .deEsser = juce::CharPointer_UTF8 ("De-Esser"),
        // DSP overlay
        .normalizacao = juce::CharPointer_UTF8 ("Normaliza\xc3\xa7\xc3\xa3" "o"),
        .reducaoRuidoStep = juce::CharPointer_UTF8 ("Redu\xc3\xa7\xc3\xa3" "o de ru\xc3\xad" "do"),
        .processandoAudio = juce::CharPointer_UTF8 ("Processando \xc3\xa1" "udio"),
        .salvandoArquivo = juce::CharPointer_UTF8 ("Salvando arquivo"),
        // Recording panel
        .espacoLivre = juce::CharPointer_UTF8 ("Espa\xc3\xa7" "o livre:"),
        .prontoGravar = juce::CharPointer_UTF8 ("Pronto para gravar"),
        // Warnings / errors
        .selecioneMic = juce::CharPointer_UTF8 ("Selecione um microfone."),
        .configurePasta = juce::CharPointer_UTF8 ("Configure a pasta de destino."),
        .falhaIniciar = juce::CharPointer_UTF8 ("Falha ao iniciar grava\xc3\xa7\xc3\xa3" "o."),
        .discoInsuficiente = juce::CharPointer_UTF8 ("Espa\xc3\xa7" "o insuficiente"),
        .liberarEspaco = juce::CharPointer_UTF8 ("min). Libere espa\xc3\xa7" "o."),
        .discoBaixo = juce::CharPointer_UTF8 ("Espa\xc3\xa7" "o em disco baixo. Restam ~"),
        .gravacaoParadaDisco = juce::CharPointer_UTF8 ("Grava\xc3\xa7\xc3\xa3" "o parada automaticamente: disco quase cheio."),
        .dispositivoDesconectado = juce::CharPointer_UTF8 ("Dispositivo desconectado. Grava\xc3\xa7\xc3\xa3" "o interrompida."),
        .audioDesconectado = juce::CharPointer_UTF8 ("Dispositivo de \xc3\xa1" "udio desconectado durante grava\xc3\xa7\xc3\xa3" "o."),
        .gravacaoAnterior = juce::CharPointer_UTF8 ("Grava\xc3\xa7\xc3\xa3" "o anterior encontrada. Deseja recuperar?"),
        .falhaRecuperacao = juce::CharPointer_UTF8 ("Falha na recupera\xc3\xa7\xc3\xa3" "o."),
        // Actions
        .recuperar = "Recuperar",
        .descartar = "Descartar",
        .ignorar = "Ignorar",
        .recuperado = "Recuperado: ",
        .descartado = "Descartado.",
        .recuperando = juce::CharPointer_UTF8 ("Recuperando grava\xc3\xa7\xc3\xa3" "o anterior..."),
        .salvo = "Salvo: ",
        .salvoNaPasta = juce::CharPointer_UTF8 ("A grava\xc3\xa7\xc3\xa3" "o foi salva com sucesso na pasta "),
        .erroProcessamento = juce::CharPointer_UTF8 ("Erro no processamento: "),
        .erroDesconhecido = juce::CharPointer_UTF8 ("Erro desconhecido no processamento."),
        // Update checker
        .updateAvailableTitle = juce::CharPointer_UTF8 ("Atualiza\xc3\xa7\xc3\xa3" "o dispon\xc3\xad" "vel"),
        .updateAvailableBody = juce::CharPointer_UTF8 ("Nova vers\xc3\xa3" "o %NEW% dispon\xc3\xad" "vel.\nVers\xc3\xa3" "o atual: %CUR%."),
        .updateDownload = "Baixar",
        .updateIgnore = "Ignorar",
        // Menu bar
        .menuBdgRec = "BDG rec",
        .menuHelp = "Ajuda",
        .menuAbout = "Sobre o BDG rec",
        .menuCheckUpdates = juce::CharPointer_UTF8 ("Buscar atualiza\xc3\xa7\xc3\xb5" "es..."),
        .menuLanguage = "Idioma",
        .menuQuit = "Sair",
        .menuWebsite = "Website BDG",
        .menuPortal = "Portal do cliente",
        .aboutBody = juce::CharPointer_UTF8 ("Gravador de \xc3\xa1" "udio profissional\npara podcasts e longas grava\xc3\xa7\xc3\xb5" "es.\n\nBicho de Goiaba\nwww.bichodegoiaba.com.br"),
        // InputPanel — BDG links
        .areaRestrita = juce::CharPointer_UTF8 ("\xc3\x81" "rea restrita para clientes BDG"),
        .portalCliente = "Portal do cliente",
        .editePodcast = "Edite seu podcast com o Bicho de Goiaba",
        // OutputPanel
        .selecionarPasta = "Selecionar pasta de destino",
        // Recording save failure
        .falhaSalvar = juce::CharPointer_UTF8 ("N\xc3\xa3" "o foi poss\xc3\xad" "vel salvar o arquivo final (disco cheio?). "
                          "A grava\xc3\xa7\xc3\xa3" "o foi preservada e ser\xc3\xa1 recuperada na pr\xc3\xb3" "xima vez que abrir o app."),
        // Device sample-rate change
        .gravacaoParadaDispositivo = juce::CharPointer_UTF8 ("A grava\xc3\xa7\xc3\xa3" "o foi encerrada porque o dispositivo de \xc3\xa1" "udio mudou. "
                                        "O que j\xc3\xa1 havia sido gravado foi salvo."),
        // Analytics consent
        .consentTitulo = juce::CharPointer_UTF8 ("Dados de uso"),
        .consentCorpo = juce::CharPointer_UTF8 ("O BDG rec pode enviar dados an\xc3\xb4" "nimos de uso (sistema, vers\xc3\xa3" "o, "
                           "tipo de microfone, dura\xc3\xa7\xc3\xa3" "o das grava\xc3\xa7\xc3\xb5" "es e erros) "
                           "para ajudar a melhorar o app.\n\n"
                           "Nenhum \xc3\xa1" "udio e nenhum dado pessoal s\xc3\xa3" "o enviados. "
                           "Voc\xc3\xaa pode mudar de ideia a qualquer momento no menu."),
        .consentAceitar = juce::CharPointer_UTF8 ("Permitir"),
        .consentRecusar = juce::CharPointer_UTF8 ("N\xc3\xa3" "o enviar"),
        .menuAnalytics = juce::CharPointer_UTF8 ("Enviar dados de uso an\xc3\xb4" "nimos"),
        .amostrasPerdidas = juce::CharPointer_UTF8 ("Aten\xc3\xa7\xc3\xa3" "o: o disco n\xc3\xa3" "o acompanhou a grava\xc3\xa7\xc3\xa3" "o e "
                                "aproximadamente %S segundos de \xc3\xa1" "udio se perderam."),
        .salvando = juce::CharPointer_UTF8 ("Salvando grava\xc3\xa7" "\xc3\xa3" "o..."),
        .nadaGravado = juce::CharPointer_UTF8 ("Nenhum \xc3\xa1" "udio foi capturado, ent\xc3\xa3" "o nada foi salvo. Confira o microfone e a permiss\xc3\xa3" "o em Ajustes do Sistema > Privacidade e Seguran\xc3\xa7" "a > Microfone."),
        .takeSilencioso = juce::CharPointer_UTF8 ("O arquivo foi salvo, mas est\xc3\xa1" " em sil\xc3\xaa" "ncio total. Confira o microfone e a permiss\xc3\xa3" "o em Ajustes do Sistema > Privacidade e Seguran\xc3\xa7" "a > Microfone."),
        .micSilencioso = juce::CharPointer_UTF8 ("O microfone n\xc3\xa3" "o est\xc3\xa1" " enviando som. Confira a permiss\xc3\xa3" "o em Ajustes do Sistema > Privacidade e Seguran\xc3\xa7" "a > Microfone, ou escolha outro dispositivo."),
        .micSemResposta = juce::CharPointer_UTF8 ("O dispositivo de \xc3\xa1" "udio parou de responder. Reconecte-o ou escolha outro."),
        .falhaAbrirDispositivo = juce::CharPointer_UTF8 ("N\xc3\xa3" "o foi poss\xc3\xad" "vel abrir este dispositivo: "),
        .pastaIndisponivel = juce::CharPointer_UTF8 ("A pasta %F n\xc3\xa3" "o est\xc3\xa1" " dispon\xc3\xad" "vel. As grava\xc3\xa7" "\xc3\xb5" "es v\xc3\xa3" "o para %D at\xc3\xa9" " voc\xc3\xaa" " escolher outra."),
        .recuperacaoParcial = juce::CharPointer_UTF8 ("Recuperado em parte: %N trecho(s) n\xc3\xa3" "o puderam ser lidos. A pasta original foi mantida ao lado do arquivo."),
        .atualizado = juce::CharPointer_UTF8 ("Voc\xc3\xaa" " j\xc3\xa1" " est\xc3\xa1" " na vers\xc3\xa3" "o mais recente (%CUR%)."),
        .falhaVerificarAtualizacao = juce::CharPointer_UTF8 ("N\xc3\xa3" "o foi poss\xc3\xad" "vel verificar atualiza\xc3\xa7" "\xc3\xb5" "es. Confira sua conex\xc3\xa3" "o."),
        .gravacaoParadaEscrita = juce::CharPointer_UTF8 ("A grava\xc3\xa7" "\xc3\xa3" "o foi encerrada porque n\xc3\xa3" "o foi poss\xc3\xad" "vel continuar escrevendo no disco. O que j\xc3\xa1" " havia sido gravado foi salvo."),
        .erroLeitura = juce::CharPointer_UTF8 ("n\xc3\xa3" "o foi poss\xc3\xad" "vel ler o arquivo gravado."),
        .erroArquivoGrande = juce::CharPointer_UTF8 ("o arquivo \xc3\xa9" " grande demais para processar (mais de 6 horas)."),
        .erroMemoria = juce::CharPointer_UTF8 ("n\xc3\xa3" "o h\xc3\xa1" " mem\xc3\xb3" "ria suficiente. Tente desativar alguns tratamentos."),
        .erroCancelado = "processamento cancelado.",
        .erroEscritaDsp = juce::CharPointer_UTF8 ("n\xc3\xa3" "o foi poss\xc3\xad" "vel gravar o arquivo tratado (disco cheio?). A grava\xc3\xa7" "\xc3\xa3" "o original foi mantida sem altera\xc3\xa7" "\xc3\xa3" "o."),
    };

    // English table
    inline const StringTable en
    {
        // Panel headers
        .entrada = "INPUT",
        .gravacao = "RECORDING",
        .saida = "OUTPUT",
        // Input panel
        .dispositivo = "DEVICE",
        .nivel = "LEVEL",
        .volume = "VOLUME",
        // Output panel
        .formato = "FORMAT",
        .pastaDestino = "DESTINATION FOLDER",
        .tratamento = "TREATMENT",
        .normalizar = "Normalize",
        .reducaoRuido = "Noise Reduction",
        .compressor = "Compressor",
        .deEsser = "De-Esser",
        // DSP overlay
        .normalizacao = "Normalization",
        .reducaoRuidoStep = "Noise Reduction",
        .processandoAudio = "Processing audio",
        .salvandoArquivo = "Saving file",
        // Recording panel
        .espacoLivre = "Free space:",
        .prontoGravar = "Ready to record",
        // Warnings / errors
        .selecioneMic = "Select a microphone.",
        .configurePasta = "Set the destination folder.",
        .falhaIniciar = "Failed to start recording.",
        .discoInsuficiente = "Insufficient space",
        .liberarEspaco = "min). Free up space.",
        .discoBaixo = "Low disk space. ~",
        .gravacaoParadaDisco = "Recording stopped automatically: disk almost full.",
        .dispositivoDesconectado = "Device disconnected. Recording interrupted.",
        .audioDesconectado = "Audio device disconnected during recording.",
        .gravacaoAnterior = "Previous recording found. Recover?",
        .falhaRecuperacao = "Recovery failed.",
        // Actions
        .recuperar = "Recover",
        .descartar = "Discard",
        .ignorar = "Ignore",
        .recuperado = "Recovered: ",
        .descartado = "Discarded.",
        .recuperando = "Recovering previous recording...",
        .salvo = "Saved: ",
        .salvoNaPasta = "Recording saved successfully to folder ",
        .erroProcessamento = "Processing error: ",
        .erroDesconhecido = "Unknown processing error.",
        // Update checker
        .updateAvailableTitle = "Update available",
        .updateAvailableBody = "New version %NEW% available.\nCurrent version: %CUR%.",
        .updateDownload = "Download",
        .updateIgnore = "Ignore",
        // Menu bar
        .menuBdgRec = "BDG rec",
        .menuHelp = "Help",
        .menuAbout = "About BDG rec",
        .menuCheckUpdates = "Check for updates...",
        .menuLanguage = "Language",
        .menuQuit = "Quit",
        .menuWebsite = "BDG Website",
        .menuPortal = "Client portal",
        .aboutBody = "Professional audio recorder\nfor podcasts and long recordings.\n\nBicho de Goiaba\nwww.bichodegoiaba.com.br",
        // InputPanel — BDG links
        .areaRestrita = "Restricted area for BDG clients",
        .portalCliente = "Client portal",
        .editePodcast = "Edit your podcast with Bicho de Goiaba",
        // OutputPanel
        .selecionarPasta = "Select destination folder",
        // Recording save failure
        .falhaSalvar = "Could not save the final file (disk full?). Your recording was preserved "
                          "and will be recovered the next time you open the app.",
        // Device sample-rate change
        .gravacaoParadaDispositivo = "Recording stopped because the audio device changed. What had already been "
                                        "recorded was saved.",
        // Analytics consent
        .consentTitulo = "Usage data",
        .consentCorpo = "BDG rec can send anonymous usage data (system, version, microphone type, "
                           "recording length and errors) to help improve the app.\n\n"
                           "No audio and no personal data are sent. You can change your mind at any "
                           "time from the menu.",
        .consentAceitar = "Allow",
        .consentRecusar = "Don't send",
        .menuAnalytics = "Send anonymous usage data",
        .amostrasPerdidas = "Warning: the disk could not keep up and roughly %S seconds "
                            "of audio were lost.",
        .salvando = "Saving recording...",
        .nadaGravado = "No audio was captured, so nothing was saved. Check the microphone and its permission in System Settings > Privacy & Security > Microphone.",
        .takeSilencioso = "The file was saved, but it is completely silent. Check the microphone and its permission in System Settings > Privacy & Security > Microphone.",
        .micSilencioso = "The microphone is not sending any sound. Check its permission in System Settings > Privacy & Security > Microphone, or pick another device.",
        .micSemResposta = "The audio device stopped responding. Reconnect it or pick another one.",
        .falhaAbrirDispositivo = "Could not open this device: ",
        .pastaIndisponivel = "The folder %F is not available. Recordings will go to %D until you pick another one.",
        .recuperacaoParcial = "Partially recovered: %N segment(s) could not be read. The original folder was kept next to the file.",
        .atualizado = "You already have the latest version (%CUR%).",
        .falhaVerificarAtualizacao = "Could not check for updates. Check your connection.",
        .gravacaoParadaEscrita = "Recording stopped because the app could no longer write to the disk. What had already been recorded was saved.",
        .erroLeitura = "the recorded file could not be read.",
        .erroArquivoGrande = "the file is too large to process (over 6 hours).",
        .erroMemoria = "there is not enough memory. Try turning off some treatments.",
        .erroCancelado = "processing was cancelled.",
        .erroEscritaDsp = "the processed file could not be written (disk full?). The original recording was kept unchanged.",
    };

    // Current language (default: Portuguese)
    inline Language currentLanguage = Language::PT;

    inline void setLanguage(Language lang) { currentLanguage = lang; }
    inline Language getLanguage() { return currentLanguage; }

    inline const StringTable& get()
    {
        return (currentLanguage == Language::EN) ? en : pt;
    }
}
