#!/bin/bash
# La copia instalada de update.sh (con @REPO@/@USER@ ya sustituidos) debe seguir sustituyéndolos al autoactualizarse.
cd "$(dirname "$0")/.." || exit 1
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
sed "s|@REPO@|/srv/repo|; s|@USER@|pi|" scripts/update.sh > "$T/inst1.sh"
L=$(grep '^sed ' "$T/inst1.sh") || { echo "FAIL update.sh: no hay línea de autoactualización"; exit 1; }
REPO=/srv/repo; RUSER=pi; PH="@"
eval "${L%%>*}" > "$T/inst2.sh"
if grep -q '@REPO@\|@USER@' <(grep -v '^#' "$T/inst2.sh") ; then echo "FAIL update.sh: la copia autoactualizada pierde REPO/USER"; exit 1; fi
if grep -q '^REPO="/srv/repo"; RUSER="pi"' "$T/inst2.sh"; then echo "PASS update.sh se autoactualiza conservando REPO y USER"; else echo "FAIL update.sh: REPO/USER incorrectos"; exit 1; fi
