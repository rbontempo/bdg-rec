#!/bin/bash
# Deploy do site BDG rec para o Hostinger
# Uso: ./deploy.sh
#
# Migrado da SiteGround em 2026-09-06. O destino antigo era
#   ssh.rec.bdg.fm / u2403-j6lgrjj2nal5 / porta 18765 / www/rec.bdg.fm/public_html
# e nao e' mais producao — rec.bdg.fm aponta para o Hostinger, atras do proxy da
# Cloudflare.
set -euo pipefail

HOST="92.113.37.13"                 # br-asc-web543, mesma conta de bontempo.pro e playbt.pro
USER="u347677120"
PORT="65002"
KEY="$HOME/.ssh/id_ed25519"
REMOTE_DIR="domains/rec.bdg.fm/public_html"
LOCAL_DIR="$(cd "$(dirname "$0")" && pwd)"

SCP=(scp -q -P "$PORT" -i "$KEY" -o IdentitiesOnly=yes)
SSH=(ssh -p "$PORT" -i "$KEY" -o IdentitiesOnly=yes)

FILES=(
  "index.html"
  "admin.html"
  "gravador-podcast.html"
  "BDG_ico.png"
  "logo-bdg-rec.png"
  "layout-bdg-rec.png"
  ".htaccess"                       # carrega a protecao de dotfiles — ler o arquivo
)

API_FILES=(
  "api/config.php"
  "api/events.php"
  "api/stats.php"
  "api/auth.php"
  "api/change-password.php"
  "api/logout.php"
)

echo "Publicando o BDG rec em $USER@$HOST:$REMOTE_DIR"

for f in "${FILES[@]}"; do
  echo "  $f"
  "${SCP[@]}" "$LOCAL_DIR/$f" "$USER@$HOST:$REMOTE_DIR/$f"
done

"${SSH[@]}" "$USER@$HOST" "mkdir -p $REMOTE_DIR/api"
for f in "${API_FILES[@]}"; do
  echo "  $f"
  "${SCP[@]}" "$LOCAL_DIR/$f" "$USER@$HOST:$REMOTE_DIR/$f"
done

# O .env NAO e' versionado e NAO e' enviado: vive so no servidor, com as
# credenciais de producao (DB_*, API_KEY, ADMIN_PASSWORD_HASH) e chmod 600,
# FORA do webroot: ~/domains/rec.bdg.fm/.env (ver envFilePath em api/config.php).
if "${SSH[@]}" "$USER@$HOST" "test -f $REMOTE_DIR/.env"; then
  echo "ATENCAO: ainda existe $REMOTE_DIR/.env dentro do webroot."
  echo "         Mover para domains/rec.bdg.fm/.env (o config.php ja procura la)."
fi

echo
echo "Verificando o que subiu…"
for u in / /gravador-podcast /admin.html; do
  printf "  %-20s %s\n" "$u" "$(curl -s -o /dev/null -w '%{http_code}' -m 20 "https://rec.bdg.fm$u")"
done
printf "  %-20s %s  (tem que ser 403)\n" "/.env" "$(curl -s -o /dev/null -w '%{http_code}' -m 20 https://rec.bdg.fm/.env)"

echo
echo "Pronto — https://rec.bdg.fm"
echo "O .env de producao fica so no servidor, em ~/domains/rec.bdg.fm/.env"
