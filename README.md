# BIL304 — OTA (Over-The-Air) Firmware Güncelleme Projesi

**Ondokuz Mayıs Üniversitesi | İşletim Sistemleri Dersi | Proje 1**

| Üye                  | Rol                         | Numara   |
| -------------------- | --------------------------- | -------- |
| Yavuz Selim Sağlam   | İstemci (Client) Geliştirme | 23060540 |
| Ramazan Burak Bayrak | Sunucu (Server) Geliştirme  | 23060492 |

---

> 🎬 **YouTube:** https://youtu.be/F2lkJMzya4E

---

## 📌 Proje Amacı

IEEE 802.15.4 telsiz ağlarında çalışan Z1 Mote (MSP430) cihazlarına, fiziksel bağlantı gerektirmeden ağ üzerinden yeni firmware (yazılım) gönderme sisteminin geliştirilmesidir.

**Temel problem:** `new-firmware.z1` dosyası ~77 KB'tır. IEEE 802.15.4 MTU limiti yalnızca 127 byte'tır. Dosya tek seferde gönderilemez. Çözüm: Firmware'i 64 byte'lık bloklara bölerek Stop-and-Wait protokolüyle güvenilir biçimde iletmek.

---

## 🏗️ Sistem Mimarisi

```
[Node 2 - Gönderici]  ──►  [Node 3 - Router]  ──►  [Node 1 - Alıcı]
   udp-client.z1              udp-client.z1           udp-server.z1
   (0m - 80m arası)          (40m)                   (0m - RPL Kökü)
```

| Düğüm  | Firmware      | Rol                                             |
| ------ | ------------- | ----------------------------------------------- |
| Node 1 | udp-server.z1 | RPL ağ kökü, firmware alıcı, CFS'ye yazar       |
| Node 2 | udp-client.z1 | Firmware'i parçalar, Stop-and-Wait ile gönderir |
| Node 3 | udp-client.z1 | Router — Node 2 ile Node 1 arasında köprü       |

**Neden aynı firmware (udp-client.z1) Node 2 ve Node 3'e yüklendi?**

```c
if(node_id != 2) {
    // Node 3 buraya düşer: OTA yapmaz, RPL'de kalır
    while(1) { PROCESS_WAIT_EVENT(); }
}
```

`node_id`, donanımın kendisinden okunur (`sys/node-id.h`). Cooja simülatörü her mote'a başlangıçta bir ID atar. Bu sayede tek firmware, ID'ye göre farklı davranır.

---

## 📦 OTA Paket Yapısı

Her firmware bloğu şu struct ile taşınır:

```c
typedef struct __attribute__((packed)) {
  uint8_t  type;              // DATA=1, ACK=2, NACK=3
  uint16_t seq_no;            // Blok sıra numarası (0'dan başlar)
  uint16_t total_blocks;      // Toplam blok sayısı
  uint8_t  payload_len;       // Bu paketteki gerçek veri uzunluğu
  uint16_t crc16;             // Veri bütünlüğü için CRC16 değeri
  uint8_t  payload[64];       // Firmware verisi (max 64 byte)
} ota_packet_t;               // Toplam: 72 byte
```

**Her alanın gerekçesi:**

| Alan           | Boyut   | Açıklama                                                            |
| -------------- | ------- | ------------------------------------------------------------------- |
| `type`         | 1 byte  | Paketin ne olduğunu belirler: DATA/ACK/NACK                         |
| `seq_no`       | 2 byte  | Hangi blok gönderiliyor; alıcı doğru offset'e yazar                 |
| `total_blocks` | 2 byte  | Alıcı bu sayıya ulaşınca transferin bittiğini anlar                 |
| `payload_len`  | 1 byte  | Son blok 64'ten küçük olabilir, alıcı `cfs_write`'a parametre verir |
| `crc16`        | 2 byte  | Tek bir bit bozulsa farklı değer üretir → tespit edilir             |
| `payload[64]`  | 64 byte | Firmware'in gerçek içeriği                                          |

**Neden payload 64 byte?**

```
IEEE 802.15.4 MTU : 127 byte
6LoWPAN başlık    : ~25 byte
UDP/IPv6 başlık   : ~20 byte
Struct üst bilgi  :  ~8 byte
─────────────────────────────
Kullanılabilir    :  74 byte → Güvenli marjla 64 byte seçildi
```

**`__attribute__((packed))` neden şart?**
Olmadan derleyici `uint16_t` alanları arasına boş byte (padding) ekler. Bu, gönderici ile alıcının aynı yapıyı farklı yorumlamasına neden olur.

---

## 🔐 CRC16 Hash Algoritması

