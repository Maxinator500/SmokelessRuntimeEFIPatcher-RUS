# SREP на Русском
Bloated версия утилиты SREP. Разработанная в 2022 году SmokelessCPUv2, программа устраняет ограничения традиционных модификаций BIOS, позволяя устанавливать временные патчи через загрузочный USB-накопитель, что полезно для разблокировки расширенных настроек в прошивке OEM-производителя.

# В чем разница в сравнении с оригиналом
* Появился Русский перевод для сообщений на экране и в логе, вывод в лог был приспособлен для кодировки Unicode. Сами сообщения учащены, чтобы прогресс выполнения лучше отслеживался, и у пользователя не возникало мысли что программа зависла.
  </br>Переключение на Анг доступно через аргумент ENG (например, "SREP.efi ENG"). Но, как можно догадаться, тогда вызов SREP нужно осуществить через командную строку в Shell. Разрешается передавать параметр ENG в .nsh.
* Файл конфига больше не обязан иметь название 'SREP_Config.cfg'. Теперь патчер выбирает в качестве конфига первый найденный файл с расширением '.cfg'.
* Добавил 11 новых команд: NonamePE, NonameTE, HandleIndex, LoadGUIDandSavePE, LoadGUIDandSaveFreeform, UninstallProtocol, Compatibility, UpdateHiiPackage, HiiForceLoad, SendForm, Skip.
  </br>В каких случаях могут быть полезны - далее.
