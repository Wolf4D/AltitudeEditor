# Полная спецификация форматов карт и структур игрового движка FPS Creator

**Автор:** Иван Клёнов (**Ivan Klenov**)  
**Проект:** FPS Creator 2D Map Viewer  
**Версия документации:** 1.0 (2026)  
**Целевые движки:** FPS Creator V1, V1.04, V1.07, V1.18 Classic, FPS Creator X10, WASP Branch

---

## 1. Введение и общая архитектура

Игровой движок **FPS Creator (FPSC)**, разработанный компанией *The Game Creators* под руководством Ли Бэмбера (Lee Bamber) на базе **DarkBasic Pro**, использует модульную блочно-сеточную архитектуру построения уровней.

Уровень FPS Creator состоит из:
1. **3D-сетки сегментов (Segments Grid)**: модульные блоки стен, полов, потолков и коридоров фиксированного размера ($100 \times 100 \times 100$ единиц мира).
2. **Библиотеки сегментов (Segment Bank)**: список уникальных сегментов (`.fps`), используемых на карте.
3. **Объектов (Entities)**: интерактивные игровые сущности (персонажи, оружие, патроны, двери, декорации, зоны триггеров, источники света, маркер игрока), имеющие произвольные непрерывные координаты $(X, Y, Z)$ и ориентацию в пространстве.
4. **Вейпоинтов (Waypoints)**: графы путей патрулирования для искусственного интеллекта.
5. **Таблицы освещения (Lights Table)**: статические и динамические точечные источники света.

Все данные уровня упаковываются в единый пакет с расширением **`.fpm`** (*FPS Creator Map*).

---

## 2. Контейнер карты: формат `.FPM`

Файл `.fpm` представляет собой стандартный архив **ZIP (PKZIP 2.0)** со сжатием Deflate или Store.

### 2.1. Парольная защита
В большинстве версий редактора FPS Creator для защиты карт от прямого вскрытия обычными архиваторами используется стандартный встроенный пароль ZIP:
```text
mypassword
```
*(Примечание: некоторые мод-паки или пользовательские сборки могут сохранять файлы без пароля)*.

### 2.2. Содержимое архива `.fpm`
Внутри архива располагается фиксированный набор файлов:

| Имя файла в архиве | Назначение | Формат |
| :--- | :--- | :--- |
| `map.fpm` / `map.fpmb` | 3D-сетка уровня и заголовки размеров | Текстовый или бинарный массив DarkBasic Pro |
| `map.seg` | Каталог типов сегментов, использованных в карте | Текстовый файл (список путей `.fps`) |
| `map.ele` | Список и свойства всех размещенных сущностей (Entities) | Бинарный формат переменной длины |
| `map.way` | Точки путей и графы маршрутов AI | Текстовый / бинарный файл |
| `map.lgt` | Таблица источников света | Бинарный / текстовый файл |
| `header.ini` *(опционально)* | Метаданные уровня (небо, туман, шейдеры) | INI-файл конфигурации |

---

## 3. Система координат движка и соответствие 2D-холсту

### 3.1. Размеры сетки
- Сетка уровня по умолчанию имеет размер **$41 \times 41$ ячеек** по горизонтали ($X \in [0..40]$, $Y_{\text{grid}} \in [0..40]$).
- По вертикали карта разбита на **21 этаж / слой (Layers)** ($L \in [0..20]$).
- Размер одной ячейки сетки (Tile): **$100 \times 100 \times 100$ единиц мира**.
- Общий размер уровня: $4100 \times 4100 \times 2100$ единиц.

### 3.2. Мировые 3D-координаты движка
В кодовой базе движка (*DarkBasic Pro* / `FPSC-Game.DBA`) координаты рассчитываются следующим образом:
- **Ось $X$**: направлена вправо ($X \ge 0$). Центр ячейки $x$:
  $$X_{\text{world}} = x \times 100 + 50$$
- **Ось $Y$ (Высота)**: направлена вверх ($Y \ge 0$). Центр этажа $L$:
  $$Y_{\text{world}} = L \times 100 + 50$$
- **Ось $Z$ (Глубина)**: в DarkBasic Pro ось $Z$ направлена вглубь со **знаком минус** ($Z \le 0$). Центр ячейки $y$:
  $$Z_{\text{world}} = -(y \times 100 + 50)$$

