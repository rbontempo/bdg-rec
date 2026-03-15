#!/bin/bash
# Deploy BDG REC site to SiteGround
# Usage: ./deploy.sh

HOST="ssh.rec.bdg.fm"
USER="u2403-j6lgrjj2nal5"
PORT="18765"
REMOTE_DIR="www/rec.bdg.fm/public_html"
LOCAL_DIR="$(cd "$(dirname "$0")" && pwd)"

FILES=(
  "index.html"
  "admin.html"
  "BDG_ico.png"
  "logo-bdg-rec.png"
  "layout-bdg-rec.png"
)

echo "Deploying BDG REC site to $HOST..."

for f in "${FILES[@]}"; do
  echo "  Uploading $f..."
  scp -P "$PORT" -i ~/.ssh/id_ed25519 "$LOCAL_DIR/$f" "$USER@$HOST:$REMOTE_DIR/$f"
done

# Upload API directory
echo "  Uploading api/..."
ssh -p "$PORT" -i ~/.ssh/id_ed25519 "$USER@$HOST" "mkdir -p $REMOTE_DIR/api"
for f in api/config.php api/events.php api/stats.php api/auth.php; do
  echo "    $f"
  scp -P "$PORT" -i ~/.ssh/id_ed25519 "$LOCAL_DIR/$f" "$USER@$HOST:$REMOTE_DIR/$f"
done

echo "Done! Site live at https://rec.bdg.fm"
echo "REMINDER: Set production credentials in api/config.php on the server"
