# ELF Analizi — udp-server.z1 (MSP430 ELF)
### BIL304 · Araştırma İş Parçacığı — Toolchain Analizi

> **Analiz ettiğim dosya:** `udp-server.z1` (Alıcı / Server Düğümü)  
> **Kullandığım araç zinciri:** `msp430-gcc` / WSL Binutils  
> **Şablon repo:** https://github.com/ismailhakkituran/ota-toolchain-anaysis

---

## 1. Dosya Kimliği — `file` Komutuyla Bakış

İlk olarak ürettiğimiz sunucu dosyasının tam olarak ne olduğuna bakmak istedik:

```bash
$ file udp-server.z1
```

**Aldığımız Çıktı:**
```text
udp-server.z1: ELF 32-bit LSB executable, TI msp430, version 1 (embedded), statically linked, with debug_info, not stripped
```

### Buradan ne anlıyoruz?
* **Format:** Dosya 32-bit ELF formatında. 
* **Mimari:** TI MSP430 serisi için derlenmiş (bizim Z1 mote'ların işlemcisi).
* **Endianness:** LSB (Little Endian) kullanılmış, yani düşük anlamlı byte'lar önce geliyor.
* **Bağlantı (Link):** "statically linked" yazıyor, yani dışarıdan bir kütüphaneye bağımlı değil, her şey (Contiki OS dahil) tek bir dosyanın içine gömülmüş.
* **Debug:** "not stripped" ve "with debug_info" ibareleri, dosyanın içinde debug sembollerinin hala durduğunu gösteriyor. Yani henüz release versiyonu gibi küçültülmemiş.

> **Peki neden düz binary (.bin) kullanmıyoruz da ELF kullanıyoruz?**  
> Düz binary dosyasında sadece makine kodları alt alta durur. Ama ELF formatı, cihaza veya debug araçlarına "kod şuradan başlıyor, şu veriyi RAM'e koy, şu veriyi Flash'ta bırak" gibi meta-verileri söylüyor. OTA yaparken ham binary kullanıyoruz ama geliştirme ve analiz aşamasında bu ELF bilgilerine ihtiyacımız var.

---

## 2. ELF Başlığı (Header) İncelemesi — `readelf -h`

```bash
$ msp430-readelf -h udp-server.z1
```

**Çıktı:**
```text
ELF Header:
  Magic:   7f 45 4c 46 01 01 01 ff 00 00 00 00 00 00 00 00 
  Class:                             ELF32
  Data:                              2's complement, little endian
  Version:                           1 (current)
  OS/ABI:                            Standalone App
  Type:                              EXEC (Executable file)
  Machine:                           Texas Instruments msp430 microcontroller
  Entry point address:               0x3100
  Start of section headers:          65412 (bytes into file)
```

### Yorumlarım:
* En baştaki **Magic** değerindeki `7f 45 4c 46`, ASCII olarak `\x7fELF` anlamına geliyor, yani dosyanın bozulmamış bir ELF imzası var.
* **Entry point (Giriş noktası):** `0x3100` olarak görünüyor. Cihaz enerji aldığında doğrudan buradaki adrese gidip çalışmaya başlıyor.
* **OS/ABI:** "Standalone App" diyor. Üzerinde kocaman bir işletim sistemi (Windows/Linux) koşmayan, Contiki-NG gibi gömülü (bare-metal) sistemler için bu beklediğimiz bir sonuç.

---

## 3. Bölümler (Sections) — `readelf -S`

Derleyicinin sunucu kodumuzu ve değişkenlerimizi nasıl parçaladığını görmek için section tablosuna baktık:

```bash
$ msp430-readelf -S udp-server.z1 | grep -E "\.text|\.data|\.bss|\.rodata|\.vectors"
```

```text
  [ 1] .text             PROGBITS        00003100 0000d4 00a0c6 00  AX  0   0  2
  [ 2] .rodata           PROGBITS        0000d1c8 00a19c 000553 00   A  0   0  4
  [ 3] .data             PROGBITS        00001100 00a6f0 000150 00  WA  0   0  2
  [ 4] .bss              NOBITS          00001250 00a840 0016e8 00  WA  0   0  2
  [ 6] .vectors          PROGBITS        0000ffc0 00a840 000040 00  AX  0   0  1
```



* **`.text` (Çalışma Adresi: 0x3100 | Boyut: 41.1 KB):**  
  Yazdığımız OTA kodlarının ve işletim sisteminin makine diline çevrilmiş hali burada tutuluyor. `AX` (Allocate ve Execute) bayrakları sayesinde bunun çalıştırılabilir bir kod olduğunu ve Flash belleğe yerleştiğini anlıyoruz.

* **`.rodata` (Çalışma Adresi: 0xD1C8 | Boyut: 1.3 KB):**  
  `LOG_INFO` içindeki metinler, `"firmware.bin"` ismi gibi sabit (const) değerler burada. Read-Only (`A` bayrağı) olduğu için RAM'e gitmesine gerek yok, Flash'ta kalıyor.

* **`.data` (Çalışma Adresi: 0x1100 | Boyut: 336 B):**  
  Kodda başlangıç değeri verdiğimiz global değişkenler burada. Flash'ta saklanıyor ama program başlarken RAM'e (`0x1100` adresine) kopyalanıyor (`WA` - Write/Allocate).

* **`.bss` (Çalışma Adresi: 0x1250 | Boyut: 5.8 KB):**  
  Başlangıç değeri atanmamış global değişkenlerimiz. Type kısmında `NOBITS` yazıyor; yani bu değişkenler ELF dosyasında (ve Flash'ta) hiç yer kaplamıyor. Sadece işlemciye "RAM'de 5.8 KB'lık yer ayır ve hepsini sıfırla" talimatı veriyor.

* **`.vectors` (Adres: 0xFFC0):**  
  Z1 Mote'un (MSP430) 32 adet kesme vektörünün adresi burada listelenir. UDP paketleri ulaştığında devreye giren donanım kesmelerinin (Interrupts) adresleri buradadır.

---

## 4. Segment Yerleşimi (Program Headers) — `readelf -l`

Bu komutla işletim sisteminin/bootloader'ın dosyayı belleğe nasıl yükleyeceğini (mapping) kontrol ettik:

```bash
$ msp430-readelf -l udp-server.z1
```

Burada **VMA** (Virtual Address) ile **LMA** (Physical Address) farkını çok net görebiliyoruz:
* `.data` segmenti için LMA ile VMA farklı değerlere sahip. Yani bu veriler Flash'ın ücra bir köşesinde yedekleniyor ama çalışma zamanında doğrudan RAM adreslerine (`0x1100`) kopyalanıyor.
* `.text` segmenti için iki adres de aynı. Kodları RAM'e kopyalamaya gerek yok, doğrudan Flash üzerinden Execute In Place (Yerinde Çalıştırma) yapılıyor.

---

## 5. Boyutların Hesaplanması — `msp430-size`

```bash
$ msp430-size udp-server.z1
```

**Çıktı:**
```text
   text    data     bss     dec     hex filename
  42585     336    5866   48787    be93 udp-server.z1
```

### Hesaplamalar:
* **Flash (ROM) Harcaması:** `text` + `data` = 42,585 + 336 = **~41.9 KB**
* **RAM (SRAM) Harcaması:** `data` + `bss` = 336 + 5,866 = **~6.05 KB**

**Z1 mote için ne ifade ediyor?**
Z1 mote'ların işlemcisi olan MSP430F2617'de 92 KB Flash ve 8 KB RAM var. 
Hesapladığımda Flash'ın **%45**'ini, RAM'in ise **%75**'ini doldurmuş durumdayız. Flash oldukça rahat ama RAM sınırına epey yaklaşmışız; bunun sebebi ağ dinlemesi için kullanılan büyük IPv6/RPL tamponları ve bizim OTA paketi bekleyen tampon belleklerimiz.

---

## 6. Sembol Tablosuna Bakış — `nm` Komutu

Uygulamadaki fonksiyonların ve ağ değişkenlerinin bellek adreslerini listelemek için nm komutunu kullandık:

```bash
$ msp430-nm -n udp-server.z1 | grep -E "main|_start|uip_buf|udp_rx_callback"
```

**Çıktıdaki kritik satırlar:**
```text
00001100 D uip_buf
00003100 T _start
0000313e T main
00005470 T udp_rx_callback
```

Buradaki `T` harfi, bu sembollerin `.text` bölümünde yer alan kodlar olduğunu belirtiyor. 
* C kodunda yazdığımız `main` fonksiyonu, derleme bitince bellekte tam olarak `0x313e` adresine denk gelmiş. 
* Paketleri dinleyen bizim meşhur `udp_rx_callback` fonksiyonumuz `0x5470` adresine yerleşmiş.
* Ağ buffer'ı olan `uip_buf` ise `0x1100` yani tam RAM başlangıç adresine (`.data`) yerleşmiş.

---

## 7. String Analizi — `strings` Komutu

Bir de derlenmiş dosyanın içinde açık metin (string) olarak neler kalmış diye bakmak istedik:

```bash
$ strings udp-server.z1 | grep -i "UDP"
```

**Bulduğumuz çıktı:**
```text
UDP sunucusu başlatıldı, OTA yazılımı bekleniyor...
Yüklenmeye hazır yeni firmware alımı tamamlandı.
Checksum error for block %u. Expected: %u, Got: %u
```

**Bunlar ne anlama geliyor?**
Bizim `udp-server.c` içine yazdığımız tüm C-string (LOG_INFO) yapılarının makine koduna doğrudan gömüldüğünü görüyoruz. Bu metinler `readelf`'te gördüğümüz o `.rodata` bölümünde tutuluyor. Eğer cihazın RAM/Flash yetersizliği olsaydı ilk kırpacağımız (sileceğimiz) yerler bu log stringleri olurdu.

---

## 8. CC1352R Donanımıyla Karşılaştırma

Aynı derlenmiş dosyayı (`udp-server.z1`) modern bir ARM cihaz olan **CC1352R**'ye atsaydım çalışır mıydı diye düşündüm. Tabi ki çalışmazdı.

Çünkü bellek haritaları tamamen alakasız:
* Z1'de Flash bellek `0x3100` adresinden başlıyor (analizde kodların oraya yerleştiğini de gördük).
* CC1352R'de ise ana Flash `0x00000000` adresinden başlıyor.
* Üstelik Z1 16-bit bir işlemciyken, CC1352R 32-bit ARM Cortex-M4F. Makine komutları bile birbirini tutmuyor. Yani CC1352R'de çalıştırmak istersek kodu ARM derleyicisiyle (`arm-none-eabi-gcc`) baştan derlemek şart.

---

## Kısaca Özetlersek

Sadece kod yazıp bırakmak yerine ELF dosyasının içine bakmak, arka planda derleyicinin RAM'i ve Flash'ı nasıl böldüğünü (bss, data, text mantığı) anlamamı sağladı. Özellikle `size` komutuyla Z1 düğümündeki RAM doluluk oranının %75'e dayandığını (ağ kütüphaneleri yüzünden) bizzat bellek adreslerinden tespit etmiş olduk.
