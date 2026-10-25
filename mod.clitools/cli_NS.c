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

INIT void init(void* res)
{
    api_init(res);
    register_function("_custom_gets",custom_gets,"_custom_gets(char* buffer, u32 maxlen)");
}
