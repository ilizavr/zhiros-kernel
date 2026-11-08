# ZhirVM
ZhriVM - виртуальная машина для языка ZhirScript. Вот и все

# Launch
Сначала нкжно скачать проект. SSH:
```
git clone git@github.com:bust6k/ZhirVM.git
```

Или HTTPS:
```
git clone https://github.com/bust6k/ZhirVM.git
```

запустить? сначала нужно отколотить. создать в ramdisk файл  vmeste_mi_edynyi.zo, там написать:
```python
print("Hello MicroPython,Now you earn new competitor for the point of the best ZhirOS's language!\n")
```

И запустить, зайдите в папку zhirvm(текущий репозиторий) и напишите:
```Makefile
make
```

Все запустится автоматом