### Teorik Açıklama

CRC16 (Cyclic Redundancy Check — 16 bit), verinin bütünlüğünü kontrol eden bir hata tespit algoritmasıdır.

**Çalışma prensibi:**

1. Veri, 16-bit'lik bir polinom (örn. `0x8005`) ile XOR ve bit kaydırma işlemlerine tabi tutulur.
2. Her byte işlendikten sonra 16-bit'lik bir sonuç (remainder) elde edilir.
3. **Tek bir bit bile değişse** sonuç tamamen farklı olur.
4. Gönderici hesaplar ve pakete ekler. Alıcı aynı hesabı yapıp karşılaştırır.

```
Veri: [0xAB, 0xCD, 0xEF, ...]
         ↓  polinom XOR + kaydırma
CRC16:  0x3509   ← her paket için eşsiz değer
```

### Projede Kullanımı

**Gönderici (udp-client.c) — hesapla ve pakete ekle:**

```c
pkt.crc16 = crc16_data(pkt.payload, pkt.payload_len, 0);
```

**Alıcı (udp-server.c) — tekrar hesapla ve karşılaştır:**

```c
uint16_t calc_crc = crc16_data(pkt->payload, pkt->payload_len, 0);
if(calc_crc != pkt->crc16) {
    reply.type = PKT_TYPE_NACK;
    simple_udp_sendto(&udp_conn, &reply, sizeof(reply), sender_addr);
    return;
}
```

**Tüm imaj bütünlük doğrulaması (transfer sonunda):**

```c
static uint32_t calculate_file_hash(const char *filename) {
    uint32_t hash = 0;
    uint8_t buf[64];
    int fd = cfs_open(filename, CFS_READ);
    int bytes_read;
    while((bytes_read = cfs_read(fd, buf, sizeof(buf))) > 0) {
        for(int i = 0; i < bytes_read; i++) {
            hash += buf[i];   // Running sum checksum
        }
    }
    cfs_close(fd);
    return hash;
}
// Simülasyon çıktısı: "Diskteki Imaj Hash Degeri: 13597"
```

---

## 🔄 Güvenilir Aktarım — Stop-and-Wait Protokolü

Stop-and-Wait, window size = 1 olan özel bir sliding window protokolüdür. Her seferinde yalnızca bir paket gönderilir ve ACK beklenir. Kaynak kısıtlı (8KB RAM) sensör cihazları için ideal seçimdir.

**Akış:**

```
Node 2 (Gönderici)                              Node 1 (Alıcı)
      │                                               │
      │──── seq_no=0, DATA, CRC=0x3A1F ─────────────►│
      │     etimer = 3 saniye                         │ CRC kontrol ✓
      │     PROCESS_WAIT_EVENT_UNTIL...               │ cfs_seek(0); cfs_write()
      │◄─── ACK(seq=0) ───────────────────────────────│
      │ current_seq_no++                              │
      │ etimer = 20ms                                 │
      │                                               │
      │──── seq_no=1, DATA ──── [KAYIP] ─────────────►│
      │     etimer = 3 saniye                         │
      │     ... 3sn geçti, cevap yok (TIMEOUT) ...   │
      │──── seq_no=1, DATA (tekrar) ─────────────────►│
      │◄─── ACK(seq=1) ───────────────────────────────│
      ...
      │──── seq_no=N, DATA, CRC bozuk ───────────────►│
      │                                               │ CRC kontrol ✗
      │◄─── NACK(seq=N) ──────────────────────────────│
      │ seq_no değişmez, etimer = 20ms               │
      │──── seq_no=N, DATA (tekrar) ─────────────────►│
```

**Kod Uygulaması:**

```c
// Paketi gönder, 3sn bekle
simple_udp_sendto(&udp_conn, &pkt, sizeof(pkt), &dest_ipaddr);
etimer_set(&send_timer, TIMEOUT_INTERVAL);  // CLOCK_SECOND * 3

// Callback: ACK gelirse
current_seq_no++;
etimer_set(&send_timer, CLOCK_SECOND / 50); // ~20ms, hemen ilerle

// Callback: NACK gelirse
// current_seq_no değişmez → döngü uyandığında aynı blok tekrar gider
etimer_set(&send_timer, CLOCK_SECOND / 50);

// Timeout: callback hiç gelmezse etimer doğal biter
// current_seq_no değişmediğinden aynı blok yeniden gönderilir
```

---

## 💾 Kalıcı Depolama — Coffee File System (CFS)

CFS, Contiki-NG'nin flash bellek için tasarlanmış hafif dosya sistemidir.

**Başlangıç hazırlığı:**

