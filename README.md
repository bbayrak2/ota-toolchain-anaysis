# MSP430 `.z1` / `.sky` / `ARM M4F(CC1352R)` / `cooja-native` Platformları için Üretilmiş Firmware’ler Üzerinde Yapılabilecek Analiz Türleri Kontrol Listesi

---
##### (* ARM Mimarisinde derlenmiş firmware analizi yapmak isteyen gruplar MSP430 Toolchain yanında ARM-Toolchain araçlarını da indirip, kullanmalıdırlar.)

``` bash
  $ wget https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu-rm/9-2020q2/gcc-arm-none-eabi-9-2020-q2-update-x86_64-linux.tar.bz2
  $ tar -xjf gcc-arm-none-eabi-9-2020-q2-update-x86_64-linux.tar.bz2
```
---
##### ** Analiz etmeniz için farklı platformlarda oluşturulmuş örnek firmware arşivi bil.omu drive linki için [tıklayınız](https://drive.google.com/file/d/1oLrZWPmDyuznWe5qS7zOsfSyyyPcQbBG/view?usp=sharing) .


---

# 1. Binary Kimlik Analizi

* Hedef platform analizi (`.z1` / `.sky` / `ARM M4F(CC1352R)` / `cooja-native`)
* MSP430 mimari tipi
* ELF format bilgisi
* Endianness nedir ve Endianness bilgisi
* Entry point adresi
* ABI nedir ve ABI bilgisi
* Compiler izi
* Toolchain versiyonu
* Optimization level tahmini
* Debug symbol var/yok analizi

Araçlar:

* `msp430-readelf`
* `msp430-objdump`
* `msp430-strings`
* `Ve üstteki araçların ARM versiyonları...`
---

# 2. Bellek Kullanım Analizi

* Flash, RAM, Stack, Heap anlamları
* Flash kullanım miktarı
* RAM kullanım miktarı
* `.text` boyutu
* `.data` boyutu
* `.bss` boyutu
* Stack kullanım tahmini
* Heap var/yok analizi
* Section dağılımı
* Memory map analizi
* Büyük veri yapılarının tespiti

Araçlar:

* `msp430-size`
* `msp430-readelf`
* `msp430-nm`
* `Ve üstteki araçların ARM versiyonları...`

---

# 3. Symbol / Function Analizi

* Fonksiyon isimleri
* Global değişkenler
* Static değişkenler
* ISR (interrupt) fonksiyonları
* Contiki process entry’leri
* Radio driver fonksiyonları
* Timer callback’leri
* Networking callback’leri
* Sensor handler’ları
* Kullanılan kütüphaneler
* Kullanılmayan (dead) fonksiyonlar
* Function address mapping

Araçlar:

* `msp430-nm`
* `msp430-readelf`
* `msp430-objdump`
* `Ve üstteki araçların ARM versiyonları...`

---

# 4. String ve Metadata Analizi

* Debug mesajları
* printf logları
* IPv6 adresleri
* MAC adresleri
* Network node ID’leri
* Sensor isimleri
* Process isimleri
* Routing protokol isimleri
* TSCH/6LoWPAN/RPL stringleri
* Hidden diagnostic message’lar
* Hardcoded config değerleri
* Developer notları

Araçlar:

* `msp430-strings`
* `Ve üstteki aracın ARM versiyonu...`

---

# 5. Assembly / Instruction Analizi

* Instruction sequence analizi
* Function prologue/epilogue
* Register kullanımı
* Stack frame yapısı
* ISR akışı
* Loop yapıları
* Branch analizi
* Jump table analizi
* Function call graph
* Inline function tespiti
* Compiler optimization davranışı
* Delay loop analizi
* Busy-wait yapıları
* Context switching
* Protothread expansion
* Scheduler davranışı

Araçlar:

* `msp430-objdump`
* `msp430-as`
* `Ve üstteki araçların ARM versiyonları...`

---

# 6. Source-Level Mapping Analizi

(Debug build varsa)

* Address → source line eşleme
* Function → source file eşleme
* ISR → source mapping
* Crash address çözümleme
* Optimization sonrası source mapping
* Inline edilmiş kodların tespiti

Araçlar:

* `msp430-addr2line`
* `msp430-objdump -S`
* `Ve üstteki araçların ARM versiyonları...`

---

# 7. ELF Yapısı Analizi

* ELF header
* Section header
* Program header
* Symbol table
* Relocation entries
* Debug sections
* DWARF info
* Linker-generated metadata
* Startup section
* Vector table
* Initialization routines

Araçlar:

* `msp430-readelf`
* `msp430-elfedit`
* `Ve üstteki araçların ARM versiyonları...`

---

# 8. Interrupt ve Donanım Analizi

* Interrupt vector table
* GPIO access pattern
* Timer interrupt kullanımı
* UART ISR
* Radio interrupt handler
* ADC access
* Sensor polling
* Low-power mode geçişleri
* Clock configuration
* MSP430 register erişimleri

Araçlar:

* `msp430-objdump`
* `msp430-readelf`
* `Ve üstteki araçların ARM versiyonları...`

---

# 9. Networking Analizi

* Unicast kullanım tespiti
* Broadcast kullanım tespiti
* Multicast tespiti
* IPv6 stack kullanımı
* RPL routing analizi
* TSCH scheduler çağrıları
* MAC layer interaction
* Packet buffer kullanımı
* Neighbor table erişimi
* Radio transmission akışı
* Retransmission logic
* ACK mekanizmaları
* CSMA/TSCH farkları
* Contiki network API kullanımı

Araçlar:

* `msp430-nm`
* `msp430-objdump`
* `msp430-strings`
* `Ve üstteki araçların ARM versiyonları...`

---

# 10. Wireless / TSCH Analizi