### 3.3. Проекция на 2D вид сверху (Top-Down Canvas)
Для отображения карты в 2D-виде (вид сверху, где $(0, 0)$ — верхний левый угол):
$$\text{Canvas } X = X_{\text{world}}$$
$$\text{Canvas } Y = -Z_{\text{world}}$$
$$\text{Этаж (Floor Layer)} = \left\lfloor \frac{Y_{\text{world}} + 25}{100} \right\rfloor$$

---

## 4. Сетка уровня: бинарный формат `map.fpmb`

Файл `map.fpmb` содержит состояние 3D-массива ячеек сетки.

### 4.1. Сериализация 3D-массивов в DarkBasic Pro
В DarkBasic Pro оператор `dim map(layermax, maxx, maxy)` выделяет 3-мерный массив, который сохраняется на диск в **Column-Major (Fortran) порядке**.

Структура файла `map.fpmb`:
1. `int32 headerCount` — заголовок (обычно 0).
2. `int32 totalCells` — общее число ячеек ($21 \times 41 \times 41 = 35\,301$).
3. Последовательность из 35 301 элементов по **8 байт** каждый:
   - `int32 cellIndex` (порядковый номер элемента);
   - `int32 mapid` (32-битное битовое поле данных ячейки).

### 4.2. Формула адресации индекса ячейки
Для ячейки с координатами $(\text{layer}, x, y)$ одномерный индекс $i$ в файле вычисляется по формуле:
$$i = \text{layer} + x \times (\text{layers}) + y \times (\text{layers} \times \text{cols})$$
где $\text{layers} = \text{layermax} + 1 = 21$, $\text{cols} = \text{maxx} + 1 = 41$.

И обратно, при чтении потока элементов $i = 0 \dots 35\,300$:
$$\text{layer} = i \pmod{21}$$
$$\text{rem} = \lfloor i / 21 \rfloor$$
$$x = \text{rem} \pmod{41}$$
$$y = \lfloor \text{rem} / 41 \rfloor$$

### 4.3. Распаковка битового поля `mapid` (Bitfield Layout)
Значение `mapid` (DWORD) кодирует тип размещенного сегмента, его вращение и ориентацию:

| Биты | Имя в коде движка | Описание |
| :--- | :--- | :--- |
| **31 .. 20** (12 бит) | `segId` | 1-based индекс сегмента в `map.seg` (`0` = пусто) |
| **19 .. 16** (4 бита) | `scaler` | Масштабирование высоты блока |
| **15 .. 14** (2 бита) | `ground` | Привязка к полу/потолку |
| **13 .. 12** (2 бита) | `rotation` | Угол поворота блока ($0 = 0^\circ, 1 = 90^\circ, 2 = 180^\circ, 3 = 270^\circ$) |
| **11 .. 10** (2 бита) | `orient` | Флаг зеркалирования / ориентации |
| **9 .. 4** (6 бит) | `symbol` | Идентификатор специального символа маркера |
| **3 .. 0** (4 бита) | `flags` | Дополнительные флаги отрисовки |

**Формулы извлечения на C++:**
```cpp
uint32_t mapid = ...;
int segId    = (mapid >> 20) & 0x0FFF;
int scaler   = (mapid >> 16) & 0x000F;
int ground   = (mapid >> 14) & 0x0003;
int rotation = (mapid >> 12) & 0x0003;
int orient   = (mapid >> 10) & 0x0003;
int symbol   = (mapid >> 4)  & 0x003F;
```

---

## 5. Библиотека сегментов: формат `map.seg`

Файл `map.seg` — это текстовый файл со списком относительных путей к файлам описания сегментов (`.fps`), проиндексированных с единицы ($1, 2, 3 \dots$).

Пример содержимого:
```text
segments\scifi\rooms\corridora.fps
segments\scifi\doors\door_frame.fps
segments\ww2\scenery\armoury.fps
```

### 5.1. Структура файла описания сегмента `.FPS`
Файлы `.fps` представляют собой текстовые конфигурации:
```ini
; Segment Configuration File
desc          = Sci-Fi Corridor A
mesh          = meshbank\scifi\corridora.x
texture       = texturebank\scifi\corridora_D.dds
materialindex = 1
kind          = 0
```
Связанные текстуры:
- `_D.dds` / `_D.tga` — диффузная текстура (Diffuse).
- `_N.dds` / `_N.tga` — карта нормалей (Normal Bump Map).
- `_S.dds` / `_S.tga` — карта отражений (Specular).
- `_I.dds` / `_I.tga` — карта свечения (Illumination).