```c
cfs_remove("update.bin");           // Eski aktarım varsa sil
int fd = cfs_open("update.bin", CFS_WRITE);
if(fd >= 0) { cfs_close(fd); }     // Boş dosya oluştur
```

**Her blok için yazma:**

```c
int fd = cfs_open("update.bin", CFS_WRITE | CFS_READ);
cfs_seek(fd, (uint32_t)pkt->seq_no * MAX_PAYLOAD_LEN, CFS_SEEK_SET);
cfs_write(fd, pkt->payload, pkt->payload_len);
cfs_close(fd);
```

`cfs_seek` neden gerekli? Her bloğun dosyada doğru konuma (offset) yazılması gerekir. `seq_no × 64` formülü o bloğun dosyadaki byte pozisyonunu verir.

---

## 👤 İstemci (Client) Tarafı — Yavuz Selim Sağlam

### Firmware'in Belleğe Alınması

`new-firmware.z1` dosyası C dizisine dönüştürülüp `firmware_data.h` içine gömülmüştür:

```c
// firmware_data.h içinde:
const uint8_t firmware_payload[] = { 0x7f, 0x45, 0x4c, 0x46, ... };
//                                    ↑ ELF Magic Bytes imzası

// udp-client.c başında:
#include "firmware_data.h"

// Boyut hesabı:
firmware_len  = sizeof(firmware_payload);   // 77757 byte
total_blocks  = (firmware_len + 63) / 64;  // 1216 blok (tavan bölme)
```

### Parçalama ve Gönderim

```c
// Her blok için offset hesabı
uint32_t offset    = (uint32_t)current_seq_no * MAX_PAYLOAD_LEN;
uint32_t remaining = firmware_len - offset;
pkt.payload_len    = (remaining > MAX_PAYLOAD_LEN) ? MAX_PAYLOAD_LEN : remaining;

// Veriyi kopyala
memset(pkt.payload, 0, MAX_PAYLOAD_LEN);
memcpy(pkt.payload, &firmware_payload[offset], pkt.payload_len);

// CRC hesapla
pkt.crc16 = crc16_data(pkt.payload, pkt.payload_len, 0);

// Gönder
simple_udp_sendto(&udp_conn, &pkt, sizeof(pkt), &dest_ipaddr);
```

### Durum ve Akış Kontrolü (udp_rx_callback)

```c
static void udp_rx_callback(...) {
    ota_packet_t *pkt = (ota_packet_t *)data;

    if(pkt->seq_no == current_seq_no) {        // Beklediğimiz bloğun cevabı mı?
        if(pkt->type == PKT_TYPE_ACK) {
            current_seq_no++;                  // Sonraki bloğa geç
            if(current_seq_no >= total_blocks) {
                transfer_complete = true;      // Tüm transfer bitti
            } else {
                etimer_set(&send_timer, CLOCK_SECOND / 50); // Hemen devam
            }
        } else if(pkt->type == PKT_TYPE_NACK) {
            etimer_set(&send_timer, CLOCK_SECOND / 50);     // Tekrar gönder
            // current_seq_no değişmez → aynı blok gider
        }
    }
}
```

### Aktarım Tamamlandığında

```c
if(transfer_complete) {
    etimer_set(&send_timer, SEND_INTERVAL * 10);  // 20sn uyku
    continue;
    // CPU ve telsiz modülü dinlendirilerek pil tasarrufu sağlanır
}
```

### Kullanılan Kütüphaneler

| Kütüphane               | Amaç                                              |
| ----------------------- | ------------------------------------------------- |
| `contiki.h`             | PROCESS makroları, Contiki-NG çekirdeği           |
| `net/routing/routing.h` | RPL API (node_is_reachable, get_root_ipaddr)      |
| `net/ipv6/simple-udp.h` | Hafif UDP soket (register, sendto, callback)      |
| `sys/node-id.h`         | Donanımdan node ID okuma                          |
| `lib/crc16.h`           | CRC16 hata kontrol hesaplama                      |
| `firmware_data.h`       | new-firmware.z1'in C dizisi olarak dahil edilmesi |

---

### 👤 Sunucu (Server) Tarafı — Ramazan Burak Bayrak

## Paket Yapısı
Server tarafında, gönderici (client) ile tam uyum sağlaması için tek bir birleştirilmiş struct kullanılmıştır. Hataları ve bellek kaymalarını önlemek için __attribute__((packed)) eklenmiştir:

c

