# Python 설정 가이드

코드 생성 도구를 사용하려면 시스템에 Python이 설치되어 있어야 합니다.

## 설치 방법

### Step 1: Python 설치

1. [Python 공식 다운로드 페이지](https://www.python.org/downloads/) 방문
2. 최신 안정 버전 다운로드 (Python 3.11 이상 권장)
3. 설치 시 **"Add Python to PATH"** 반드시 체크

### Step 2: 설치 확인

명령 프롬프트에서:

```batch
py --version
```

출력 예시:

```
Python 3.11.9
```

### Step 3: 패키지 설치

```batch
pip install jinja2
```

## 트러블슈팅

### Q1. "py를 찾을 수 없습니다" 오류

**원인**: Python이 PATH에 추가되지 않음

**해결**: Python 재설치 후 "Add Python to PATH" 체크

### Q2. "No module named 'jinja2'" 오류

**원인**: Jinja2 패키지가 설치되지 않음

**해결**:

```batch
pip install jinja2
```

## 요약

1. Python 설치 (PATH 추가 체크)
2. `pip install jinja2`
3. 빌드 실행 → 자동으로 코드 생성됨
