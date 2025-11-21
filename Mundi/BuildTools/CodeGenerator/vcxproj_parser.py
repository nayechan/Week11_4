"""
vcxproj 파일 파서
AdditionalIncludeDirectories를 읽어서 VS와 동일한 방식으로 include 경로를 해석합니다.
"""

import xml.etree.ElementTree as ET
from pathlib import Path
from typing import List, Optional


class VcxprojParser:
    """vcxproj 파일을 파싱하여 include 디렉토리 정보를 추출"""

    def __init__(self, vcxproj_path: Path):
        self.vcxproj_path = vcxproj_path.resolve()
        self.project_dir = self.vcxproj_path.parent
        self.include_dirs: List[Path] = []
        self._parse()

    def _parse(self):
        """vcxproj 파일을 파싱하여 AdditionalIncludeDirectories 추출"""
        try:
            tree = ET.parse(self.vcxproj_path)
            root = tree.getroot()

            # XML namespace 추출 (동적으로)
            ns_match = root.tag.find('{')
            if ns_match != -1:
                namespace = root.tag[ns_match+1:root.tag.find('}')]
                ns = {'ms': namespace}
            else:
                ns = {}

            # ClCompile 요소에서 AdditionalIncludeDirectories 찾기
            # namespace가 있으면 사용, 없으면 namespace 없이 검색
            if ns:
                elements = root.findall('.//ms:ClCompile/ms:AdditionalIncludeDirectories', ns)
            else:
                elements = root.findall('.//ClCompile/AdditionalIncludeDirectories')

            for elem in elements:
                include_dirs_str = elem.text
                if include_dirs_str:
                    # 세미콜론으로 구분된 경로들 파싱 (VS와 동일한 순서로)
                    for path_str in include_dirs_str.split(';'):
                        path_str = path_str.strip()
                        if path_str and not path_str.startswith('%'):  # %(AdditionalIncludeDirectories) 제외
                            expanded_path = self._expand_macros(path_str)
                            if expanded_path and expanded_path.exists():
                                self.include_dirs.append(expanded_path)
                    break  # 첫 번째 설정만 사용 (보통 Debug 설정)

        except Exception as e:
            print(f"[WARNING] Failed to parse vcxproj: {e}")
            import traceback
            traceback.print_exc()
            # 실패 시 빈 리스트
            self.include_dirs = []

    def _expand_macros(self, path_str: str) -> Optional[Path]:
        """MSBuild 매크로를 실제 경로로 확장"""
        # $(ProjectDir) 확장 - 뒤에 백슬래시나 슬래시 추가
        path_str = path_str.replace('$(ProjectDir)', str(self.project_dir) + '\\')

        # $(SolutionDir) 확장 - 프로젝트 디렉토리와 동일하다고 가정
        path_str = path_str.replace('$(SolutionDir)', str(self.project_dir) + '\\')

        # Path 객체로 변환하여 정규화
        try:
            return Path(path_str).resolve()
        except Exception:
            return None

    def resolve_include(self, include_str: str) -> Optional[Path]:
        """
        VS와 동일한 방식으로 include 문자열을 실제 파일 경로로 해석합니다.
        AdditionalIncludeDirectories에 나열된 순서대로 탐색합니다.

        Args:
            include_str: include 경로 (예: "Actor.h" 또는 "Core/Object/Actor.h")

        Returns:
            실제 파일 경로, 찾지 못하면 None
        """
        # include_str을 Path로 변환 (백슬래시 처리)
        include_path = Path(include_str.replace('\\', '/'))

        # AdditionalIncludeDirectories 순서대로 탐색 (VS 동작 모방)
        for include_dir in self.include_dirs:
            candidate = include_dir / include_path
            if candidate.exists():
                return candidate.resolve()

        return None

    def get_project_relative_path(self, file_path: Path) -> str:
        """
        절대 경로를 프로젝트 루트 기준 상대 경로로 변환합니다.

        Args:
            file_path: 절대 경로의 파일

        Returns:
            프로젝트 루트 기준 상대 경로 (Unix 스타일)
        """
        file_path = file_path.resolve()
        try:
            rel_path = file_path.relative_to(self.project_dir)
            return rel_path.as_posix()
        except ValueError:
            # 프로젝트 디렉토리 밖의 파일은 파일명만 반환
            return file_path.name

    def convert_to_absolute_include(self, include_str: str) -> str:
        """
        include 문자열을 프로젝트 루트 기준 절대 경로로 변환합니다.
        VS의 include 탐색 로직을 사용합니다.

        Args:
            include_str: 원본 include 경로 (예: "Actor.h")

        Returns:
            절대 경로 (예: "Source/Runtime/Core/Object/Actor.h") 또는 원본 경로 (찾지 못한 경우)
        """
        # VS처럼 include 경로 해석
        resolved_path = self.resolve_include(include_str)

        if resolved_path:
            # 프로젝트 루트 기준 상대 경로로 변환
            return self.get_project_relative_path(resolved_path)
        else:
            # 찾지 못한 경우 원본 반환
            return include_str