// İletişimde kullanılan ortak paket yapısı (Veri, ACK ve NACK için)
typedef struct __attribute__((packed)) {
  uint8_t type;             // Paket tipi (1: DATA, 2: ACK, 3: NACK)
  uint16_t seq_no;          // Blok sıra numarası (0'dan başlar)
  uint16_t total_blocks;    // Toplam blok sayısı
  uint8_t payload_len;      // Bu bloktaki gerçek veri uzunluğu
  uint16_t crc16;           // Veri bütünlük kontrolü (CRC)
  uint8_t payload[MAX_PAYLOAD_LEN]; // Firmware verisi
} ota_packet_t;
magic alanı neden kaldırıldı ve type geldi? Önceki versiyonda ağdaki yabancı paketleri filtrelemek için OTA_MAGIC_NUMBER kullanılıyordu. Güncel yapıda ise Contiki standartlarına daha uygun olarak type (Paket Tipi) alanı getirilmiştir. Eğer gelen paketin tipi PKT_TYPE_DATA (1) değilse paket doğrudan reddedilir. Ayrıca dönüşlerde aynı struct kullanılarak sadece type alanı PKT_TYPE_ACK (2) veya PKT_TYPE_NACK (3) yapılarak geri yollanır.

Checksum Yöntemi: CRC16 (Döngüsel Artıklık Denetimi)
Basit byte toplamı (aritmetik toplam) yöntemi yerine Contiki-NG kütüphanesine dahil olan crc16_data() fonksiyonuna geçiş yapılmıştır:

c

// Gelen verinin CRC16'sı hesaplanır ve paketteki imza ile karşılaştırılır
uint16_t calc_chk = crc16_data(packet.payload, packet.payload_len, 0);
Basit toplam yöntemi CPU için daha az maliyetli olsa da, kablosuz ortamdaki çoklu bit kaymalarını veya sıfır basmalarını gözden kaçırabilir. CRC16 algoritması hata yakalama oranını (%99.998) seviyesine çıkararak Flash belleğe bozuk firmware yazılma ihtimalini ortadan kaldırır.

## Alıcı Callback Fonksiyonu — Adım Adım
c

static void udp_rx_callback(..., const uint8_t *data, uint16_t datalen) {
    // 1. Boyut kontrolü — beklenmedik veya eksik paketleri filtrele
    if(datalen != sizeof(ota_packet_t)) { return; }
    ota_packet_t packet;
    memcpy(&packet, data, sizeof(ota_packet_t));
    // 2. Paket Tipi kontrolü (magic yerine) — Sadece DATA paketlerini işle
    if(packet.type != PKT_TYPE_DATA) { return; }
    // 3. CRC16 doğrulaması ve Anında NACK
    uint16_t calc_chk = crc16_data(packet.payload, packet.payload_len, 0);
    if(calc_chk != packet.crc16) {
        LOG_INFO("Checksum error for block %u. Expected: %u, Got: %u\n", 
                 packet.seq_no, packet.crc16, calc_chk);
                 
        // Hatalı paket için zaman kaybetmeden anında NACK yollanır
        ota_packet_t nack_pkt;
        memset(&nack_pkt, 0, sizeof(nack_pkt));
        nack_pkt.type = PKT_TYPE_NACK;
        nack_pkt.seq_no = packet.seq_no;
        simple_udp_sendto(&udp_conn, &nack_pkt, sizeof(nack_pkt), sender_addr);
        return; 
    }
    // 4. Sıra kontrolü ve CFS'ye yazma
    if(packet.seq_no == expected_block) {
        int fd;
        if(expected_block == 0) {
            cfs_remove("firmware.bin");           // İlk blokta eski dosyayı temizle
            fd = cfs_open("firmware.bin", CFS_WRITE);
        } else {
            fd = cfs_open("firmware.bin", CFS_WRITE | CFS_APPEND); // Sonrakilerde sonuna ekle
        }
        if(fd >= 0) {
            cfs_write(fd, packet.payload, packet.payload_len);
            cfs_close(fd);
            expected_block++;
            if(expected_block == packet.total_blocks && !is_complete) {
                is_complete = true;
                LOG_INFO("Yüklenmeye hazır yeni firmware alımı tamamlandı.\n");
            }
        } else {
            LOG_ERR("CFS Hatası! Flash belleğe yazma başarısız.\n");
            return; 
        }
    }
    // Not: seq_no < expected_block ise (daha önce alınan blok ağ gecikmesiyle tekrar geldiyse)
    // Flash'a tekrar yazılmaz ama Client'ı kilitlememek için aşağıdaki ACK yine de gönderilir.
    // 5. Başarılı doğrulama sonrası ACK gönder (ota_ack_t yerine ortak ota_packet_t ile)
    ota_packet_t ack_pkt;
    memset(&ack_pkt, 0, sizeof(ack_pkt));
    ack_pkt.type = PKT_TYPE_ACK;
    ack_pkt.seq_no = packet.seq_no;
    simple_udp_sendto(&udp_conn, &ack_pkt, sizeof(ack_pkt), sender_addr);
}
CFS_APPEND Farkı ve Sıralı Yazım: İlk blok için CFS_WRITE (yeni dosya açar), sonraki bloklar için CFS_WRITE | CFS_APPEND (sonuna ekler). Bu yaklaşımda blokların sıralı gelmesi ve Flash'a düzgün kazınması garantidir — zaten expected_block kontrolü bunu sağlar. Yanlış sıradaki bloklar yazılmaz.

