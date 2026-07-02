# csdecomp

Декомпилятор C#/.NET сборок, написанный на C++.

`csdecomp` читает PE/CLI-файлы (`.dll`, `.exe`), разбирает метаданные ECMA-335 и восстанавливает исходный код C# из IL (Intermediate Language).

## Возможности

- Парсинг PE-образов и CLI-заголовков (.NET assembly)
- Чтение метаданных: типы, поля, методы, параметры
- Декодирование сигнатур методов и полей
- Дизассемблирование IL и восстановление C# через стековую машину
- Поддержка арифметики, вызовов, ветвлений, полей, массивов, строк
- CLI-утилита с выводом в stdout или файл

## Сборка

```bash
cmake -S . -B build
cmake --build build
```

На Windows с Visual Studio или MinGW:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Готовый `csdecomp.exe` для Windows можно скачать в [Releases](https://github.com/capyba099/RepoCheat-/releases).

## Использование

```bash
# Linux
./build/csdecomp path/to/assembly.dll

# Windows
csdecomp.exe path\to\assembly.dll
csdecomp.exe path\to\assembly.dll -o output.cs
csdecomp.exe path\to\assembly.dll --il-comments
```

## Тестовый пример

```bash
# Собрать пример на C#
dotnet build samples/SampleApp/SampleApp.csproj -c Release

# Декомпилировать
./build/csdecomp samples/SampleApp/bin/Release/net8.0/SampleApp.dll -o decompiled.cs
```

## Архитектура

```
┌─────────────┐    ┌──────────────┐    ┌───────────────┐    ┌────────────┐
│  PeReader   │───▶│ CliMetadata  │───▶│   IlReader    │───▶│ Decompiler │
│  (PE/CLI)   │    │  (таблицы)   │    │  (опкоды IL)  │    │  (C# код)  │
└─────────────┘    └──────────────┘    └───────────────┘    └────────────┘
```

| Модуль | Описание |
|--------|----------|
| `binary_reader` | Побайтовое чтение с little-endian |
| `pe_reader` | DOS/PE заголовки, секции, CLI directory |
| `cli_metadata` | Потоки `#~`, `#Strings`, `#Blob`, `#US` |
| `il_opcode` / `il_reader` | Таблица опкодов и разбор тел методов |
| `decompiler` | Стековая декомпиляция IL → C# |

## Ограничения

Это образовательная реализация, не полный аналог ILSpy/dnSpy:

- Нет полной поддержки generics, async/await, LINQ
- Сложный control flow выводится через `goto`
- Не восстанавливаются атрибуты и debug-символы
- Ограниченная поддержка вызовов (количество аргументов эвристическое)

## Лицензия

MIT
