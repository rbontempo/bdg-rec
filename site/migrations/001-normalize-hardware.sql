-- Normaliza a coluna `hardware` para o vocabulário fixo
-- (builtin / usb / bluetooth / other / none), arquivando antes o valor original.
--
-- Contexto: até a v1.1.11 o app enviava o NOME do dispositivo de entrada.
-- Nome de dispositivo de áudio PODE conter o nome da pessoa ("AirPods do
-- Fulano"), e por isso a v1.1.12 passou a enviar categoria e o servidor
-- normaliza o que chega de versões antigas (normalizeHardware em api/config.php).
--
-- Ao inspecionar o banco de produção antes de rodar, porém, os 577 registros
-- históricos se mostraram descrições de hardware ("Microphone (2- HyperX
-- QuadCast S)", "Microfone (C922 Pro Stream Webcam)"), sem nome de pessoa.
-- Esse detalhe é informação de produto útil — quais microfones os usuários
-- têm — e normalizar apagaria 345 deles em 'other'.
--
-- Daí o arquivo: `events` fica consistente, e o detalhe segue consultável.
--
-- Idempotente: cada passo só toca o que ainda não foi classificado.
--
-- Uso:
--   mysql -u USER -p BANCO < migrations/001-normalize-hardware.sql

START TRANSACTION;

-- ---------------------------------------------------------------- arquivo
CREATE TABLE IF NOT EXISTS events_hardware_legacy (
    event_id    BIGINT UNSIGNED NOT NULL PRIMARY KEY,
    hardware    VARCHAR(100) NOT NULL,
    archived_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- INSERT IGNORE + a chave primária tornam o passo repetível.
INSERT IGNORE INTO events_hardware_legacy (event_id, hardware)
SELECT id, hardware
FROM events
WHERE hardware IS NOT NULL
  AND hardware NOT IN ('builtin', 'usb', 'bluetooth', 'other', 'none');

SELECT COUNT(*) AS arquivadas FROM events_hardware_legacy;

-- ------------------------------------------------------------ normalização
SELECT COUNT(*) AS antes_com_nome_cru
FROM events
WHERE hardware IS NOT NULL
  AND hardware NOT IN ('builtin', 'usb', 'bluetooth', 'other', 'none');

UPDATE events SET hardware = 'bluetooth'
WHERE hardware IS NOT NULL
  AND hardware NOT IN ('builtin', 'usb', 'bluetooth', 'other', 'none')
  AND (LOWER(hardware) LIKE '%bluetooth%'
    OR LOWER(hardware) LIKE '%airpod%'
    OR LOWER(hardware) LIKE '%wireless%');

UPDATE events SET hardware = 'usb'
WHERE hardware IS NOT NULL
  AND hardware NOT IN ('builtin', 'usb', 'bluetooth', 'other', 'none')
  AND LOWER(hardware) LIKE '%usb%';

UPDATE events SET hardware = 'builtin'
WHERE hardware IS NOT NULL
  AND hardware NOT IN ('builtin', 'usb', 'bluetooth', 'other', 'none')
  AND (LOWER(hardware) LIKE '%built-in%'
    OR LOWER(hardware) LIKE '%builtin%'
    OR LOWER(hardware) LIKE '%internal%'
    OR LOWER(hardware) LIKE '%macbook%'
    OR LOWER(hardware) LIKE '%interno%'
    OR LOWER(hardware) LIKE '%integrado%');

UPDATE events SET hardware = 'other'
WHERE hardware IS NOT NULL
  AND hardware NOT IN ('builtin', 'usb', 'bluetooth', 'other', 'none');

-- --------------------------------------------------------------- conferência
SELECT COUNT(*) AS restaram_nao_normalizados
FROM events
WHERE hardware IS NOT NULL
  AND hardware NOT IN ('builtin', 'usb', 'bluetooth', 'other', 'none');

SELECT hardware, COUNT(*) AS total
FROM events
GROUP BY hardware
ORDER BY total DESC;

COMMIT;

-- Para recuperar o detalhe depois:
--   SELECT l.hardware, COUNT(*) n
--   FROM events_hardware_legacy l
--   GROUP BY l.hardware ORDER BY n DESC;
