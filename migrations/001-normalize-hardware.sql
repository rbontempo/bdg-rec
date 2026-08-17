-- Normaliza a coluna `hardware` para o vocabulário fixo
-- (builtin / usb / bluetooth / other / none).
--
-- Motivo: até a v1.1.11 o app enviava o NOME do dispositivo de entrada, e
-- nomes de dispositivo de áudio costumam conter o nome da pessoa — "AirPods
-- do Renato", "iPhone de Fulano — Microfone". Isso é dado pessoal, e está
-- gravado nas linhas antigas desta tabela.
--
-- A partir da v1.1.12 o app envia categoria, e `normalizeHardware()` em
-- api/config.php converte o que chegar de versões antigas. Esta migração
-- cuida do que já está no banco.
--
-- A ordem dos UPDATEs importa: cada um só toca linhas que ainda não foram
-- classificadas, então rodar de novo é inofensivo (idempotente).
--
-- Uso:
--   mysql -u USER -p bdg_analytics < migrations/001-normalize-hardware.sql

START TRANSACTION;

-- Quantas linhas ainda têm nome cru (para conferir depois).
SELECT COUNT(*) AS linhas_com_nome_cru
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

-- Tudo que sobrou vira 'other'. É aqui que os nomes próprios somem.
UPDATE events SET hardware = 'other'
WHERE hardware IS NOT NULL
  AND hardware NOT IN ('builtin', 'usb', 'bluetooth', 'other', 'none');

-- Confirmação: deve retornar zero.
SELECT COUNT(*) AS restaram_nao_normalizados
FROM events
WHERE hardware IS NOT NULL
  AND hardware NOT IN ('builtin', 'usb', 'bluetooth', 'other', 'none');

SELECT hardware, COUNT(*) AS total
FROM events
GROUP BY hardware
ORDER BY total DESC;

COMMIT;