---

## 6. База сущностей уровня: бинарный формат `map.ele`

Файл `map.ele` хранит все динамические и статические сущности уровня. Формат развивался от версии к версии движка.

### 6.1. Заголовок файла
- `int32 version` — номер версии формата ($100 \dots 218$).
- `int32 count` — количество сохраненных элементов.

### 6.2. Чтение строк в DarkBasic Pro
Оператор `write string 1, a$` записывает строку с суффиксом **CRLF (`\r\n`)**.
При десериализации парсер должен сканировать байты до маркера `\r\n` (2 байта).

### 6.3. Побайтовая структура одной сущности (Entity Block)

#### Базовый блок (Версия 101):
1. `int32 mainType`
2. `int32 bankIndex` (1-based индекс профиля `.fpe`)
3. `int32 staticFlag` ($0$ = динамический, $1$ = статический)
4. `float x, y, z` (Мировые 3D координаты, 12 байт)
5. `float rx, ry, rz` (Углы вращения в градусах, 12 байт)
6. `string name$` (Имя сущности, CRLF)
7. `string aiInit$` (Скрипт инициализации, CRLF)
8. `string aiMain$` (Главный скрипт поведения, CRLF)
9. `string aiDestroy$` (Скрипт уничтожения, CRLF)
10. `int32 isObjective`
11. `string useKey$` (CRLF)
12. `string ifUsed$` (CRLF)
13. `string ifUsedNear$` (CRLF)
14. `int32 uniqueElement`
15. `string texD$` (Пользовательская текстура, CRLF)
16. `string texAltD$` (Альтернативная текстура, CRLF)
17. `string effect$` (Шейдер эффекта, CRLF)
18. `int32 transparency`
19. `int32 editorFixed`
20. `string soundSet$` (CRLF)
21. `string soundSet1$` (CRLF)
22. `int32[7] spawnParams` ($7 \times 4 = 28$ байт: `spawnmax`, `spawndelay`, `spawnqty`, `hurtfall`, `castshadow`, `reducetexture`, `speed`)
23. `string aiShoot$` (CRLF)
24. `string hasWeapon$` (CRLF)
25. `int32[4] liveSpawn` ($4 \times 4 = 16$ байт: `lives`, `spawn.max`, `spawn.delay`, `spawn.qty`)
26. `float[3] coneScale` ($3 \times 4 = 12$ байт: `scale`, `coneheight`, `coneangle`)
27. `int32[13] propsAndTrigger` ($13 \times 4 = 52$ байта: `strength`, `isimmobile`, `cantakeweapon`, `quantity`, `markerindex`, `light.color`, `light.range`, `areax1`, `areay1`, `areaz1`, `areax2`, `areay2`, `areaz2`)
28. `string baseDecal$` (CRLF)

#### Дополнительные блоки версий:
- **$\ge 102$**: **80 байт** (20 полей `int32`/`float`: `rateoffire`, `damage`, `accuracy`, `reloadqty`, `fireiterations`, `lifespan`, `throwspeed`, `throwangle`, `bounceqty`, `explodeonhit`, `weaponisammo`, `spawnupto`, `spawnafterdelay`, `spawnwhendead`, `spare1..6`).
- **$\ge 103$**: **36 байт** (9 полей `int32`: параметры физики Newton/ODE — `physics`, `phyweight`, `phyfriction`, `phyforcedamage`, `rotatethrow`, `explodable`, `explodedamage`, `phydw4`, `phydw5`).
- **$\ge 104$**: **4 байта** (`phyalways`).
- **$\ge 105$**: **24 байта** (6 полей: расширенный рандомизатор спавна).
- **$\ge 106$**: **8 байт** (`spawnatstart`, `spawnlife`).
- **$\ge 107$**: **4 байта** (`light.index` — индекс динамического источника).
- **$\ge 199$** *(FPS Creator X10)*: **68 байт** (17 полей расширенного ИИ).
- **$\ge 200$** *(FPS Creator X10)*: **24 байта** (6 полей расширенной физики).
- **$\ge 217$** *(FPS Creator V1.18)*: **68 байт** (17 полей управления эмиттерами частиц).
- **$\ge 218$** *(FPS Creator V1.18 Final)*: **4 байта** (`particle.animated`).