* TSCH slot operation
* Channel hopping logic
* ASN handling
* Radio timing loops
* Synchronization routines
* Schedule management
* Packet timing
* MAC timing critical path
* Drift compensation
* Low-power radio behavior

Araçlar:

* `msp430-objdump`
* `msp430-nm`
* `Ve üstteki araçların ARM versiyonları...`

---

# 11. Sensor ve Peripheral Analizi

* Button handler
* LED driver
* UART usage
* SPI access
* I2C access
* ADC routines
* Sensor polling interval
* Interrupt-driven sensor logic
* GPIO toggle behavior
* Peripheral initialization sequence

Araçlar:

* `msp430-objdump`
* `msp430-nm`
* `Ve üstteki araçların ARM versiyonları...`

---

# 12. Algoritma Koşma / DSP / Matematiksel Analiz

* Floating-point kullanımı
* Fixed-point kullanımı
* Trigonometric computation
* Multiply/divide routines
* Software floating-point emulation
* DSP benzeri loop’lar
* Matrix operation izleri
* Signal processing pattern’leri
* Computational hotspot’lar
* Numerical optimization

Araçlar:

* `msp430-objdump`
* `msp430-gprof`
* `msp430-nm`
* `Ve üstteki araçların ARM versiyonları...`

---

# 13. Güç ve Performans Analizi

* Low-power mode geçişleri
* CPU-intensive function’lar
* Busy-wait detection
* Sleep/wakeup flow
* Timer usage intensity
* Radio duty cycle tahmini
* ISR yoğunluğu
* Function execution cost
* Flash/RAM efficiency
* Energy-heavy computation bölgeleri

Araçlar:

* `msp430-gprof`
* `msp430-objdump`
* `msp430-size`
* `Ve üstteki araçların ARM versiyonları...`

---

# 14. Coverage ve Profiling Analizi

* Function call frequency
* Execution hotspot
* Unused branch’ler
* Rarely executed path’ler
* Test coverage
* Critical execution path
* Runtime bottleneck’ler

Araçlar:

* `msp430-gcov`
* `msp430-gprof`
* `Ve üstteki araçların ARM versiyonları...`

---

# 15. Reverse Engineering Analizi

* Firmware behavior recovery
* Unknown firmware classification
* Feature inference
* Protocol inference
* ISR purpose discovery
* Hardware interaction recovery
* State machine extraction
* Scheduler reconstruction
* Event-flow reconstruction
* Network role inference

Araçlar:

* `msp430-objdump`
* `msp430-nm`
* `msp430-readelf`
* `msp430-strings`
* `Ve üstteki araçların ARM versiyonları...`

---

# 16. Compiler ve Optimization Analizi

* `-O0/-O2/-Os` farkları
* Inlining behavior
* Dead code elimination
* Constant folding
* Loop optimization
* Register allocation
* Tail-call optimization
* Branch optimization
* Macro expansion
* Preprocessor etkileri

Araçlar:

* `msp430-gcc`
* `msp430-cpp`
* `msp430-objdump`
* `Ve üstteki araçların ARM versiyonları...`

---

# 17. Linker ve Build Sistemi Analizi

* Section placement
* Link order
* Static library linkage
* Startup code
* Linker script behavior
* Vector placement
* Symbol resolution
* Relocation behavior

Araçlar:

* `msp430-ld`
* `msp430-ar`
* `msp430-ranlib`
* `msp430-readelf`
* `Ve üstteki araçların ARM versiyonları...`

---

# 18. Binary Transformation Analizi

* ELF → HEX conversion
* ELF → binary conversion
* Section extraction
* Symbol stripping
* Debug removal
* Firmware minimization
* Binary patch preparation

Araçlar:

* `msp430-objcopy`
* `msp430-strip`
* `Ve üstteki araçların ARM versiyonları...`

---

# 19. Library ve Archive Analizi

* Static library içeriği
* Object file extraction
* Archive symbol table
* Linked module analizi

Araçlar:

* `msp430-ar`
* `msp430-gcc-ar`
* `msp430-ranlib`
* `Ve üstteki araçların ARM versiyonları...`

---

# 20. Contiki-NG Özel Analizler

* PROCESS_THREAD recovery
* Protothread expansion
* Event-driven scheduler analizi
* etimer/ctimer usage
* PROCESS_BEGIN/END expansion
* PROCESS_YIELD flow
* NETSTACK interaction
* Packetbuf lifecycle
* uIP callback chain
* Rime stack usage

Araçlar:

* `msp430-cpp`
* `msp430-objdump`
* `msp430-nm`
* `Ve üstteki araçların ARM versiyonları...`

---

# 21. Güvenlik ve Robustness Analizi

* Hardcoded credential arama
* Debug backdoor izleri
* Buffer handling
* Unsafe memory access
* Stack-heavy routines
* Potential overflow bölgeleri
* Assert/debug remnants
* Information leakage string’leri

Araçlar:

* `msp430-strings`
* `msp430-objdump`
* `msp430-readelf`
* `Ve üstteki araçların ARM versiyonları...`

---

# 22. Karşılaştırmalı Firmware Analizi

İki firmware arasında:

* Code size farkı
* RAM farkı
* Function count farkı
* ISR yoğunluğu
* Networking complexity
* Radio stack farkı
* Symbol farkı
* Optimization farkı
* Assembly complexity farkı



---

# 23. Eğitimsel Reverse Engineering Görevleri

* Bir firmware’in ne yaptığını bulma
* hangi protokolü kullandığını çıkarma
* button/LED mapping bulma
* ISR’leri tanıma
* network role çıkarımı
* Kullandığı algoritmik blok tespiti
* energy-heavy bölgeleri bulma
* stripped firmware çözümleme


---
