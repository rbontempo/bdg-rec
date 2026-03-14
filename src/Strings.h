#pragma once
#include <juce_core/juce_core.h>

// All user-visible Portuguese strings, using CharPointer_UTF8 for correct encoding.
// Hex escapes: ç=\xc3\xa7  ã=\xc3\xa3  á=\xc3\xa1  í=\xc3\xad  ó=\xc3\xb3
//              Ç=\xc3\x87  Ã=\xc3\x83  Á=\xc3\x81  Í=\xc3\x8d
// NOTE: When a hex escape is immediately followed by a hex digit or letter a-f,
// use adjacent string literal concatenation to break the sequence, e.g. "\xc3\x8d" "DA".

namespace Strings
{
    // ── Panel headers ──
    inline const juce::String entrada       { juce::CharPointer_UTF8 ("ENTRADA") };
    inline const juce::String gravacao      { juce::CharPointer_UTF8 ("GRAVA\xc3\x87\xc3\x83" "O") };
    inline const juce::String saida         { juce::CharPointer_UTF8 ("SA\xc3\x8d" "DA") };

    // ── Input panel ──
    inline const juce::String dispositivo   { juce::CharPointer_UTF8 ("DISPOSITIVO") };
    inline const juce::String nivel         { juce::CharPointer_UTF8 ("N\xc3\x8d" "VEL") };
    inline const juce::String volume        { juce::CharPointer_UTF8 ("VOLUME") };

    // ── Output panel ──
    inline const juce::String formato       { juce::CharPointer_UTF8 ("FORMATO") };
    inline const juce::String pastaDestino  { juce::CharPointer_UTF8 ("PASTA DE DESTINO") };
    inline const juce::String tratamento    { juce::CharPointer_UTF8 ("TRATAMENTO") };
    inline const juce::String normalizar    { juce::CharPointer_UTF8 ("Normalizar") };
    inline const juce::String reducaoRuido  { juce::CharPointer_UTF8 ("Redu\xc3\xa7\xc3\xa3" "o de ru\xc3\xad" "do") };
    inline const juce::String compressor    { juce::CharPointer_UTF8 ("Compressor") };
    inline const juce::String deEsser       { juce::CharPointer_UTF8 ("De-Esser") };

    // ── DSP overlay steps ──
    inline const juce::String normalizacao      { juce::CharPointer_UTF8 ("Normaliza\xc3\xa7\xc3\xa3" "o") };
    inline const juce::String reducaoRuidoStep  { juce::CharPointer_UTF8 ("Redu\xc3\xa7\xc3\xa3" "o de ru\xc3\xad" "do") };
    inline const juce::String processandoAudio  { juce::CharPointer_UTF8 ("Processando \xc3\xa1" "udio") };
    inline const juce::String salvandoArquivo   { juce::CharPointer_UTF8 ("Salvando arquivo") };

    // ── Recording panel ──
    inline const juce::String espacoLivre   { juce::CharPointer_UTF8 ("Espa\xc3\xa7" "o livre:") };
    inline const juce::String prontoGravar  { juce::CharPointer_UTF8 ("Pronto para gravar") };

    // ── Warnings / errors ──
    inline const juce::String selecioneMic        { juce::CharPointer_UTF8 ("Selecione um microfone.") };
    inline const juce::String configurePasta      { juce::CharPointer_UTF8 ("Configure a pasta de destino.") };
    inline const juce::String falhaIniciar        { juce::CharPointer_UTF8 ("Falha ao iniciar grava\xc3\xa7\xc3\xa3" "o.") };
    inline const juce::String discoInsuficiente   { juce::CharPointer_UTF8 ("Espa\xc3\xa7" "o insuficiente") };
    inline const juce::String liberarEspaco       { juce::CharPointer_UTF8 ("min). Libere espa\xc3\xa7" "o.") };
    inline const juce::String discoBaixo          { juce::CharPointer_UTF8 ("Espa\xc3\xa7" "o em disco baixo. Restam ~") };
    inline const juce::String gravacaoParadaDisco { juce::CharPointer_UTF8 ("Grava\xc3\xa7\xc3\xa3" "o parada automaticamente: disco quase cheio.") };
    inline const juce::String dispositivoDesconectado { juce::CharPointer_UTF8 ("Dispositivo desconectado. Grava\xc3\xa7\xc3\xa3" "o interrompida.") };
    inline const juce::String audioDesconectado   { juce::CharPointer_UTF8 ("Dispositivo de \xc3\xa1" "udio desconectado durante grava\xc3\xa7\xc3\xa3" "o.") };
    inline const juce::String gravacaoAnterior    { juce::CharPointer_UTF8 ("Grava\xc3\xa7\xc3\xa3" "o anterior encontrada. Deseja recuperar?") };
    inline const juce::String falhaRecuperacao    { juce::CharPointer_UTF8 ("Falha na recupera\xc3\xa7\xc3\xa3" "o.") };
}
