#!/bin/bash
# Git Hooks 설치 스크립트
# 이 스크립트는 .githooks 디렉토리의 hook들을 .git/hooks로 복사합니다.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HOOKS_DIR="$(git rev-parse --git-dir)/hooks"

echo "Installing Git hooks..."
echo "Source: $SCRIPT_DIR"
echo "Target: $HOOKS_DIR"
echo

# Hook 파일 복사
for hook in post-checkout post-merge pre-push; do
    if [ -f "$SCRIPT_DIR/$hook" ]; then
        cp "$SCRIPT_DIR/$hook" "$HOOKS_DIR/$hook"
        chmod +x "$HOOKS_DIR/$hook"
        echo "✓ Installed: $hook"
    fi
done

echo
echo "Git hooks installed successfully!"
echo
echo "Installed hooks:"
echo "  - post-checkout: Auto-update submodules after branch checkout"
echo "  - post-merge: Auto-update submodules after merge"
echo "  - pre-push: Warn if submodule commits are not pushed"
echo
