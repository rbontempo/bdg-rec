# Contrato da telemetria

O app e o servidor eram dois repositórios até agora, e o payload de telemetria
existia escrito em dois lugares — sem nenhum que fosse o oficial. Deu no que
tinha que dar: o app passou a mandar `hardware` como categoria e o servidor
seguiu guardando como nome de dispositivo, sem ninguém notar.

Este arquivo é o contrato. Mudou de um lado, muda dos dois **no mesmo commit**.

## Endpoint

```
POST https://rec.bdg.fm/api/events.php
Content-Type: application/json
X-API-Key: <chave>
```

A chave **não é segredo**: ela está no binário e no repositório público. Serve
como identificador do app. A proteção real é o rate limit por IP no
`site/api/events.php` (60 requisições por minuto).

## Corpo

Sempre um lote, mesmo com um evento só. O servidor aceita no máximo 100 por
requisição.

```json
{
  "batch": [
    {
      "event": "app_open",
      "machine_id": "tucano-5a72ab0f",
      "os": "macOS 15.3",
      "app_version": "1.1.12",
      "hardware": "builtin",
      "locale": "pt-BR",
      "timestamp": 1786915430000,
      "extra": { }
    }
  ]
}
```

| Campo | Limite | Observação |
|---|---|---|
| `event` | lista fechada | fora dela, o evento é descartado em silêncio |
| `machine_id` | 60 chars | identificador anônimo, criado **só após consentimento** |
| `os` | 40 chars | |
| `app_version` | 15 chars | |
| `hardware` | vocabulário fixo | ver abaixo |
| `locale` | 10 chars | `pt-BR` ou `en` |
| `extra` | 2 KB | validado por tipo de evento |

### `event`

`app_open`, `recording_end`, `dsp_applied`, `export_complete`, `error`

### `hardware` — vocabulário fixo

`builtin`, `usb`, `bluetooth`, `other`, `none`

**Nunca o nome do dispositivo.** Nome de entrada de áudio quase sempre carrega
o nome da pessoa ("AirPods do Renato", "iPhone de Fulano — Microfone"), e isso
é dado pessoal.

A regra existe nos dois lados de propósito:

- app: `AudioEngine::getCurrentInputDeviceCategory()`
- servidor: `normalizeHardware()` em `site/api/config.php`

O servidor normaliza de novo o que chega porque **versões antigas do app
continuam mandando o nome cru** por tempo indeterminado. Corrigir só o cliente
deixaria o banco coletando nomes de quem não atualizou.

Histórico já gravado: `site/migrations/001-normalize-hardware.sql`.

### `extra` por evento

| Evento | Campos |
|---|---|
| `recording_end` | `duration_seconds` (0–86400) |
| `export_complete` | `file_size_mb` (0–10000) |
| `dsp_applied` | `effects[]` — `normalize`, `denoise`, `compress`, `deesser` |
| `error` | `error_code`, `message` |

`error_code` em uso: `device_lost`, `device_rate_changed`, `concat_failed`,
`samples_dropped`, `dsp_crash`.

## Consentimento

Nada é coletado, gravado em disco ou enviado antes de o usuário responder ao
diálogo da primeira execução. Ao recusar, o `machine_id` é apagado e a fila
descartada. Reversível pelo menu a qualquer momento.

Ver `AnalyticsReporter::setConsent()`.

## Ao mudar este contrato

1. Atualize este arquivo.
2. Atualize o app **e** o servidor no mesmo commit.
3. Se o campo já existir no banco em outro formato, escreva uma migração em
   `site/migrations/`.
4. Lembre que versões antigas do app continuam enviando o formato velho —
   o servidor precisa aceitar os dois.
