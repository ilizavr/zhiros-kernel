# zhir-kernel
ядро для жирОС

## лицензия:
- код ядра - открыт под лицензией GPLv3+
- код примеров модулей и библиотек для них, скриптов компиляции под лицензией MIT

## особенности:
1. single address space
2. безопасность на уровне компилятора(модуль zhir-vm). unix-совместимость через модуль unixbox(внутри unixbox используется MMU для изоляции)
3. максимальная модульность. человек сам должен выбирать что ему есть, система без его участия не будет жиреть
4. лаконичность и простота каждой подсистемы ядра
5. ядро может работать без модулей, предоставляя минималистичную консоль для отладки и тестирования модулей
6. консоль не отдельная подсистема, а просто программа для вызова функций линкера
7. кооперативный планировщик
8. минимальные обновления ядра. только багофиксы и минимальные согласованные дополнения, не ломающие совместимость

### причины такого подхода:
1. модульная архитектура для масштабируемости системы
2. SAS для максимальной производительности без оверхеда IPC
3. кооперативный планировщик намного стабильнее и проще вытесняющего. все его проблемы пропадают при использовании zhir-vm, который автоматически будет вставлять yield

### безопасность
модули являются доверенным кодом, как и в линукс. они имеют неограниченый доступ к системе. для недоверенного кода используется zhir-vm или unixbox

## компоненты ядра:
1. модуль взаимодействия с multiboot2 загрузчиком
2. текстовый видеодрайвер fbcon, который можно отключить через _fbcon_stop и реализовать свой драйвер
3. драйвер ps/2 клавиатуры
4. абстракции диска, разделов и vfs
5. минималистичная ФС ustar
6. динамический линковщик модулей
7. обработчик CPU exception. вывод паники с регистрами, стеком и консолью(частично готово)
8. консоль отладки и минимальный набор утилит: disasm, ls, lsmod, load_module(частично готово)
9. планировщик(не готов)

## API ядра для модулей и консоли:
### графика
```c
_print_color(char *string, u32 color) -> None
_printf(...) -> None
_clearframe() -> None
_fbcon_stop() -> fb_info* fb //отключается базовый графический драйвер для замены на кастомный
_getfb() -> fb_info* fb //получить fbinfo без отключения текстового графического драйвера
```
### клавиатура
```c
_get_key_state_matrix() -> bool key[256] //возвращает массив key[keycode]=status, где status=1 когда нажата и 0 когда отпущена
_gets(char* string, int max_size) -> int readed //чтение с клавиатуры для консольных приложений
_getchar() -> char
```
### работа с памятью
```c
_alloc(u32 size) -> void* buf
_alloc_aligned(u32 size, u32 allign) -> void* buf // не реализовано
_free(void* buf) -> bool success
_getfree() -> i_ptr freemem
```
### линковка модулей и ядра
```c
_get_linker_head() -> struct function_info *  //получить связный список функций линкера
_get_module_array() -> struct module_info *  //получить массив модулей
_resolve_function(char* name) -> void* function
_register_function(char *function_name, void* call, char *description) -> None
_replace_function(char *function_name, void* newfnc) -> bool success //переопределение функций в линкере
_hook_interrupt(u32 n, void* function) -> bool success //хук прерывания, для предотвращения конфликта модулей за прерывания
_load_mod(char diskletter, char* path) -> bool success
```
### таймер
```c
_sleep_ms(u32 ms) -> None
_getticks(u32 ms) -> u32 ticks
```
### абстракции дисков
```c
_diskadd(struct disk* dsk) -> bool success
_getdisk(int idx) -> struct disk*
```
### VFS
```c
_mount(struct disk* dsk, void * open_fnc, void* mkdir_function) -> bool success
_open(char diskletter, char* path) -> struct file*
_mkdir(char diskletter, char* path) -> struct file*
```
### многопоточность - не реализовано
```c
_yield() -> None
_get_tasks() -> struct task** tasks
_create_task(void* function) -> None
```

## Основные структуры ядра
```c
struct disk//эта же структура используется для разделов диска
{
    char *name;
    u32 size;//in sectors

    bool (*lba_read)(struct disk* dsk, u32 lba, char* buffer, u32 blocks);
    bool (*lba_write)(struct disk* dsk, u32 lba, char* buffer, u32 blocks);

    void* other_info;
    void* fs_info;
};

struct file // это также директория
{
    struct disk* dsk;
    char *path;
    bool is_dir;

    u32 (*read)(struct file* file, void* buffer, u32 size, u32 offset);// read от директории читает имяфайла\nимя2файла и тд. крч read от директории выглядит как ls
    u32 (*write)(struct file* file, void* buffer, u32 size, u32 offset);
    u32 (*getsize)(struct file* file); //размер файла

    bool (*close)(struct file* file);

    void* other_info;
};
```
остальные описаны в `zhirtypes.h`

## linker
linker хранит функции 2х типов - `fastcall`(имеют тип стандартный для C и префикс _ и не безопасны) и `zhirfunction`(для zhirvm безопасные)
zhirfunction - это функция, которая принимает массив объектов(zhirobjectarray) и возвращает zhirobject(универсальный объект, который хранит свой тип в своей структуре)

регистрация API-функций в `kernel.c`

регистрация шелл функций в `shell/shell.c`

модули имеют формат raw bin. при запуске к ним в первом аргументе передается указатель на функцию резольвера символов(resolve_symbol)
### ВАЖНО: 
модули компилируются с определенным набором флагов компиляции(смотрите пример компиляции в `build32.sh`). иногда бывают проблемы с массивами и функциями, которые вызываются из вне(добавляйте `static` при объявлении). желательно компилировать модули в elf формат, смотрите elf module loder

### пример кода модуля
```c
#include "lib/zhirtypes.h"
#include "lib/string.h"

void (*printf)(char* fmt, ...);

INIT void init(void* _resolve_function(char* name))
{
    printf = _resolve_function("_printf");

    printf("hello world from module!");
}
```

## набор модулей:
- zhirGL - ожидается оконный менеджер
- windowmanager - разработка ведется - https://github.com/TINERKOTL/zhiros-module-example
- mouse-driver - готов - NEUROSLOP - https://github.com/ilizavr/mouse-module
- zhir-vm(jit vm для memorysafe языка) - разработка ведется - https://github.com/bust6k/ZhirVM
- ide-driver, fat16 - скоро
- network - ожидается доработка ядра
- unixbox(слой совместимости для unix-подобных программ) - в планах
- elf module loader - решает проблемы raw модулей - скоро

## нейросети и вайбкодинг
- в ядре не должно быть ни строчки кода написанного ИИ.
- разработчикам модулей МОЖНО пользоваться нейросетями, но тогда модуль должен иметь пометку NEUROSLOP. 
- написание тестов и review кода с помощью нейросетей РАЗРЕШЕНО

<img width="1768" height="1286" alt="screen" src="https://github.com/user-attachments/assets/fe965596-a3c1-451b-b6fc-fabca75baf92" />
