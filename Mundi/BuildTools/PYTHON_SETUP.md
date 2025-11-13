# Python 임베디드 버전 설정 가이드

이 가이드는 Python을 설치하지 않고도 코드 생성 도구를 사용할 수 있도록 Python 임베디드 버전을 설정하는 방법을 설명합니다.

## 왜 임베디드 Python인가?

- ✅ **Python 설치 불필요**: 팀원 모두가 Python을 설치할 필요 없음
- ✅ **일관된 환경**: 모든 개발자가 동일한 Python 버전 사용
- ✅ **간편한 배포**: 프로젝트와 함께 배포되므로 추가 설정 없음
- ✅ **작은 용량**: 압축 시 ~15MB, 압축 해제 시 ~50MB

## 설치 방법

### Step 1: Python Embeddable 다운로드

1. [Python 공식 다운로드 페이지](https://www.python.org/downloads/windows/) 방문
2. 최신 안정 버전 선택 (예: Python 3.11.x)
3. 페이지 하단에서 **"Windows embeddable package (64-bit)"** 다운로드
   - 파일명 예: `python-3.11.9-embed-amd64.zip`

### Step 2: 압축 해제

다운로드한 zip 파일을 다음 경로에 압축 해제:

```
Mundi/Tools/Python/
```

압축 해제 후 구조:
```
Mundi/
├── Tools/
│   ├── Python/
│   │   ├── python.exe          ← Python 실행 파일
│   │   ├── python311.zip       ← 표준 라이브러리
│   │   ├── python311.dll
│   │   └── ...
│   └── CodeGenerator/
│       ├── generate.py
│       └── ...
```

### Step 3: pip 설치 (선택 사항)

Jinja2 등의 패키지가 필요한 경우:

1. [get-pip.py](https://bootstrap.pypa.io/get-pip.py) 다운로드
2. `Tools/Python/` 폴더에 저장
3. 명령 프롬프트에서 실행:
   ```batch
   cd Tools\Python
   python.exe get-pip.py
   ```

4. `python311._pth` 파일 수정 (주석 해제):
   ```
   python311.zip
   .

   # Uncomment to run site.main() automatically
   import site    ← 이 줄의 주석 제거
   ```

5. 패키지 설치:
   ```batch
   python.exe -m pip install jinja2
   ```

### Step 4: 설치 확인

```batch
cd C:\Users\Jungle\KraftonGTL_week10\Mundi
Tools\Python\python.exe --version
```

출력 예시:
```
Python 3.11.9
```

### Step 5: 코드 생성 테스트

```batch
Tools\Python\python.exe Tools\CodeGenerator\generate.py --source-dir Source\Runtime --output-dir Generated
```

## 자동화 설정

이미 `Build/PreBuild.bat` 스크립트가 임베디드 Python을 자동으로 감지하도록 설정되어 있습니다.

Visual Studio에서 빌드하면 자동으로 코드 생성이 실행됩니다.

## 트러블슈팅

### Q1. "python.exe를 찾을 수 없습니다" 오류

**원인**: Python이 잘못된 경로에 설치됨

**해결**:
```batch
dir Tools\Python\python.exe
```
위 명령으로 파일이 존재하는지 확인

### Q2. "No module named 'jinja2'" 오류

**원인**: Jinja2 패키지가 설치되지 않음

**해결**: Step 3의 pip 설치 과정 수행

### Q3. 한글이 깨짐

**원인**: 인코딩 문제

**해결**: `python311._pth` 파일에 추가:
```
import site
import sys; sys.setdefaultencoding('utf-8')
```

## Git 설정

`.gitignore`에 다음 내용이 추가되어 있어야 합니다:

```gitignore
# Python embeddable은 커밋하지 않음
Tools/Python/

# 하지만 설정 가이드는 커밋
!Tools/PYTHON_SETUP.md
```

## 대안: 시스템 Python 사용

임베디드 Python 대신 시스템에 설치된 Python을 사용하려면:

1. [Python 공식 설치 프로그램](https://www.python.org/downloads/) 다운로드
2. 설치 시 "Add Python to PATH" 체크
3. 패키지 설치:
   ```batch
   pip install jinja2
   ```

빌드 스크립트가 자동으로 시스템 Python을 감지합니다.

## 요약

1. Python embeddable 다운로드
2. `Tools/Python/`에 압축 해제
3. (선택) pip 및 Jinja2 설치
4. 빌드 실행 → 자동으로 코드 생성됨

**설치 완료 후 팀원들과 공유하세요!**
