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
    juce::String recuperado, descartado;
    juce::String salvo, salvoNaPasta;
    juce::String erroProcessamento, erroDesconhecido;
    juce::String updateAvailableTitle, updateAvailableBody, updateDownload, updateIgnore;
};

namespace Strings
{
    // Portuguese table
    inline const StringTable pt
    {
        // Panel headers
        juce::CharPointer_UTF8 ("ENTRADA"),
        juce::CharPointer_UTF8 ("GRAVA\xc3\x87\xc3\x83" "O"),
        juce::CharPointer_UTF8 ("SA\xc3\x8d" "DA"),
        // Input panel
        juce::CharPointer_UTF8 ("DISPOSITIVO"),
        juce::CharPointer_UTF8 ("N\xc3\x8d" "VEL"),
        juce::CharPointer_UTF8 ("VOLUME"),
        // Output panel
        juce::CharPointer_UTF8 ("FORMATO"),
        juce::CharPointer_UTF8 ("PASTA DE DESTINO"),
        juce::CharPointer_UTF8 ("TRATAMENTO"),
        juce::CharPointer_UTF8 ("Normalizar"),
        juce::CharPointer_UTF8 ("Redu\xc3\xa7\xc3\xa3" "o de ru\xc3\xad" "do"),
        juce::CharPointer_UTF8 ("Compressor"),
        juce::CharPointer_UTF8 ("De-Esser"),
        // DSP overlay
        juce::CharPointer_UTF8 ("Normaliza\xc3\xa7\xc3\xa3" "o"),
        juce::CharPointer_UTF8 ("Redu\xc3\xa7\xc3\xa3" "o de ru\xc3\xad" "do"),
        juce::CharPointer_UTF8 ("Processando \xc3\xa1" "udio"),
        juce::CharPointer_UTF8 ("Salvando arquivo"),
        // Recording panel
        juce::CharPointer_UTF8 ("Espa\xc3\xa7" "o livre:"),
        juce::CharPointer_UTF8 ("Pronto para gravar"),
        // Warnings / errors
        juce::CharPointer_UTF8 ("Selecione um microfone."),
        juce::CharPointer_UTF8 ("Configure a pasta de destino."),
        juce::CharPointer_UTF8 ("Falha ao iniciar grava\xc3\xa7\xc3\xa3" "o."),
        juce::CharPointer_UTF8 ("Espa\xc3\xa7" "o insuficiente"),
        juce::CharPointer_UTF8 ("min). Libere espa\xc3\xa7" "o."),
        juce::CharPointer_UTF8 ("Espa\xc3\xa7" "o em disco baixo. Restam ~"),
        juce::CharPointer_UTF8 ("Grava\xc3\xa7\xc3\xa3" "o parada automaticamente: disco quase cheio."),
        juce::CharPointer_UTF8 ("Dispositivo desconectado. Grava\xc3\xa7\xc3\xa3" "o interrompida."),
        juce::CharPointer_UTF8 ("Dispositivo de \xc3\xa1" "udio desconectado durante grava\xc3\xa7\xc3\xa3" "o."),
        juce::CharPointer_UTF8 ("Grava\xc3\xa7\xc3\xa3" "o anterior encontrada. Deseja recuperar?"),
        juce::CharPointer_UTF8 ("Falha na recupera\xc3\xa7\xc3\xa3" "o."),
        // Actions
        "Recuperar", "Descartar", "Ignorar",
        "Recuperado: ", "Descartado.",
        "Salvo: ",
        juce::CharPointer_UTF8 ("A grava\xc3\xa7\xc3\xa3" "o foi salva com sucesso na pasta "),
        juce::CharPointer_UTF8 ("Erro no processamento: "),
        juce::CharPointer_UTF8 ("Erro desconhecido no processamento."),
        // Update checker
        juce::CharPointer_UTF8 ("Atualiza\xc3\xa7\xc3\xa3" "o dispon\xc3\xad" "vel"),
        juce::CharPointer_UTF8 ("Nova vers\xc3\xa3" "o %s dispon\xc3\xad" "vel.\nVers\xc3\xa3" "o atual: %s."),
        "Baixar",
        "Ignorar",
    };

    // English table
    inline const StringTable en
    {
        // Panel headers
        "INPUT", "RECORDING", "OUTPUT",
        // Input panel
        "DEVICE", "LEVEL", "VOLUME",
        // Output panel
        "FORMAT", "DESTINATION FOLDER", "TREATMENT",
        "Normalize", "Noise Reduction", "Compressor", "De-Esser",
        // DSP overlay
        "Normalization", "Noise Reduction", "Processing audio", "Saving file",
        // Recording panel
        "Free space:", "Ready to record",
        // Warnings / errors
        "Select a microphone.", "Set the destination folder.",
        "Failed to start recording.",
        "Insufficient space", "min). Free up space.",
        "Low disk space. ~",
        "Recording stopped automatically: disk almost full.",
        "Device disconnected. Recording interrupted.",
        "Audio device disconnected during recording.",
        "Previous recording found. Recover?",
        "Recovery failed.",
        // Actions
        "Recover", "Discard", "Ignore",
        "Recovered: ", "Discarded.",
        "Saved: ",
        "Recording saved successfully to folder ",
        "Processing error: ",
        "Unknown processing error.",
        // Update checker
        "Update available",
        "New version %s available.\nCurrent version: %s.",
        "Download",
        "Ignore",
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
