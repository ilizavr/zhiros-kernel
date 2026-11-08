#include "../lib/zhirapi.h"
#include "../lib/string.h"

#define MAX_HISTORY 10

static char command_history[MAX_HISTORY][256];
static int history_count = 0;

#define KEY_UP    0x18
#define KEY_DOWN  0x19
#define KEY_LEFT  0x1B
#define KEY_RIGHT 0x1A

static u32 custom_gets(char *str, u32 maxlen)
{
    u32 len = 0;     // Реальная длина строки
    u32 cursor = 0;  // Текущая позиция курсора в строке
    int current_hist = history_count;

    str[0] = '\0';

    while (len < (maxlen - 1))
    {
        char chr = getchar();

        // 1. Enter
        if (chr == '\n' || chr == '\r') {
            printf("\n");
            break;
        }

        // 2. Backspace
        if (chr == '\b' || chr == 127)
        {
            if (cursor > 0) {
                if (cursor == len) {
                    // Удаление в самом конце строки (как было)
                    printf("\b  \b\b");
                    len--;
                    cursor--;
                    str[len] = '\0';
                } else {
                    // Удаление в середине строки
                    // Сдвигаем символы влево
                    for (u32 j = cursor - 1; j < len - 1; j++) {
                        str[j] = str[j + 1];
                    }
                    len--;
                    cursor--;
                    str[len] = '\0';

                    // Перерисовываем хвост строки на экране
                    printf("\b");
                    for (u32 j = cursor; j < len; j++) {
                        printf("%c", str[j]);
                    }
                    printf(" "); // Стираем остаток старого символа
                    // Возвращаем терминальный курсор на позицию курсора
                    for (u32 j = cursor; j <= len; j++) {
                        printf("\b");
                    }
                }
            }
            continue;
        }

        // 3. Стрелка ВВЕРХ (История назад)
        if (chr == KEY_UP) {
            if (history_count > 0 && current_hist > 0) {
                current_hist--;

                // Возвращаем терминальный курсор в начало строки ввода
                while (cursor > 0) {
                    printf("\b");
                    cursor--;
                }
                // Стираем старый текст пробелами
                for (u32 j = 0; j < (len+1); j++) printf(" ");
                for (u32 j = 0; j < (len+1); j++) printf("\b");

                // Загружаем историю
                strcpy(str, command_history[current_hist]);
                len = strlen(str);
                cursor = len;
                printf("%s", str);
            }
            continue;
        }
        // 4. Стрелка ВНИЗ (История вперед)
        else if (chr == KEY_DOWN) {
            if (history_count > 0) {
                current_hist++;

                while (cursor > 0) {
                    printf("\b");
                    cursor--;
                }
                for (u32 j = 0; j < (len+1); j++) printf(" ");
                for (u32 j = 0; j < (len+1); j++) printf("\b");


                if (current_hist >= history_count) {
                    current_hist = history_count;
                    str[0] = '\0';
                    len = 0;
                    cursor = 0;
                } else {
                    strcpy(str, command_history[current_hist]);
                    len = strlen(str);
                    cursor = len;
                    printf("%s", str);
                }
            }
            continue;
        }
        // 5. Стрелка ВЛЕВО
        else if (chr == KEY_LEFT) {
            if (cursor > 0) {
                printf("%c\b\b",str[cursor]);//фикс курсора
                cursor--;
            }
            continue;
        }
        // 6. Стрелка ВПРАВО
        else if (chr == KEY_RIGHT) {
            if (cursor < len) {
                printf("%c", str[cursor]); // Печатаем символ справа, продвигая визуальный курсор
                cursor++;
            }
            continue;
        }
        // 7. Обычные печатные символы
        if (chr >= 32 && chr < 127) {
            if (cursor == len) {
                // Ввод в конце строки
                printf("%c", chr);
                str[cursor++] = chr;
                len++;
                str[len] = '\0';
            } else {
                // Ввод в середине строки (вставка)
                for (int j = len; j > (int)cursor; j--) {
                    str[j] = str[j - 1];
                }
                str[cursor] = chr;
                len++;
                str[len] = '\0';

                // Перерисовываем оставшуюся часть строки
                for (u32 j = cursor; j < len; j++) {
                    printf("%c", str[j]);
                }
                // Возвращаем курсор на правильную позицию (сразу после вставленного символа)
                for (u32 j = cursor + 1; j < len; j++) {
                    printf("\b");
                }
                cursor++;
            }
        }
    }

    str[len] = '\0';

    if (!strlen(str)) {
        return len;
    }

    // Сохранение в историю
    if (history_count < MAX_HISTORY) {
        strcpy(command_history[history_count], str);
        history_count++;
    } else {
        for (int h = 0; h < MAX_HISTORY - 1; h++) {
            strcpy(command_history[h], command_history[h + 1]);
        }
        strcpy(command_history[MAX_HISTORY - 1], str);
    }

    return len;
}

