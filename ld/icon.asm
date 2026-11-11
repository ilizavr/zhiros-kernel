section .text.init
extern init
extern _text_end
extern _end
extern icon
jmp init

align 4
db "ZHIR"
dd 2 ; header version
dd _text_end
dd _end
dd icon