---

## 7. Вейпоинты и маршруты патрулирования: `map.way`

Файл `map.way` описывает графы навигации AI.

- Содержит массив узловых точек $(\text{waypoint\_x}, \text{waypoint\_y}, \text{waypoint\_z})$.
- Каждая точка связывается в линейные или циклические последовательности (Sequences).
- В редакторе и 2D-просмотрщике линии между вейпоинтами одного маршрута отрисовываются пунктирными линиями со стрелками направления движения патруля.

---

## 8. Профили сущностей `.FPE` и прозрачность иконок

Каждая сущность ссылается на профиль `entitybank\...\*.fpe` (*FPS Creator Entity Profile*).

### 8.1. Формат `.FPE`
Текстовый конфигурационный файл с парами `key = value`:
```ini
; Saved by FPS Creator
desc          = Sci-Fi Door A
model         = meshbank\scifi\door_a.x
textured      = texturebank\scifi\door_a_D.dds
ischaracter   = 0
isweapon      = 0
health        = 100
speed         = 0
collisionmode = 1
defaultstatic = 0
ai_main       = defaultdoor.fpi
```

### 8.2. Превью-иконки и алгоритм прозрачности (Chroma-Key)
Рядом с каждым `.fpe` находится иконка предпросмотра `.bmp` ($64 \times 64$ пикселя, 24-bit RGB).
В оригинальном редакторе BMP не имеет альфа-канала, а фон является сплошным белым `RGB(255, 255, 255)` или черным `RGB(0, 0, 0)`.

**Алгоритм авто-хромакея в FPSC 2D Viewer:**
1. Сэмплируются 4 угловых пикселя $(0,0)$, $(W-1,0)$, $(0,H-1)$, $(W-1,H-1)$.
2. Если углы однородны и близки к фоновому цвету $(R_0, G_0, B_0)$:
   - Для каждого пикселя вычисляется максимальное цветовое расстояние:
     $$\Delta = \max(|R - R_0|, |G - G_0|, |B - B_0|)$$
   - Если $\Delta < 12 \implies \text{Alpha} = 0$ (полная прозрачность).
   - Если $12 \le \Delta < 28 \implies \text{Alpha} = \frac{\Delta - 12}{16} \times 255$ (плавное сглаживание контура).
   - Иначе $\text{Alpha} = 255$.

---

## 9. Анализ использования оперативной памяти (Memory Budget)

Так как движок FPS Creator собран как **32-битное приложение** на базе DirectX 9 и DarkBasic Pro, предельный лимит выделяемой памяти процесса составляет **$1.8 \dots 2.0\text{ ГБ}$**. Превышение этого лимита приводит к немедленному падению (`Runtime Error 7005: Out of Memory`).

### 9.1. Расчет памяти геометрии (Meshes RAM)
Для каждого уникального `.x` меша:
$$\text{RAM}_{\text{mesh}} \approx \text{Header} + (\text{Vertices} \times 32\text{ байта}) + (\text{Indices} \times 2\text{ байта})$$

### 9.2. Расчет памяти текстур (Textures VRAM)
- **Сжатые DDS DXT1**: $\frac{\text{Width} \times \text{Height}}{2}$ байт.
- **Сжатые DDS DXT3 / DXT5**: $\text{Width} \times \text{Height}$ байт.
- **Несжатые 32-bit TGA / BMP / PNG**: $\text{Width} \times \text{Height} \times 4$ байта.
- **Мип-мапы (Mipmaps)**: добавляют $+33.3\%$ к общему объему текстуры.

### 9.3. Расчет памяти аудио (Audio RAM)
- **WAV (PCM)**: несжатый размер в памяти ($\text{SampleRate} \times \text{Channels} \times \text{BytesPerSample} \times \text{Duration}$).
- **MP3 / OGG**: размер закодированного потока + буфер декодера DirectShow ($\sim 512\text{ КБ}$ на трек).

---

## 10. Заключение

Данная спецификация полностью покрывает все внутренние форматы данных FPS Creator и может использоваться как справочник при разработке новых инструментов, конвертеров, редакторов и движков, совместимых с классической экосистемой The Game Creators.
