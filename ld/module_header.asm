section .text.init
extern init
extern _text_end
extern _end
jmp init

align 4
db "ZHIR"
dd 1 ; header version
dd _text_end
dd _end
