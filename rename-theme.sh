#!/usr/bin/env bash
#
# Renomeia o tema/estilos de MiceWine -> Windroid no repositorio Windroid-emu.
#
# USO:
#   ./rename-theme.sh /caminho/para/Windroid-emu
#
# O QUE ELE FAZ:
#   Troca (case-sensitive) a string "MiceWine" por "Windroid" em todo
#   arquivo .xml que a contenha. Isso cobre tanto as DEFINICOES dos
#   estilos (res/values/themes.xml, res/values/styles.xml e as
#   variantes res/values-night/) quanto os USOS (android:theme=... e
#   style=... nos 30+ layouts, alem do AndroidManifest.xml):
#
#     Theme.MiceWine                -> Theme.Windroid
#     Theme.MiceWine.FullScreen     -> Theme.Windroid.FullScreen
#     Base.Theme.MiceWine           -> Base.Theme.Windroid
#     Base.Theme.MiceWine.FullScreen -> Base.Theme.Windroid.FullScreen
#     MiceWine.CardView             -> Windroid.CardView
#
#   E' independente do script de rename do pacote (com.micewine.emu) --
#   pode ser rodado antes, depois, ou sozinho.
#
# NAO MEXE em:
#   - nome do arquivo .so nativo (libmicewine.so, definido no
#     CMakeLists.txt como add_library(micewine ...))
#   - o secret do CI (KEYSTORE_BASE64_MICEWINE) em .github/workflows/
#   - mencoes a "Micewine"/"MiceWine" em texto livre (README.md)
#   (esses usam "micewine" minusculo ou sao branding fora do escopo
#   de tema visual; se quiser trocar tambem, e' so pedir.)

set -euo pipefail

OLD="MiceWine"
NEW="Windroid"

REPO_DIR="${1:?Uso: $0 /caminho/para/Windroid-emu}"
cd "$REPO_DIR"

if [ ! -d .git ]; then
  echo "Erro: '$REPO_DIR' nao parece ser um repositorio git." >&2
  exit 1
fi

echo "==> Repositorio: $(pwd)"
echo "==> Procurando arquivos .xml com '$OLD' ..."

mapfile -t FILES < <(grep -rIl "$OLD" --include="*.xml" . 2>/dev/null || true)

echo "==> ${#FILES[@]} arquivo(s) serao alterados:"
printf '    %s\n' "${FILES[@]}"

for f in "${FILES[@]}"; do
  sed -i "s/${OLD}/${NEW}/g" "$f"
done

echo ""
echo "==> Concluido."
echo "==> Proximos passos:"
echo "    1. git diff --stat   (revisar o que mudou)"
echo "    2. Recompilar e conferir visualmente as telas -- e so"
echo "       renomeacao de identificador de estilo, nao muda cores"
echo "       nem layout, mas vale conferir."
