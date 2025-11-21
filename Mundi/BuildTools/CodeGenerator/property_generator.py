"""
프로퍼티 리플렉션 코드 생성 모듈
BEGIN_PROPERTIES / END_PROPERTIES 블록을 생성합니다.
"""

from jinja2 import Template
from header_parser import ClassInfo, Property


PROPERTY_TEMPLATE = """
{{ begin_macro }}({{ class_name }})
{%- if mark_type == 'SPAWNABLE' %}
    MARK_AS_SPAWNABLE("{{ display_name }}", "{{ description }}")
{%- elif mark_type == 'COMPONENT' %}
    MARK_AS_COMPONENT("{{ display_name }}", "{{ description }}")
{%- endif %}
{%- for prop in properties %}
    {%- if prop.get_property_type_macro() == 'ADD_PROPERTY_RANGE' %}
    ADD_PROPERTY_RANGE({{ prop.type }}, {{ prop.name }}, "{{ prop.category }}", {{ prop.min_value }}f, {{ prop.max_value }}f, {{ 'true' if prop.editable else 'false' }}{% if prop.tooltip %}, "{{ prop.tooltip }}"{% endif %})
    {%- elif prop.get_property_type_macro() == 'ADD_PROPERTY_ARRAY' %}
    ADD_PROPERTY_ARRAY({{ prop.metadata.get('inner_type', 'EPropertyType::ObjectPtr') }}, {{ prop.name }}, "{{ prop.category }}", {{ 'true' if prop.editable else 'false' }}{% if prop.tooltip %}, "{{ prop.tooltip }}"{% endif %})
    {%- elif prop.get_property_type_macro() == 'ADD_PROPERTY_MAP' %}
    ADD_PROPERTY_MAP({{ prop.metadata.get('key_type', 'EPropertyType::FString') }}, {{ prop.metadata.get('value_type', 'EPropertyType::Int32') }}, {{ prop.name }}, "{{ prop.category }}", {{ 'true' if prop.editable else 'false' }}{% if prop.tooltip %}, "{{ prop.tooltip }}"{% endif %})
    {%- elif prop.get_property_type_macro() == 'ADD_PROPERTY_SCRIPT' %}
    ADD_PROPERTY_SCRIPT({{ prop.type }}, {{ prop.name }}, "{{ prop.category }}", "{{ prop.metadata.get('ScriptFile', '.lua') }}", {{ 'true' if prop.editable else 'false' }}{% if prop.tooltip %}, "{{ prop.tooltip }}"{% endif %})
    {%- else %}
    {{ prop.get_property_type_macro() }}({{ prop.type }}, {{ prop.name }}, "{{ prop.category }}", {{ 'true' if prop.editable else 'false' }}{% if prop.tooltip %}, "{{ prop.tooltip }}"{% endif %})
    {%- endif %}
    {%- if prop.metadata %}
    {%- for key, value in prop.metadata.items() %}
    {%- if key not in ['inner_type', 'key_type', 'value_type', 'ScriptFile'] %}
    ADD_PROPERTY_METADATA({{ prop.name }}, "{{ key }}", "{{ value }}")
    {%- endif %}
    {%- endfor %}
    {%- endif %}
{%- endfor %}
END_PROPERTIES()
"""


class PropertyGenerator:
    """프로퍼티 등록 코드 생성기"""

    def __init__(self):
        self.template = Template(PROPERTY_TEMPLATE)

    def generate(self, class_info: ClassInfo) -> str:
        """ClassInfo로부터 BEGIN_PROPERTIES/BEGIN_STRUCT_PROPERTIES 블록 생성"""

        # struct인지 class인지에 따라 BEGIN 매크로 이름과 mark_type 결정
        if hasattr(class_info, 'type') and class_info.type == "struct":
            begin_macro = "BEGIN_STRUCT_PROPERTIES"
            mark_type = None  # struct는 MARK 매크로 없음
        else:
            begin_macro = "BEGIN_PROPERTIES"
            # mark_type 결정:
            # 1. Abstract 클래스는 MARK 없음 (언리얼 엔진 패턴)
            # 2. AActor/UActorComponent 자체는 MARK 없음
            # 3. AActor를 상속받은 클래스는 MARK_AS_SPAWNABLE (직접/간접)
            # 4. UActorComponent를 상속받은 클래스는 MARK_AS_COMPONENT (직접/간접)
            # 5. 그 외 (순수 UObject 등)는 MARK 없음
            mark_type = None

            if class_info.is_abstract:
                mark_type = None  # Abstract 클래스는 에디터에 노출 안 함
            elif class_info.name in ['AActor', 'UActorComponent']:
                mark_type = None  # 베이스 클래스는 MARK 없음
            elif hasattr(class_info, 'is_derived_from') and class_info.is_derived_from('AActor'):
                mark_type = 'SPAWNABLE'  # AActor 파생 (직접/간접)
            elif hasattr(class_info, 'is_derived_from') and class_info.is_derived_from('UActorComponent'):
                mark_type = 'COMPONENT'  # UActorComponent 파생 (직접/간접)
            # else: mark_type은 None으로 유지 (순수 UObject 등)

        # DisplayName과 Description 결정
        display_name = class_info.display_name or class_info.name
        description = class_info.description or f"Auto-generated {class_info.name}"

        if not class_info.properties:
            # 프로퍼티가 없어도 기본 블록은 생성
            if mark_type == 'SPAWNABLE':
                mark_line = f'    MARK_AS_SPAWNABLE("{display_name}", "{description}")'
            elif mark_type == 'COMPONENT':
                mark_line = f'    MARK_AS_COMPONENT("{display_name}", "{description}")'
            else:
                mark_line = ''

            return f"""
{begin_macro}({class_info.name})
{mark_line}
END_PROPERTIES()
"""

        # 템플릿 재사용: begin_macro만 다르게 전달
        return self.template.render(
            begin_macro=begin_macro,
            class_name=class_info.name,
            mark_type=mark_type,
            display_name=display_name,
            description=description,
            properties=class_info.properties
        )