## Ana Süreç
c

PROCESS_THREAD(udp_server_process, ev, data) {
    PROCESS_BEGIN();
    NETSTACK_ROUTING.root_start();   // Bu düğüm RPL ağının kökü olur
    simple_udp_register(&udp_conn, UDP_SERVER_PORT, NULL,
                        UDP_CLIENT_PORT, udp_rx_callback);
    LOG_INFO("UDP sunucusu başlatıldı, OTA yazılımı bekleniyor...\n");
    PROCESS_END();
}
NETSTACK_ROUTING.root_start() çağrısı Node 1'i RPL ağının kök düğümü (DAG Root) yapar. Diğer gönderici düğümler bu kökü bularak ağa dahil olur.

## ✅ Simülasyon Çıktısı (Cooja Log)

```
[INFO: UDP-Client-OTA] Node 1 henuz ulasilabilir degil. Bekleniyor...
[INFO: UDP-Server-OTA] Blok 0 basariyla alindi, dogrulandi ve diske (cfs_write) yazildi.
[INFO: UDP-Client-OTA] ACK alindi: Blok 0. Sonraki bloga geciliyor.
[INFO: UDP-Client-OTA] Gonderiliyor: Blok 2/5 (64 byte)
[INFO: UDP-Server-OTA] Blok 1 basariyla alindi, dogrulandi ve diske (cfs_write) yazildi.
[INFO: UDP-Client-OTA] ACK alindi: Blok 1. Sonraki bloga geciliyor.
...
[INFO: UDP-Server-OTA] Tum bloklar birlestirildi. Diskteki Imaj Hash Degeri: 13597
[INFO: UDP-Client-OTA] Tum firmware bloklari basariyla gonderildi.
```

---

## 🔬 ELF Analizi (Araştırma Bölümü)

Derlenmiş firmware dosyaları `msp430-gcc` araç zinciriyle analiz edilmiştir. Detaylı ELF analizi raporları proje repolarındaki `BIL304_Proje_Raporu.txt` dosyasında mevcuttur.

**Kullanılan komutlar:**

```bash
msp430-readelf -h new-firmware.z1   # ELF başlık (magic bytes, entry point)
msp430-readelf -S new-firmware.z1   # Bölüm tablosu (.text, .data, .bss, .vectors)
msp430-size    new-firmware.z1      # Bellek ayak izi
msp430-nm -n   new-firmware.z1      # Sembol tablosu
```

**Özet bulgular:**

| Firmware        | Flash    | RAM     | Giriş Adresi |
| --------------- | -------- | ------- | ------------ |
| udp-client.z1   | ~42.2 KB | ~6.1 KB | 0x3100       |
| udp-server.z1   | ~41.9 KB | ~6.1 KB | 0x3100       |
| new-firmware.z1 | ~70.4 KB | ~5.9 KB | 0x3100       |

Z1 kapasitesi: Flash=92KB, RAM=8KB → Üçü de güvenle sığar.

---

## 🛠️ Kullanılan Teknolojiler

| Teknoloji     | Açıklama                                                |
| ------------- | ------------------------------------------------------- |
| Contiki-NG    | Gömülü sistemler için açık kaynaklı IoT işletim sistemi |
| Cooja         | Contiki-NG tabanlı telsiz ağ simülatörü                 |
| Z1 Mote       | MSP430F2617 işlemci — 8KB RAM, 92KB Flash               |
| RPL           | IPv6 tabanlı düşük güç ağ yönlendirme protokolü         |
| 6LoWPAN       | IEEE 802.15.4 üzerinde IPv6 sıkıştırma katmanı          |
| CFS (Coffee)  | Contiki-NG'nin flash bellek dosya sistemi               |
| CRC16         | Paket bütünlüğü için hata tespit algoritması            |
| Stop-and-Wait | Window size=1 olan güvenilir sıralı iletim protokolü    |
| Docker        | msp430-gcc derleme ortamı (contiker/contiki-ng image)   |