* Добавил поддержку абстрактных знаков (regex) для использования в строке шаблона. Классы знаков доступны [по ссылке](https://gist.github.com/kaigouthro/e8bad6a2c8df6ff13b8716027a172dc0#3-character-types).
* По умолчанию, комбинация Patch - Pattern теперь заменяет все вхождения, а не только первое, которые соответствуют указанному шаблону.
  </br>Количество мест к нахождению с использованием Patch теперь может быть настроена на определённое число для одного шаблона. Прогресс поиска отображается на экране в виде полосы загрузки.
  </br>Старую реализацию Patch выделил в отдельную команду FastPatch.
  </br>При неверном запуске SREP, Op Patch превращается в Op FastPatch и абстрактные знаки в шаблоне приводят к преждевременному выходу из программы.
* Добавил игнорирование строк конфига включащие символ решетки "#". Применимо для написания комментариев в файле.
  <details>
  <summary><strong>Пример комментариев</strong></summary>
  # Здесь выбираем FilterProtocol</br>
  Op Compatibility</br>
  389F751F-1838-4388-8390-CD8154BD27F8</br>
  </details>

# Как использовать
1. Скомпильте исп. файл сами или загрузите со страницы релизов. Текущая версия - 0.2.x.
2. Распакуйте файл на накопитель и сделайте его загрузочным.
3. Создайте новый или скопируйте готовый конфиг в корень.
4. Запустите.

# Синтаксис для каждой из новых команд (с примерами)
Операция в "<...>" опциональна. Некоторые команды не требуют доп. параметров для работы (например, Exec).

    Op Команда
        <GUID или Cтрока или Число>
        # Команды LoadGUIDandSaveFreeform и UpdateHiiPackage поддерживают до 2 "параметров".
        # В зависимости от наличия второго, выбирается режим работы.
        <GUID или Cтрока или Число>
    <Op Patch> или <Op FastPatch> 
        Аргумент 1
        Аргумент 2
        Аргумент 3
    <Op End>
    
    # Если драйвер ещё не в памяти.
    <Op Exec>

### Значение

    Команда : NonamePE, Patch, LoadGUIDandSavePE, UninstallProtocol, Compatibility и остальные
    GUID : GUID драйвера или протокола для поиска
    Строка : Название модуля из UI Section
    Число : Кол-во строк к пропуску для Op Skip, или EFI_HANDLE ID для Op HandleIndex, или пользовательский размер для UpdateHiiPackage
    Аргумент 1 : Offset, Pattern, RelNegOffset, RelPosOffset для Patch
    Аргумент 2 : Модификатор для Аргумент 1 (например, HEX шаблон)
    Аргумент 3 : HEX патч
    
# Добавленные команды
## LoadGUIDandSavePE, LoadGUIDandSaveFreeform
Загружают PE, RAW, FREEFORM секции модуля из Firmware Volume по GUID. Применимо когда у модуля нет имени, а Op LoadFromFV не использовать. Редко для патчей, так как в память загружается вторая копия модуля.
</br>Т.е. первая команда нужна для особых случаев когда у App, запуск которого инициирует вход в биос, нет секции UI.
</br>Что делает команды менее бесполезными, это сохранение секции модуля как файл на флешку. Может выручить когда ни один способ дампа не работает.
</br>Формат GUID как в UEFITool.
  <details>
  <summary><strong>LoadGUIDandSavePE</strong></summary>
    
  ```
  Op LoadGUIDandSavePE
  
  # Это SetupUtility
  FE3542FE-C1D3-4EF8-657C-8048606FF670
  ```

  </details>
  <details>
  <summary><strong>LoadGUIDandSaveFreeform</strong></summary>
    
  ```
  Op LoadGUIDandSaveFreeform
  
  # Это SmallLogo с section subtype RAW. GUID от File.
  63819805-67BB-46EF-AA8D-1524A19A01E4


  Op LoadGUIDandSaveFreeform
  
  # Это setupdata. У section subtype FREEFORM есть свой GUID, его тоже нужно указать, даже если они одинаковы.
  FE612B72-203C-47B1-8560-A66D946EB371
  FE612B72-203C-47B1-8560-A66D946EB371
  ```

  </details>

## NonamePE, NonameTE
Используют одну и ту-же функцию, с различием в лишь одном передаваемом значении.

* NonamePE - ищет секцию PE по GUID;
* NonameTE - ищет секцию TE по GUID.
</br>Синтаксис как у LoadGUIDandSavePE, но у этих команд область поиска RAM, вместо FV.

Работает таким образом, что сначала находятся все дескрипторы с поддержкой LoadedImageProtocol. Затем путь FvFile каждого модуля по порядку сравнивается с указанным в конфиге, а при совпадении цикл сравнения останавливается, функция возвращает ImageInfo модуля на котором сравнение дало успех.

## HandleIndex
Находит загруженные дескрипторы в RAM по их ID из handle dump, который можно получить через команды UEFI shell, а также в SREP.log при первом запуске HandleIndex со случайным численным параметром.

## UninstallProtocol
Находит все дескрипторы с заданным протоколом и по очереди удаляет из них этот протокол.
В случае провала, укажет порядковый номер дескриптора в буфере, у которого протокол удалить не удалось.

## Compatibility
Дает возможность задать фильтрующий протокол во всех функциях поиска. Если указанный GUID уникален, то программа не отвергает его, но на экран выдаются рекомендуемые.
  <details>
  <summary><strong>Выдача рекомендуемых</strong></summary>
    
  ```
  Recommended protocols are:
  EFI_FIRMWARE_VOLUME_PROTOCOL_GUID(good for HP Insyde Rev.3)
  389F751F-1838-4388-8390-CD8154BD27F8
   
  EFI_LEGACY_BIOS_PROTOCOL_GUID(good for Aptio 4, Insyde Rev.3)
  DB9A1E3D-45CB-4ABB-853B-E5387FDB2E2D
  ```
  
  </details>
Если Compatibility не использована вообще, SREP работает как обычно. Таким образом организована поддержка Insyde Rev. 3.

## HiiForceLoad
Использует результат поиска NonamePE, или HandleIndex, или LoadFromFS, или другой команды для получения информации о смещении данных HII внутри дескриптора, чтобы построить и загрузить в HII Database новый PackageList. Если в дескрипторе есть больше одной структуры Form Set, то при построении используется лишь самый первый Form Set, и самый первый Language Package. Не принимает дополнительных аргументов.

## SendForm
Работает в паре с HiiForceLoad для вызова метода SendForm у протокола FormBrowser2. Если этот метод поддерживается реализацией FormBrowser2 устройства, происходит запуск BIOS Setup для этого меню.
  <details>
  <summary><strong>HiiForceLoad + SendForm</strong></summary>
    
  ```
  # Получение ImageInfo
  Op LoadFromFS
  AodSetup.efi

  # Построение и публикация PackageList на основе AodSetup
  Op HiiForceLoad

  # Использование PackageList от HiiForceLoad
  Op SendForm
  ```
  
  </details>

## UpdateHiiPackage
Позволяет найти начало HII данных после EFI_HII_PACKAGE_LIST_HEADER, которые выгружены в HII Database у определенного модуля.
  <details>
  <summary><strong>UpdateHiiPackage</strong></summary>
    
  ```
  Op UpdateHiiPackage
  
  # Это Setup package в HII Database.
  899407D7-99FE-43D8-9A21-79EC328CAC21
  
  # Это переназначение размера package для последующих операций. 
  A000000
  ```

  </details>

## Skip
Дает возможность пропускать указанное число команд (считаются только Op), если прошлая завершилась с успехом. Можно создавать простые логические цепочки.
  <details>
  <summary><strong>Skip</strong></summary>
    
  ```
  Op LoadGUIDandSavePE
  # SetupUtility
  FE3542FE-C1D3-4EF8-657C-8048606FF670
  Op Skip
  # Пропустить секцию конфига для систем Aptio
  4
  
  Op LoadGUIDandSavePE
  # EsaFull
  A2DF5376-C2ED-49C0-90FF-8B173B0FD066
  Op LoadGUIDandSavePE
  # AMITSE
  B1DA0ADF-4F77-4070-A88E-BFFE1C60529A
  Op LoadGUIDandSavePE
  # Setup
  899407D7-99FE-43D8-9A21-79EC328CAC21
  Op Skip
  # Это система Aptio, пропустить всё до конца файла
  32
  
  ...
  ```

  </details>
  
## Op Patch <ins>с параметром</ins>
С версии 0.2.3, добавление десятичного числа в строку вызова команды изменит поведение аргумента Pattern. Число означает лимит на поиск вхождений шаблона.
  <details>
  <summary><strong>Принимается значение в пределах от 0 до 9</strong></summary>
    
  ```
  Op Loaded

  # Setup
  899407D7-99FE-43D8-9A21-79EC328CAC21

  # Установить лимит в 5 вхождений для Pattern
  Op Patch 5
  Pattern

  # Какой-то шаблон
  290201861.27
  ```

  </details>
Помогает ускорить выполнение конфига, когда меры по улучшению шаблона поиска не эффективны, оставляя выражение недостаточно узким.

# Todos

    [x] Regex Matching
    [x] Batch Replacement
    [x] Uninstall Protocol
    [x] Insyde Rev. 3 Support
    [x] Rework on-screen and log outputs (make separate IFR package for each language)
    [ ] Improve various algos
    [ ] Write documentation

Версии 0.1.6 и 0.1.7 удалены со страницы релизов из-за бага. Он заключался в том, что перед каждой следующей командой патча (Op Patch), массив хранящий смещения найденных вхождений шаблона не очищался. Поэтому самый последний "Op Patch" всегда заменял абсолютно все вхождения, которые были найдены за сеанс работы SREP.
Релиз 0.2.1 Удален из-за бага Op Patch, при котором абстрактные символы в строке шаблона рассматривались как неподдерживаемые.