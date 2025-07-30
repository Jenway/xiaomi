#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(pwd)"
MONOREPO_DIR="$REPO_ROOT/mono"
REPOS=(lab01 lab02 lab03 lab04 lab05 lab06 lab07 lab08 lab09 lab10 lab11 lab12)

# ========== New Step 0: Set up a shared LFS cache ==========
LFS_CACHE_DIR="$REPO_ROOT/lfs_cache"
mkdir -p "$LFS_CACHE_DIR"
echo "📁 Using shared LFS cache at $LFS_CACHE_DIR"

# 初始化 monorepo
mkdir -p "$MONOREPO_DIR"
cd "$MONOREPO_DIR"
if [ ! -d .git ]; then
  git init
  git lfs install
  # Key Fix: Configure the monorepo to use the shared cache
  git config lfs.storage "$LFS_CACHE_DIR"
  git commit --allow-empty -m "Initial empty commit"
  echo "📁 Created monorepo at $MONOREPO_DIR"
fi

# ========== Step 1: 并行拉取 LFS 到共享缓存 ==========
echo "📦 Step 1: Parallel LFS pull into shared cache..."
pull_lfs() {
  local dir="$1"
  local repo_path="$REPO_ROOT/$dir"
  echo "  ⏳ Pulling LFS for $dir"
  git -C "$repo_path" config lfs.storage "$LFS_CACHE_DIR"
  git -C "$repo_path" lfs pull 2>/dev/null \
    && echo "  ✅ LFS pull for $dir done" \
    || echo "  ❌ LFS pull failed for $dir"
}
export -f pull_lfs
export REPO_ROOT
export LFS_CACHE_DIR # Export for the sub-shell
printf "%s\n" "${REPOS[@]}" | xargs -n1 -P4 -I{} bash -c 'pull_lfs "$@"' _ {}

# ========== Step 2: 导入所有仓库 ==========
echo "📦 Step 2: Importing repositories into monorepo..."
for repo in "${REPOS[@]}"; do
  REPO_PATH="$REPO_ROOT/$repo"
  LAB_NAME="$(basename "$repo")"
  REMOTE_NAME="${LAB_NAME}_remote"

  echo "📦 Processing $LAB_NAME..."

  # The 'subtree' command will now find LFS objects in the shared cache
  git remote add --fetch "$REMOTE_NAME" "$REPO_PATH" > /dev/null

  BRANCHES=$(git -C "$REPO_PATH" for-each-ref --format='%(refname:short)' refs/heads)

  for branch in $BRANCHES; do
    if [[ "$branch" == "master" ]]; then
      SUBDIR="$LAB_NAME"
    else
      SUBDIR="${LAB_NAME}-${branch}"
    fi

    if [ -e "$SUBDIR" ]; then
      echo "  ⚠️  Skipping $branch — $SUBDIR already exists."
      continue
    fi

    echo "  🔄 Importing $branch → $SUBDIR"
    git subtree add --prefix="$SUBDIR" "$REMOTE_NAME" "$branch" -m "Import $LAB_NAME:$branch → $SUBDIR" \
      && echo "  ✅ Imported $branch → $SUBDIR" \
      || echo "  ❌ Failed to import $branch → $SUBDIR"
  done

  git remote remove "$REMOTE_NAME"
done

echo "✅ All repositories imported into $MONOREPO_DIR"
