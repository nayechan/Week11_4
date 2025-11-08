#!/usr/bin/env python3
"""
모든 클래스에 UCLASS 매크로를 자동으로 추가하는 스크립트
"""

import re
from pathlib import Path
import sys


# 클래스별 DisplayName 및 Description 매핑
CLASS_DESCRIPTIONS = {
    # Actors
    "ADirectionalLightActor": ("방향성 라이트", "태양광과 같은 평행광을 생성하는 액터입니다"),
    "APointLightActor": ("포인트 라이트", "점광원을 생성하는 액터입니다"),
    "ASpotLightActor": ("스포트 라이트", "원뿔 모양의 조명을 생성하는 액터입니다"),
    "AAmbientLightActor": ("앰비언트 라이트", "전역 조명을 생성하는 액터입니다"),
    "AStaticMeshActor": ("스태틱 메시", "정적 메시를 배치하는 액터입니다"),
    "ACameraActor": ("카메라", "씬을 렌더링할 카메라 액터입니다"),
    "ADecalActor": ("데칼", "표면에 데칼을 투영하는 액터입니다"),
    "AHeightFogActor": ("높이 안개", "높이 기반 안개 효과를 생성하는 액터입니다"),
    "AEmptyActor": ("빈 액터", "기본 빈 액터입니다"),
    "AFakeSpotLightActor": ("가짜 스포트 라이트", "테스트용 스포트 라이트 액터입니다"),
    "AFireBallActor": ("파이어볼", "파이어볼 이펙트 액터입니다"),

    # Components - Lights
    "UDirectionalLightComponent": ("방향성 라이트 컴포넌트", "평행광 조명 컴포넌트입니다"),
    "UPointLightComponent": ("포인트 라이트 컴포넌트", "점광원 조명 컴포넌트입니다"),
    "USpotLightComponent": ("스포트 라이트 컴포넌트", "원뿔 조명 컴포넌트입니다"),
    "UAmbientLightComponent": ("앰비언트 라이트 컴포넌트", "전역 조명 컴포넌트입니다"),
    "ULightComponent": ("라이트 컴포넌트", "기본 조명 컴포넌트입니다"),
    "ULightComponentBase": ("라이트 베이스 컴포넌트", "모든 조명의 기본 컴포넌트입니다"),
    "ULocalLightComponent": ("로컬 라이트 컴포넌트", "국소 조명 컴포넌트입니다"),

    # Components - Mesh & Rendering
    "UStaticMeshComponent": ("스태틱 메시 컴포넌트", "정적 메시를 렌더링하는 컴포넌트입니다"),
    "UMeshComponent": ("메시 컴포넌트", "메시 렌더링 기본 컴포넌트입니다"),
    "UPrimitiveComponent": ("프리미티브 컴포넌트", "렌더링 가능한 기본 컴포넌트입니다"),
    "UBillboardComponent": ("빌보드 컴포넌트", "항상 카메라를 향하는 스프라이트 컴포넌트입니다"),
    "UDecalComponent": ("데칼 컴포넌트", "표면에 데칼을 투영하는 컴포넌트입니다"),
    "UPerspectiveDecalComponent": ("원근 데칼 컴포넌트", "원근 투영 데칼 컴포넌트입니다"),
    "UTextRenderComponent": ("텍스트 렌더 컴포넌트", "3D 공간에 텍스트를 렌더링하는 컴포넌트입니다"),

    # Components - Collision & Shape
    "UShapeComponent": ("셰이프 컴포넌트", "충돌 모양 기본 컴포넌트입니다"),
    "UBoxComponent": ("박스 컴포넌트", "박스 모양 충돌 컴포넌트입니다"),
    "USphereComponent": ("구체 컴포넌트", "구체 모양 충돌 컴포넌트입니다"),
    "UCapsuleComponent": ("캡슐 컴포넌트", "캡슐 모양 충돌 컴포넌트입니다"),

    # Components - Camera & Effects
    "UCameraComponent": ("카메라 컴포넌트", "카메라 뷰포트 컴포넌트입니다"),
    "UHeightFogComponent": ("높이 안개 컴포넌트", "높이 기반 안개 컴포넌트입니다"),

    # Components - Audio & Script
    "UAudioComponent": ("오디오 컴포넌트", "사운드를 재생하는 컴포넌트입니다"),
    "ULuaScriptComponent": ("Lua 스크립트 컴포넌트", "Lua 스크립트를 실행하는 컴포넌트입니다"),

    # Components - Movement
    "UMovementComponent": ("이동 컴포넌트", "기본 이동 컴포넌트입니다"),
    "UProjectileMovementComponent": ("발사체 이동 컴포넌트", "발사체 물리 이동 컴포넌트입니다"),
    "URotatingMovementComponent": ("회전 이동 컴포넌트", "자동 회전 컴포넌트입니다"),

    # Components - Other
    "USceneComponent": ("씬 컴포넌트", "트랜스폼을 가진 기본 컴포넌트입니다"),
    "UTestAutoBindComponent": ("테스트 자동 바인딩 컴포넌트", "자동 바인딩 테스트용 컴포넌트입니다"),
}


def add_uclass_to_file(header_path: Path) -> bool:
    """
    헤더 파일에 UCLASS 매크로를 추가합니다.

    Returns:
        True if file was modified, False otherwise
    """
    content = header_path.read_text(encoding='utf-8')
    original_content = content

    # GENERATED_REFLECTION_BODY() 체크
    if 'GENERATED_REFLECTION_BODY()' not in content:
        return False

    # 이미 UCLASS가 있는지 확인
    if 'UCLASS(' in content:
        return False

    # 클래스 정보 추출
    class_match = re.search(r'class\s+(\w+)\s*:\s*public\s+(\w+)', content)
    if not class_match:
        return False

    class_name = class_match.group(1)

    # 설명 가져오기
    if class_name in CLASS_DESCRIPTIONS:
        display_name, description = CLASS_DESCRIPTIONS[class_name]
    else:
        # 기본값
        display_name = class_name
        description = f"{class_name} 컴포넌트" if class_name.startswith('U') else f"{class_name} 액터"

    # UCLASS 매크로 추가
    # 패턴: class ClassName : public ParentClass
    # 변경: UCLASS(DisplayName="...", Description="...")\nclass ClassName : public ParentClass
    uclass_line = f'UCLASS(DisplayName="{display_name}", Description="{description}")\n'

    content = re.sub(
        r'(class\s+\w+\s*:\s*public\s+\w+)',
        rf'{uclass_line}\1',
        content,
        count=1
    )

    if content != original_content:
        header_path.write_text(content, encoding='utf-8')
        print(f"  ✅ {header_path.name} - Added UCLASS")
        return True

    return False


def main():
    if len(sys.argv) < 2:
        print("Usage: python add_uclass.py <source-dir>")
        print("Example: python add_uclass.py Source/Runtime")
        sys.exit(1)

    source_dir = Path(sys.argv[1])

    if not source_dir.exists():
        print(f"❌ Error: Directory not found: {source_dir}")
        sys.exit(1)

    print("=" * 60)
    print("🔧 Adding UCLASS macros to headers")
    print("=" * 60)
    print(f"📁 Source: {source_dir}\n")

    updated_count = 0

    # 모든 .h 파일 찾기
    for header in source_dir.rglob("*.h"):
        if add_uclass_to_file(header):
            updated_count += 1

    print()
    print("=" * 60)
    print(f"✅ Update complete! ({updated_count} files updated)")
    print("=" * 60)


if __name__ == '__main__':
    main()
