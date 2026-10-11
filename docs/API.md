# API ядра для модулей и консоли:

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
_unload_mod(u32 idx) -> bool success
```
### таймер
```c
_sleep_ms(u32 ms) -> None
_getticks() -> u32 ticks
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
### многопоточность
```c
_yield() -> None
_get_tasks() -> struct task* tasks
_create_task(char * name, void* function) -> struct task* 
```

## структуры ядра описаны в `lib/zhirtypes.h`
