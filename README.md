# Ondra — C++11

Nativní emulátor počítače **Ondra SPO 186**, převedený z
[omikron88/jondra](https://github.com/omikron88/jondra). Procesor emuluje
[redcode/Z80](https://github.com/redcode/Z80), obraz, klávesnici a zvuk zajišťuje
SDL2. Java ani JVM nejsou potřeba. Vlastní kód používá výhradně C++11,
procesorové jádro je v C99.

## Sestavení v Linuxu

Na Debianu/Ubuntu:

```sh
sudo apt install build-essential cmake ninja-build libsdl2-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/ondra
```

CMake musí být alespoň 3.16. Na jiných distribucích stačí odpovídající balíčky
C/C++ překladače, CMake a vývojové SDL2. Procesor Z80 a jeho jediná závislost
Zeta jsou součástí zdrojových kódů v `third_party`; sestavení s nainstalovaným
SDL2 nepotřebuje připojení k síti ani Git submoduly.

Alternativně lze SDL2 stáhnout a sestavit z připnuté, kontrolním součtem ověřené
verze:

```sh
cmake -S . -B build -DONDRA_FETCH_SDL2=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
```

Pro skutečné okno a zvuk musí mít sestavované SDL2 k dispozici vývojové závislosti
X11/Wayland a zvukového systému. Proto je v Linuxu doporučen distribuční balíček
`libsdl2-dev`. Ovladače dostupné ve staženém SDL vypíše CMake při konfiguraci.

## Windows / MSYS2

Nainstalujte [MSYS2](https://www.msys2.org/) a otevřete terminál **MSYS2 UCRT64**.
Aktualizujte jej podle pokynů MSYS2 a pak spusťte:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-SDL2
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/ondra.exe
```

Použijte CMake a GCC ze stejného prostředí UCRT64. Pro MINGW64 použijte všechny
balíčky s prefixem `mingw-w64-x86_64-` v terminálu MINGW64. Nekombinujte je
s balíčky UCRT64 nebo s MSYS překladačem z `/usr/bin`.

Ve výchozí dynamické variantě zajistí terminál MSYS2 dostupnost runtime DLL. Při kopírování
programu mimo MSYS2 přibalte potřebné knihovny z `ucrt64/bin` (SDL2, libstdc++,
libgcc, případně libwinpthread); jejich skutečný seznam ukáže
`objdump -p build/ondra.exe`. ROM a zvukové vzorky jsou vloženy přímo do programu.
Není potřeba samostatný adresář s ROM nebo zvukovými daty.

### Samostatné EXE bez přibalovaných DLL

Pro Windows lze SDL2, libstdc++, libgcc i libwinpthread připojit staticky.
V terminálu **MSYS2 UCRT64** použijte novou sestavovací složku:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja
cmake -S . -B build-static -G Ninja -DCMAKE_BUILD_TYPE=Release -DONDRA_STATIC=ON
cmake --build build-static --parallel 4
ctest --test-dir build-static --output-on-failure
./build-static/ondra.exe
```

Na jiný počítač s Windows 10/11 pak stačí přenést **`ondra.exe`**. Pro běh
nepotřebuje instalaci SDL2, MSYS2 ani DLL z adresáře `ucrt64/bin`. ROM i zvukové
vzorky jsou již součástí EXE. Zůstávají závislosti na systémových DLL Windows,
které jsou součástí operačního systému. Při distribuci přiložte také licenční
texty; CI je přibaluje do archivu.

`ONDRA_STATIC=ON` automaticky stáhne a sestaví připnuté SDL2, i když je v MSYS2
nainstalovaná sdílená verze. Balíček SDL2 tedy pro tuto variantu není potřeba;
první konfigurace vyžaduje internet. Volba podporuje MinGW GCC v UCRT64,
MINGW64 a cross překladač z Linuxu. Sestavení kontroluje importy výsledných
EXE pomocí `objdump` a skončí chybou, pokud najde jinou než systémovou DLL.
Pouhé `-static-libstdc++` by nestačilo, protože ponechává závislost na
libwinpthread.

V [GitHub Actions](https://github.com/omikron88/ondra/actions/workflows/build.yml)
se sestavují obě varianty. Po úspěšném běhu je v části **Artifacts** dostupný
archiv **`ondra-windows-x64-static`** s EXE a licencemi. Statická varianta se
navíc testuje v samostatném adresáři s PATH obsahujícím pouze systém Windows.

Volitelný cross překlad z Linuxu:

```sh
cmake -S . -B build-windows \
  -DCMAKE_TOOLCHAIN_FILE=cmake/mingw64-toolchain.cmake \
  -DONDRA_STATIC=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-windows --parallel 4
```

Vyžaduje nainstalovaný `x86_64-w64-mingw32-gcc/g++/windres`. Nativní sestavení
v MSYS2 tento toolchain soubor nepoužívá. CI v `.github/workflows/build.yml`
obsahuje nativní linuxové a MSYS2 sestavení včetně testů.

## Používání

```sh
./build/ondra --rom plus
./build/ondra --rom vili27 --tape program.tap
./build/ondra --rom plus --load program.bin --melodik
./build/ondra --sd cesta/ke/karte
./build/ondra --snapshot ulozeno.osn
./build/ondra --rom-a vlastni_a.rom --rom-b vlastni_b.rom
```

Výchozí ROM je BASIC EXP V5. K dispozici jsou `basic`, `tesla`, `vili25`,
`vili27` a `plus`. Vlastní ROM má dva samostatné banky po 2, 4 nebo 8 KiB;
menší banky se opakují v příslušném 8KiB okně jako v JOndře.

Soubor BIN se standardně načte po 100 snímcích, aby ROM nejprve inicializovala
počítač. `--load-after 0` načítá před prvním snímkem. Podporován je blokový formát
OndraSD: blok `01h, adresa LE16, délka LE16, data` a volitelný vstupní bod
`02h, adresa LE16`. Data se načítají do fyzické RAM; běžící program nadále ovládá
mapování ROM a I/O portem A3. Pro soubor bez hlavičky použijte
`--raw soubor --address 0x4000 --pc 0x4000`.

Do okna lze přetáhnout BIN, OSN nebo páskový soubor. Připojení pásky samo
nespouští zavaděč: načítání/nahrávání a motor řídí program v emulovaném počítači.
Přepínač `--record vystup.csw` zaznamenává páskový výstup a uloží jej při ukončení.
Úplný přehled parametrů vypíše `--help`.

Ve ViLi ROM načte první program z pásky prázdný řádek potvrzený klávesou Enter.
V Ondra+ zvolte v úvodním menu „Zavést z pásky“. Generátor TAP používá stabilní
bifázový signál 2 500 bit/s, ověřený skutečným dekodérem v těchto ROM;
nepřebírá závislost původního generátoru na zaokrouhlování Java časovače.

| Klávesa PC | Funkce |
|---|---|
| F1 | Nápověda do konzole |
| F2 | Pauza / pokračování |
| F3 | Jedna instrukce a výpis registrů |
| F4 | NMI |
| F5 / Shift+F5 | Reset se zachováním RAM / studený reset |
| F6 | Přepnutí 2/4 MHz |
| F8 / F9 | Uložit / načíst `ondra.osn` |
| F10 | Ukončit |
| F11 | Celá obrazovka |
| F12 | Uložit `screenshot.ppm` |
| Home | Převinout pásku |

Písmena a šipky používají mapování původní JOndry: `Shift` = Shift,
`Alt` = Symbols, `Ctrl` = Ctrl, `Tab` = Numbers, `=` = CS. Čísla se zadávají
kombinací Numbers s příslušnou klávesou Ondry. Joystick používá numerickou
klávesnici 8/2/4/6 a 0 jako tlačítko. Při ztrátě fokusu se stisky uvolní.
Zelená/žlutá LED a stav motoru pásky jsou vidět v titulku okna.

## Testování a terminálový debugger

Jádro lze sestavit samostatně, bez SDL2 a bez grafického prostředí:

```sh
cmake -S . -B build-headless -DONDRA_BUILD_GUI=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build-headless
ctest --test-dir build-headless --output-on-failure
./build-headless/ondra-headless --rom plus --frames 150 --check-boot \
  --screenshot obraz.ppm
./build-headless/ondra-headless --rom plus --debug
```

Headless režim běží bez čekání na skutečný čas. Vypíše registry, počet CPU
T-stavů, rozlišení, počet svítících bodů a FNV-1a hash obrazu. `--check-boot`
ověřuje zapnuté video a neprázdný obraz; samotné CPU a mapování ověřuje test
`core`, který se skládá z deseti skupin a více než 4 000 kontrol. Ověřuje i
skutečné načtení syntetického TAP programu pomocí zavaděče v ROM. Samostatné
testy spouštějí každou ROM. SDL smoke test používá ovladače `dummy`.

Příkazy debuggeru (adresy jsou desetinné nebo s prefixem `0x`/`$`):

```text
r                 registry
s [počet]         instrukční krokování
n                 krok přes CALL/RST (limit 1 000 000 instrukcí)
c [snímky]        pokračovat; výchozí limit 50 snímků
b 0x4000          přidat/odebrat breakpoint na PC
d 0x4000 128      číst mapovanou paměť
w 0x4000 0x3e 1   upravit fyzickou RAM
pc 0x4000         nastavit PC a opustit HALT
in 0xff           číst port
out 3 7           zapsat port
key 5 4 1         stisk matice: řádek, bit, stav (0 = pustit)
load soubor.bin   načíst BIN
save soubor.osn   uložit OSN
loadsn soubor.osn načíst OSN
tape soubor.tap   připojit pásku
reset             studený reset
q                 ukončit
```

## Rozsah a přesnost portu

Implementována je 64KiB RAM, 16KiB okno ROM, I/O v E000–FFFF, aktivně nízká
klávesnicová matice, joystick, adresové aliasy výstupních latchů A0/A1/A3,
proměnné rozlišení, obraz 320 × 256, 50Hz přerušení, NMI, turbo, původní
zvukové vzorky a Melodik se SN76489. Zvuk má 44,1 kHz / 16 bitů / mono.

Pásky: Ondra TAP s kontrolou součtu a volitelným LF/CR oddělením, CSW v1/v2
s RLE (typ 1), WAV mono PCM 8/16 bitů; záznam do CSW v2. Z-RLE/zlib CSW není
podporováno, stejně jako v původní JOndře. Vadné nebo neúplné BIN/OSN obrazy
nezmění RAM částečně.

OndraSD zachovává bitovou sériovou linku 9600/57600 Bd, vysílání `__LOADER.BIN`,
příkazy adresáře/souboru, FAT 8.3 aliasy a RTC. Adresář zadaný `--sd` představuje
kartu. Sektorové CP/M příkazy B/R/W vracejí chybu stejně jako v Javě. Symlinky
mimo kořen karty se nezpřístupňují. Emulátor nemění hostitelské systémové hodiny.

OSN v1/v2 je kompatibilní s JOndrou a ukládá registry, porty, ROM typ, rozlišení
a fyzickou RAM včetně RAM pod ROM/I/O; vlastní ROM přibalí do obrazu. Tento
historický formát neukládá PSG, polohu pásky, stav přenosu SD, turbo ani rozdíl
ViLi 25/27. Pro ViLi 25 načítejte snapshot s `--rom vili25`; jinak se použije 27.

CPU je instrukčně přesné jádro redcode. Zdržení CPU videem se účtuje po snímcích
podle původní JOndry; nejde o nový model sběrnice po jednotlivých T-stavech.
PSG je samostatná implementace registrů, děličů a šumového LFSR; zvuk zatím
nebyl porovnán měřením s fyzickým Melodikem.

Swingové dialogy jsou nahrazeny parametry, přetažením souborů a klávesovými
zkratkami. Terminálový debugger poskytuje krokování, breakpointy a editaci
paměti. Grafický disassembler, timeline se zpětným krokováním, obrázková
klávesnice a efekty CRT/ScaleNx z Java UI zatím přeneseny nejsou.

## Uspořádání a licence

`include/ondra` a `src` obsahují přenositelné jádro, souborové formáty a obě
rozhraní. `tests` ověřuje hardware i formáty. `assets` obsahuje nezměněné ROM
a vzorky z JOndry; CMake je vloží do programu. `third_party` obsahuje nezměněné
Z80 a Zeta, takže lze jádro sestavit i offline.

Projekt navazuje na GPLv3 JOndru a je vydán pod GPL-3.0-or-later, viz [LICENSE](LICENSE).
Původ a licence převzatých částí jsou uvedeny v [THIRD_PARTY.md](THIRD_PARTY.md).
