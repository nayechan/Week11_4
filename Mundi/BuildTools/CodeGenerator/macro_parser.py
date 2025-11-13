"""
ObjectMacros.h 파서
ADD_PROPERTY_* 매크로들을 자동으로 파싱하여 매크로-타입 매핑 정보를 생성합니다.
"""

import re
from pathlib import Path
from dataclasses import dataclass
from typing import Dict, Optional, List


@dataclass
class MacroInfo:
    """매크로 정보"""
    macro_name: str          # 예: "ADD_PROPERTY_TEXTURE"
    property_type: str       # 예: "EPropertyType::Texture"
    type_suffix: str         # 예: "TEXTURE"

    def get_cpp_type_patterns(self) -> List[str]:
        """
        C++ 타입 패턴 리스트 추출 (여러 가능성을 모두 반환)

        EPropertyType enum 값을 기반으로 자동 추론하여 모든 가능한 패턴 생성
        예: ADD_PROPERTY_AUDIO -> EPropertyType::Sound
            -> ['sound', 'usound', 'usoundbase', 'soundbase']
        """
        # EPropertyType::XXX에서 XXX 부분 추출
        if '::' in self.property_type:
            enum_value = self.property_type.split('::')[-1].lower()
        else:
            enum_value = self.type_suffix.lower()

        # 특수 케이스: 패턴을 생성하지 않는 타입들
        no_pattern_types = ['array', 'script', 'scriptfile']
        if enum_value in no_pattern_types:
            return []

        # 특수 케이스: srv는 그대로 사용
        if enum_value == 'srv':
            return ['srv', 'shaderresourceview']

        # 일반적인 패턴들 생성
        patterns = []

        # 1. enum 값 그대로 (예: 'sound', 'texture')
        patterns.append(enum_value)

        # 2. U + enum 값 (예: 'usound', 'utexture')
        patterns.append('u' + enum_value)

        # 3. U + enum 값 + Base (예: 'usoundbase')
        patterns.append('u' + enum_value + 'base')

        # 4. enum 값 + Base (예: 'soundbase')
        patterns.append(enum_value + 'base')

        return patterns


class MacroParser:
    """ObjectMacros.h 파서"""

    # ADD_PROPERTY_XXX 매크로 패턴
    # 예: #define ADD_PROPERTY_TEXTURE(...) { ... Prop.Type = EPropertyType::Texture; ... }
    # 전처리로 백슬래시-줄바꿈이 제거된 후 파싱되므로 단순한 패턴 사용
    MACRO_PATTERN = re.compile(
        r'#define\s+(ADD_PROPERTY_\w+)\s*\([^)]*\)\s*\{[^}]*?Prop\.Type\s*=\s*(EPropertyType::\w+);',
        re.DOTALL
    )

    def __init__(self, macros_header_path: Path):
        """
        Args:
            macros_header_path: ObjectMacros.h 파일 경로
        """
        self.macros_header_path = macros_header_path
        self.macros: Dict[str, MacroInfo] = {}

    def parse(self) -> Dict[str, MacroInfo]:
        """
        ObjectMacros.h를 파싱하여 매크로 정보 추출

        Returns:
            매크로 이름 -> MacroInfo 딕셔너리
        """
        if not self.macros_header_path.exists():
            print(f"Warning: ObjectMacros.h not found at {self.macros_header_path}")
            return {}

        content = self.macros_header_path.read_text(encoding='utf-8')

        # 전처리: 백슬래시-줄바꿈 제거 (C 매크로 연속 처리)
        # "\ \n" 또는 "\\\n"을 공백으로 치환
        content = re.sub(r'\\\s*\n', ' ', content)

        # 모든 ADD_PROPERTY_* 매크로 찾기
        for match in self.MACRO_PATTERN.finditer(content):
            macro_name = match.group(1)
            property_type = match.group(2)

            # 매크로 이름에서 타입 suffix 추출
            # ADD_PROPERTY_TEXTURE -> TEXTURE
            if macro_name.startswith('ADD_PROPERTY_'):
                type_suffix = macro_name[len('ADD_PROPERTY_'):]
            else:
                continue

            macro_info = MacroInfo(
                macro_name=macro_name,
                property_type=property_type,
                type_suffix=type_suffix
            )

            self.macros[macro_name] = macro_info
            print(f"[MacroParser] Found: {macro_name} -> {property_type}")

        return self.macros

    def get_macro_for_type(self, cpp_type: str) -> Optional[str]:
        """
        C++ 타입에 맞는 매크로 이름 반환 (자동 추론)

        Args:
            cpp_type: C++ 타입 문자열 (예: "UTexture*", "USoundBase*", "UStaticMesh*")

        Returns:
            매크로 이름 (예: "ADD_PROPERTY_TEXTURE") 또는 None
        """
        type_lower = cpp_type.lower()

        # 포인터 체크
        if '*' not in cpp_type:
            return None

        # 각 매크로의 패턴들과 매칭 시도
        for macro_name, macro_info in self.macros.items():
            patterns = macro_info.get_cpp_type_patterns()

            # 여러 패턴 중 하나라도 매칭되면 성공
            for pattern in patterns:
                if pattern and pattern in type_lower:
                    return macro_name

        return None

    def get_property_type_enum(self, macro_name: str) -> Optional[str]:
        """
        매크로 이름으로 EPropertyType enum 값 반환

        Args:
            macro_name: 매크로 이름 (예: "ADD_PROPERTY_TEXTURE")

        Returns:
            EPropertyType enum 값 (예: "EPropertyType::Texture") 또는 None
        """
        if macro_name in self.macros:
            return self.macros[macro_name].property_type
        return None


def create_macro_parser(source_root: Path) -> MacroParser:
    """
    MacroParser 인스턴스 생성 및 파싱 실행

    Args:
        source_root: 프로젝트 루트 경로

    Returns:
        파싱이 완료된 MacroParser 인스턴스
    """
    # ObjectMacros.h 경로 찾기
    macros_path = source_root / "Source" / "Runtime" / "Core" / "Object" / "ObjectMacros.h"

    parser = MacroParser(macros_path)
    parser.parse()

    return parser


if __name__ == "__main__":
    # 테스트용
    import sys
    if len(sys.argv) > 1:
        project_root = Path(sys.argv[1])
    else:
        project_root = Path(__file__).parent.parent.parent

    parser = create_macro_parser(project_root)

    print("\n=== Parsed Macros ===")
    for macro_name, macro_info in parser.macros.items():
        patterns = macro_info.get_cpp_type_patterns()
        patterns_str = ', '.join(patterns) if patterns else 'None'
        print(f"{macro_name:30} -> {macro_info.property_type:30} (patterns: {patterns_str})")

    print("\n=== Type Matching Test ===")
    test_types = [
        "UTexture*",
        "UStaticMesh*",
        "USkeletalMesh*",
        "UMaterialInterface*",
        "USoundBase*",
        "USoundWave*",
        "UCurveFloat*",
    ]

    for test_type in test_types:
        macro = parser.get_macro_for_type(test_type)
        print(f"{test_type:25} -> {macro}")
