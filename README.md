# ЛОШ 2024 Стратегия

Программы точно работают на ОС Linux, нужен компилятор g++ версии не ниже 13.

Для запуска программ в Windows выполните инструкции из раздела **Установка WSL**. Если у вас ОС Linux, установите g++ версии хотя бы 13 и переходите к пункту **Установка утилит**.

Если у вас Mac, то ~~страдайте~~ подходите к преподавателям. Возможно, с вашей помощью (и вашим компьютером) мы справимся составить нормальную инструкцию.

## Установка WSL

Пуск -> поиск -> включение или отключение компонентов Windows -> выбрать галочки Подсистема Windows для Linux и Платформа виртуальной машины -> ОК -> Перезагрузить сейчас

Microsoft Store -> Ubuntu 24.04 LTS - Install

В PowerShell выполнить wsl.exe --install, затем wsl.exe --update

Из меню Пуск запустить Ubuntu 24.04 LTS, выполнить установку

## Установка утилит

### Выполнить команду:

```bash
sudo apt install gcc g++ make python3 python3-tk git && git clone "https://github.com/Semen-prog/losh-2024-strategy" && cd losh-2024-strategy && make && mv import_strategy.sh binaries && chmod u+x ./binaries/import_strategy.sh && cd binaries && clear && echo "Please execute ./import_strategy"
```

Скрипт *import_strategy.sh* создаёт ссылку на файл, путь до которого вы укажете. Копирование не производится, вы можете менять исходный файл без повторного вызова команды.

## Использование

В папке *binaries* лежат скомпилированные файлы интерактора, валидатора и генератора исходных полей. Использование программ:

### Интерактор

```bash
./interactor field.txt log.txt validate strategy1 strategy2 [strategy3 ...]
```

- *field.txt* - путь до файла с исходным полем;
- *log.txt* путь до файла, в который производится запись лога игры (для последующей визуализации);
- *validate* - путь до скомпилированного фалйа валидатора
- *strategy1, strategy2, ...* - пути до скомпилированных файлов стратегий участников.

Если вы хотите вручную изменить ограничение по времени на ход (локально у себя для дебага!), поменяйте значение переменной **STEP_USEC** в шапке файла *sources/interactor.c*, а затем выполните команду *make* в корне репозитория.

Для включения дебаг-вывода (например, для вывода полного лога вазимодействия) интерактора надо выполнить в корневой папке репозитория `make debug`. Для дальнейшего запуска без дебаг-вывода надо выполнить `make clean`, затем `make`.

Если вы хотите запустить интерактор с ограничениями, идентичными ограниччениям турнира стратегий, запускайте интерактор командой `SECURE=1 ./interactor ...` (т.е. в начало строки запуска интерактора надо добавить `SECURE=1`). Способ не работает для Mac OS.

### Генератор поля

```bash
./fieldgen t n p k [seed [prob]]
```

*t, n, p, k* - параметры из условия.

*seed* - значение *seed* генератора случайных чисел. Если не передать seed, используется значение seed по умолчанию.

*prob* - вероятность в процентах генерации стены. *prob* можно указывать только при указанном *seed*. *prob* может принимать целые значения от 0 до 50 (ероятность в процентах). Если *prob* не указан, используется значение по умолчанию - 15%.

### Валидатор

`validate` - программа валидатора, предусмотрена только для использования вместе с интерактором.

### Визуализатор

```bash
./visualize log.txt msec font wait
```
*log.txt* - путь до файла лога игры. Файл создаётся интерактором, см. использование интерактора (*log.txt*);

*msec* - интервал в миллисекундах между соседними шагами визализации;

*font* - размер шрифта;

*wait* - интервал в миллисекундах перед началом визуализации.
