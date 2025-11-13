# Git Hooks for Mundi Engine

이 디렉토리에는 Mundi Engine 프로젝트를 위한 Git hooks가 포함되어 있습니다.

## 📋 포함된 Hooks

### post-checkout
브랜치를 전환할 때 자동으로 실행됩니다.
- **기능:**
  - Git LFS 파일 자동 동기화
  - Submodule 자동 업데이트 (`git submodule update --init --recursive`)
- **목적:** 브랜치 전환 시 BuildTools submodule이 올바른 버전으로 업데이트되도록 보장

### post-merge
Git merge 후 자동으로 실행됩니다.
- **기능:**
  - Git LFS 파일 자동 동기화
  - Submodule 자동 업데이트
- **목적:** Merge 후 submodule 변경사항이 자동으로 반영되도록 보장

### pre-push
Git push 전에 실행됩니다.
- **기능:**
  - Git LFS 파일 검증
  - Submodule의 unpushed commit 검사 (경고만, 차단 안 함)
- **목적:** Submodule 커밋을 push하지 않고 부모 repo만 push하는 실수 방지

## 🚀 설치 방법

### Windows

```cmd
.githooks\install-hooks.bat
```

또는 Git Bash에서:

```bash
bash .githooks/install-hooks.sh
```

### Linux/Mac

```bash
bash .githooks/install-hooks.sh
```

## 🔧 수동 설치

자동 설치 스크립트를 사용하지 않으려면, 아래 명령어로 수동 설치할 수 있습니다:

```bash
cp .githooks/post-checkout .git/hooks/
cp .githooks/post-merge .git/hooks/
cp .githooks/pre-push .git/hooks/
chmod +x .git/hooks/post-checkout
chmod +x .git/hooks/post-merge
chmod +x .git/hooks/pre-push
```

## ⚠️ 중요 사항

1. **Git LFS 필수**: 이 hooks는 Git LFS가 설치되어 있어야 작동합니다.
2. **Submodule 자동 업데이트**: 브랜치 전환 시 submodule이 자동으로 업데이트되므로, 로컬 변경사항이 있다면 주의하세요.
3. **Pre-push 경고**: Submodule에 unpushed commit이 있어도 push는 진행됩니다. 경고만 표시됩니다.

## 🔍 Hook 비활성화

특정 hook을 비활성화하려면 `.git/hooks/` 디렉토리에서 해당 파일을 삭제하거나 이름을 변경하세요:

```bash
# 예: pre-push hook 비활성화
mv .git/hooks/pre-push .git/hooks/pre-push.disabled
```

## 📝 Hook 수정

Hook을 수정하려면:
1. `.githooks/` 디렉토리의 파일을 수정
2. 설치 스크립트 재실행
3. (선택) 수정 사항을 커밋하여 팀원들과 공유

## 🤝 팀 협업

- 이 hooks는 Git으로 추적되므로, 팀 전체가 동일한 hooks를 사용할 수 있습니다.
- 새로운 팀원은 클론 후 설치 스크립트만 실행하면 됩니다.
- Hooks를 수정할 때는 팀원들에게 공지하세요.
