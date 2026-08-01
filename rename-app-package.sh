#!/usr/bin/env bash
#
# Renomeia com.micewine.emu -> com.windroid.emu no repositorio Windroid-emu
# (fork do MiceWine-Application).
#
# USO:
#   ./rename-app-package.sh /caminho/para/Windroid-emu
#
# O QUE ELE FAZ:
#   1. Move as pastas do pacote Java e do AIDL com `git mv`
#      (preserva o historico do git, em vez de deletar/recriar).
#   2. Substitui em TODOS os arquivos de texto relevantes:
#        - forma com ponto:      com.micewine.emu   -> com.windroid.emu
#        - forma com barra:      com/micewine/emu   -> com/windroid/emu
#          (usada em FindClass() no codigo JNI/C)
#        - forma com underscore: com_micewine_emu   -> com_windroid_emu
#          (usada nos nomes das funcoes JNI nativas, ex:
#           Java_com_micewine_emu_LorieView_setColorProfile)
#
# IMPORTANTE:
#   - Rode isso num clone limpo, com working tree sem alteracoes pendentes.
#   - Depois de rodar, confira com `git diff --stat` e `git status`.
#   - O antigo Theme.MiceWine (nome de estilo) NAO e tocado por este script
#     de proposito -- e so uma questao de branding visual, nao de pacote.
#   - Depois de recompilar, teste a fundo as partes que usam JNI
#     (renderizacao / LorieView / ShellLoader / EmulationActivity) --
#     e onde um esquecimento gera UnsatisfiedLinkError em runtime.
#   - O app vai passar a instalar como um pacote NOVO no Android
#     (applicationId diferente). Nao substitui/atualiza uma instalacao
#     existente do MiceWine/Windroid antigo -- fica lado a lado.

set -euo pipefail

OLD_DOTTED="com.micewine.emu"
NEW_DOTTED="com.windroid.emu"
OLD_SLASHED="com/micewine/emu"
NEW_SLASHED="com/windroid/emu"
OLD_UNDERSCORE="com_micewine_emu"
NEW_UNDERSCORE="com_windroid_emu"

REPO_DIR="${1:?Uso: $0 /caminho/para/Windroid-emu}"
cd "$REPO_DIR"

if [ ! -d .git ]; then
  echo "Erro: '$REPO_DIR' nao parece ser um repositorio git." >&2
  exit 1
fi

echo "==> Repositorio: $(pwd)"

# --- 1. Mover pastas do pacote Java ---------------------------------------
JAVA_OLD="app/src/main/java/com/micewine/emu"
JAVA_NEW="app/src/main/java/com/windroid/emu"

if [ -d "$JAVA_OLD" ]; then
  echo "==> Movendo $JAVA_OLD -> $JAVA_NEW"
  mkdir -p "app/src/main/java/com/windroid"
  git mv "$JAVA_OLD" "$JAVA_NEW"
  rmdir "app/src/main/java/com/micewine" 2>/dev/null || true
else
  echo "==> (pulado) $JAVA_OLD nao encontrado -- ja renomeado?"
fi

# --- 2. Mover pasta do AIDL --------------------------------------------
AIDL_OLD="app/src/main/aidl/com/micewine/emu"
AIDL_NEW="app/src/main/aidl/com/windroid/emu"

if [ -d "$AIDL_OLD" ]; then
  echo "==> Movendo $AIDL_OLD -> $AIDL_NEW"
  mkdir -p "app/src/main/aidl/com/windroid"
  git mv "$AIDL_OLD" "$AIDL_NEW"
  rmdir "app/src/main/aidl/com/micewine" 2>/dev/null || true
else
  echo "==> (pulado) $AIDL_OLD nao encontrado -- ja renomeado?"
fi

# --- 3. Substituir conteudo de texto em todos os arquivos relevantes ------
echo "==> Procurando arquivos com referencias a com.micewine.emu ..."

mapfile -t FILES < <(grep -rIl \
  -e "$OLD_DOTTED" -e "$OLD_SLASHED" -e "$OLD_UNDERSCORE" \
  --include="*.java" --include="*.xml" --include="*.gradle" \
  --include="*.aidl" --include="*.c" --include="*.h" --include="*.py" \
  . 2>/dev/null || true)

echo "==> ${#FILES[@]} arquivo(s) serao alterados:"
printf '    %s\n' "${FILES[@]}"

for f in "${FILES[@]}"; do
  sed -i \
    -e "s#${OLD_DOTTED//./\\.}#${NEW_DOTTED}#g" \
    -e "s#${OLD_SLASHED}#${NEW_SLASHED}#g" \
    -e "s#${OLD_UNDERSCORE}#${NEW_UNDERSCORE}#g" \
    "$f"
done

echo ""
echo "==> Concluido."
echo "==> Proximos passos:"
echo "    1. git diff --stat   (revisar o que mudou)"
echo "    2. Recompilar o projeto e testar em especial:"
echo "       - LorieView / renderer.c (JNI)"
echo "       - ShellLoader / shell_loader.c (JNI)"
echo "       - EmulationActivity / activity.c (JNI)"
echo "       - Broadcasts internos (setPackage/ACTION_*)"
echo "    3. Se quiser trocar tambem o rootfs, use o outro script"
echo "       (rename-rootfs-package.sh) -- E RECOMPILE O ROOTFS INTEIRO."