#define COL_RESET  0xAAAAAAu  /* дефолтный цвет консоли (сброс)      */
#define COL_PLAIN  0xE8E8E8u  /* обычные идентификаторы              */
#define COL_KW     0xFFAA00u  /* ключевые слова — оранжевый          */
#define COL_TYPE   0x56C9E8u  /* типы u32/fp/str — голубой           */
#define COL_BUILT  0xC792EAu  /* встроенные функции — сиреневый      */
#define COL_FUNC   0x82AAFFu  /* имена функций — синий               */
#define COL_STR    0x8CE99Au  /* строки — зелёный                    */
#define COL_NUM    0xFFD866u  /* числа — жёлтый                      */
#define COL_COM    0x707070u  /* комментарии — серый                 */
#define COL_OP     0xFF7EB6u  /* операторы == && + — розовый         */
#define COL_PUNCT  0x8C8C8Cu  /* скобки/запятые/двоеточия — тусклый  */

#define TOK_MAX 192

enum { LANG_PY, LANG_ZHIR };

/* -------------------- словари языков ----------------------- */
static const char* py_keywords[] = {
    "def","return","if","elif","else","for","while","in","import","from",
    "as","class","pass","break","continue","lambda","yield","with","try",
    "except","finally","raise","global","del","not","and","or","is",
    "None","True","False", 0
};

static const char* py_builtins[] = {
    "print","range","len","int","float","str","chr","ord","sum","abs",
    "min","max","enumerate","zip","list","dict","set","tuple","type",
    "bool","input","open", 0
};

static const char* zs_keywords[] = {
    "udf","ret","var","whl","if","else","and","or","not",
    "brk","cnt","loop", 0
};

static const char* zs_types[] = {
    "u8","u16","u32","u64","i8","i16","i32","i64",
    "fp","f32","f64","str","bool","void", 0
};

static const char* zs_builtins[] = {
    "println","printf","len","type","ord","chr","int","float","str", 0
};

