; penter 안 쓰는데 참고자료로 남겨놓음


; MASA(Microsoft macro Assmebler)에서 주석은 세미콜론

COMMENT !
 여러줄 주석, !말고 다른 문자로 대체 가능(구분자 역할)
 !

; PROC: Procedure -> 함수
EXTERN PenterHook : PROC
; boolean 1Byte
EXTERN bIsCrashMode : BYTE

; 코드 영역 시작(.DATA, .STACK, .CODE)
.CODE

; _Penter함수 정의 시작(), 
; /Gh 옵션을 키면 컴파일러가 _penter함수를 무조건 호출하도록 함, 시스템 시멘틱임(C표준 아닌 MS에서만 쓰는 비표준 함수들에 _ 접두사를 붙인다고 함)
_penter PROC
