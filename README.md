# Mundi 엔진

## 🚀 시작하기

### 필수 요구사항

- **Python 3.x** - PATH에 추가되어 있어야 합니다
  - 빌드 시 코드 생성 스크립트가 Python을 사용합니다
  - 설치 확인: `python --version`

### 외부 의존성 설치

`Mundi/ThirdParty/lib` 폴더는 Git에서 제외되어 있습니다. 처음 클론 후 다음 단계를 수행하세요:

1. **lib 파일 다운로드**: [다운로드 링크] (TODO: 링크 추가)
2. **압축 해제**: `Mundi/ThirdParty/lib/` 폴더에 압축 해제
3. **폴더 구조 확인**:
   ```
   Mundi/ThirdParty/lib/
   ├── DirectXTex/
   │   ├── Debug/
   │   └── Release/
   ├── DirectXTK/
   │   ├── Debug/
   │   └── Release/
   ├── FBXSDK/
   │   ├── debug/
   │   └── release/
   └── Lua/
   ```

> ⚠️ lib 폴더가 없으면 빌드가 실패합니다.

---

> 🚫 **경고: 이 내용은 Mundi 엔진 렌더링 기준의 근본입니다.**  
> 삭제하거나 수정하면 엔진 전반의 좌표계 및 버텍스 연산이 깨집니다.  
> **반드시 유지하십시오.**

## 📘 기본 좌표계

* **좌표계:** Z-Up, **왼손 좌표계 (Left-Handed)**
* **버텍스 시계 방향 (CW)** 이 **앞면(Face Front)** 으로 간주됩니다.
  > → **DirectX의 기본 설정**을 그대로 따릅니다.

---

## 🔄 OBJ 파일 Import 규칙

* OBJ 포맷은 **오른손 좌표계 + CCW(반시계)** 버텍스 순서를 사용한다고 가정합니다.
  > → 블렌더에서 OBJ 포맷으로 Export 시 기본적으로 이렇게 저장되기 때문입니다.
* 따라서 OBJ를 로드할 때, 엔진 내부 좌표계와 일치하도록 자동 변환을 수행합니다.

```cpp
FObjImporter::LoadObjModel(... , bIsRightHanded = true) // 기본값
```

즉, OBJ를 **Right-Handed → Left-Handed**,  
**CCW → CW** 방향으로 변환하여 엔진의 렌더링 방식과 동일하게 맞춥니다.

---

## 🧭 블렌더(Blender) Export 설정

* 블렌더에서 모델을 **Z-Up, X-Forward** 설정으로 Export하여  
  Mundi 엔진에 Import 시 **동일한 방향을 바라보게** 됩니다.

> 💡 참고:
> 블렌더에서 축 설정을 변경해도 **좌표계나 버텍스 순서 자체는 변하지 않습니다.**  
> 단지 **기본 회전 방향만 바뀌므로**, Mundi 엔진에서는 항상 같은 방식으로 Import하면 됩니다.

---

## ✅ 정리

| 구분     | Mundi 엔진 내부 표현      | Mundi 엔진이 해석하는 OBJ   | OBJ Import 결과 |
| ------ | ----------------- | ------------------ | ----------------- |
| 좌표계    | Z-Up, Left-Handed | Z-Up, Right-Handed | Z-Up, Left-Handed |
| 버텍스 순서 | CW (시계 방향)        | CCW (반시계 방향)       | CW |