/* -------------------- мелкие помощники --------------------- */
static int is_alpha(char c){ return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'; }
static int is_digit(char c){ return c>='0'&&c<='9'; }
static int is_ident(char c){ return is_alpha(c)||is_digit(c); }

static int in_list(const char* w, const char** list){
    unsigned int i;
    for (i = 0; list[i]; i++)
        if (strcmp(w, list[i]) == 0) return 1;
        return 0;
}

/* %C печатаем только когда цвет реально поменялся */
static void set_col(unsigned int c, unsigned int* cur){
    if (*cur != c){ printf("%C", c); *cur = c; }
}

/* по характерным словам определяем язык фрагмента */
static int detect_lang(const char* t){
    if (strstr(t, "udf ") || strstr(t, "whl ") || strstr(t, "var u32") ||
        strstr(t, "var fp") || strstr(t, "println("))
        return LANG_ZHIR;
    if (strstr(t, "def ") || strstr(t, "elif") || strstr(t, "import ") ||
        strstr(t, "self") || strstr(t, "__"))
        return LANG_PY;
    return LANG_ZHIR; /* язык ОС по умолчанию */
}

/* --------------------- сам подсветчик ---------------------- */
static void highlight_run(const char* p, int lang){
    char tok[TOK_MAX];
    unsigned int cur = 0xFFFFFFFFu; /* чтобы первый %C точно сработал */
    unsigned int col;
    int n;

    while (*p){
        char c = *p;

        /* перевод строки: цвет сбрасываем "лениво",
         * следующий токен сам выставит свой цвет */
        if (c == '\n'){ printf("%c", '\n'); p++; cur = COL_RESET; continue; }

        /* пробелы/табы — как есть, без цвета */
        if (c == ' ' || c == '\t' || c == '\r'){
            n = 0;
            while (*p == ' ' || *p == '\t' || *p == '\r'){
                if (n < TOK_MAX-1) tok[n++] = *p;
                p++;
            }
            tok[n] = 0; printf("%s", tok);
            continue;
        }

        /* комментарии: '#' у обоих языков, плюс '//' у zhirscript */
        if (c == '#' || (c == '/' && p[1] == '/')){
            set_col(COL_COM, &cur);
            n = 0;
            while (*p && *p != '\n'){
                if (n < TOK_MAX-1) tok[n++] = *p;
                p++;
            }
            tok[n] = 0; printf("%s", tok);
            continue;
        }

        /* строковые литералы "..." и '...' (учёт экранирования \x) */
        if (c == '"' || c == '\''){
            char q = c;
            set_col(COL_STR, &cur);
            n = 0; tok[n++] = c; p++;
            while (*p && *p != '\n' && *p != q){
                if (n < TOK_MAX-1) tok[n++] = *p;
                if (*p == '\\' && p[1] && p[1] != '\n'){
                    p++;
                    if (n < TOK_MAX-1) tok[n++] = *p;
                }
                p++;
            }
            if (*p == q){ if (n < TOK_MAX-1) tok[n++] = q; p++; }
            tok[n] = 0; printf("%s", tok);
            continue;
        }

        /* числа: 0, 65, 0x1F, 1.5 */
        if (is_digit(c) || (c == '.' && is_digit(p[1]))){
            set_col(COL_NUM, &cur);
            n = 0;
            while (is_ident(*p) || *p == '.'){
                if (n < TOK_MAX-1) tok[n++] = *p;
                p++;
            }
            tok[n] = 0; printf("%s", tok);
            continue;
        }

        /* идентификаторы: ключевые слова / типы / встроенные
         * функции / имена функций / обычные имена */
        if (is_alpha(c)){
            const char* q;
            n = 0;
            while (is_ident(*p)){
                if (n < TOK_MAX-1) tok[n++] = *p;
                p++;
            }
            tok[n] = 0;

            if (lang == LANG_PY){
                if (in_list(tok, py_keywords))      col = COL_KW;
                else if (in_list(tok, py_builtins)) col = COL_BUILT;
                else                                col = COL_PLAIN;
            } else {
                if (in_list(tok, zs_keywords))      col = COL_KW;
                else if (in_list(tok, zs_types))    col = COL_TYPE;
                else if (in_list(tok, zs_builtins)) col = COL_BUILT;
                else                                col = COL_PLAIN;
            }
            /* обычный идентификатор, после которого стоит '(' —
             * это имя функции */
            if (col == COL_PLAIN){
                q = p;
                while (*q == ' ' || *q == '\t') q++;
                if (*q == '(') col = COL_FUNC;
            }
            set_col(col, &cur);
            printf("%s", tok);
            continue;
        }

        /* операторы: жадно глотаем серии ==, <=, &&, -> */
        if (c && strchr("+-*/%=<>!&|^~", c)){
            set_col(COL_OP, &cur);
            n = 0;
            while (*p && strchr("+-*/%=<>!&|^~", *p)){
                if (n < TOK_MAX-1) tok[n++] = *p;
                p++;
            }
            tok[n] = 0; printf("%s", tok);
            continue;
        }

        /* скобки, запятые, двоеточия, точки */
        if (strchr("()[]{},;.:?", c)){
            set_col(COL_PUNCT, &cur);
            printf("%c", c);
            p++;
            continue;
        }

        /* всё остальное — без классификации */
        set_col(COL_PLAIN, &cur);
        printf("%c", c);
        p++;
    }

    /* вернуть терминалу дефолтный цвет (0xAAAAAA, как в примере) */
    set_col(COL_RESET, &cur);
}

static void printfile_syntax(char diskletter, char* name)
{
    struct file* f = open(diskletter,name);
    if(!f)
    {
        LOGE("file %s not found", name);
        return;
    }

    int size = f->getsize(f);
    if(size>65536)
    {
        LOGW("file is big. continue?");
        char chr = getchar();
        if(chr!='y'){
            if(f->close) f->close(f);
            return;
        }
    }
    char *buffer = alloc(size+1);
    f->read(f,buffer,size,0);
    buffer[size] = 0;

    highlight_run(buffer, detect_lang(buffer));

    if(f->close)f->close(f);
    free(buffer);
}

INIT void init(void* res)
{
    api_init(res);
    register_function("_custom_gets",custom_gets,"_custom_gets(char* buffer, u32 maxlen)");
    register_function("_pfs",printfile_syntax,"print file with syntax. use _pfs;c:diskletter;name");

}
