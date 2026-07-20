# Pardus Hardware Control and Optimization Center

[Türkçe](#türkçe) | [English](#english)

## Türkçe

Pardus için donanım izleme ve sistem optimizasyon aracı. İki ayrı bileşenden oluşur: bir masaüstü uygulaması (donanım kullanımını gösterir) ve arka planda çalışan bir sistem servisi (arka plan kullanıcı oturumlarını kısıtlayarak ve önbelleği boşaltarak sistemi optimize eder).

### Nasıl çalışır

1. **Optimization Control Center** uygulaması CPU, RAM, Disk, GPU ve Ağ kullanımını saniyede bir günceller. Herhangi bir karta tıklamak CPU-Z tarzı detaylı bir pencere açar (model, çekirdek/iş parçacığı sayısı, önbellek seviyeleri, saat hızları, dosya sistemleri, ağ arayüzleri ve dahası).
2. En çok RAM kullanan 5 işlem ayrı bir listede gösterilir; her biri kapatılabilir (SIGTERM), önceliği düşürülebilir (renice 19) veya çalıştığı klasör dosya yöneticisinde açılabilir.
3. **pardus-optimizer** servisi arka planda `systemd-logind` üzerinden kullanıcı oturumlarını izler. Bir kullanıcı "Kullanıcı Değiştir" ile arka plana geçtiğinde, o kullanıcının cgroup dilimine CPU (%20) ve bellek (memory.high) kısıtı uygular; kullanıcı geri döndüğünde kısıtlamayı anında kaldırır. Sıkıştırılmış veriler ZRAM üzerinde tutulur, hiçbir uygulama veya sekme kapanmaz.
4. Servis ayrıca düzenli aralıklarla (varsayılan 10 dakika) disk önbelleğini boşaltıp belleği sıkıştırır. "Sistemi Optimize Et" butonu bu taramayı anında tetikler ve ne kadar RAM boşaldığını gösterir.

Arayüz Türkçe ve İngilizce'yi destekler, dil değişimi anında uygulanır (yeniden başlatma gerekmez).

### Proje yapısı

- `app/` — masaüstü uygulaması (donanım izleme, CPU-Z detay pencereleri, optimize butonu)
- `daemon/` — arka planda çalışan `pardus-optimizer` sistem servisi
- `app/fonts/`, `app/images/` — paylaşılan yazı tipi ve ikon dosyaları

### Derleme

Gerekli paketler:

```
sudo apt install cmake ninja-build git clang pkg-config qt6-base-dev qt6-declarative-dev qt6-tools-dev libsystemd-dev
```

Derleme:

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### .deb paketi oluşturma

```
cd build
cpack
sudo dpkg -i pardushw_1.0.0_amd64.deb
sudo apt install -f
```

Kurulum sonrası `pardus-optimizer` servisi otomatik olarak etkinleştirilir ve başlatılır (systemd üzerinden, root olarak). Masaüstü uygulaması hiçbir zaman root gerektirmez.

### Çalıştırma (kaynak derlemesinden)

Servisi test etmek için (kurulum yapılmadan) root gerekir:

```
sudo ./build/PardusOptimizer
```

Uygulamayı açmak için:

```
./build/"Optimization Control Center"
```

### Mimari notu

Masaüstü uygulaması hiçbir zaman root ile çalışmaz. "Sistemi Optimize Et" butonu `/run/pardus-optimizer.trigger` adında dünya-yazılabilir (world-writable) bir named pipe'a tek bayt yazar; bu pipe'ı yalnızca root olarak çalışan `pardus-optimizer` servisi okur ve ayrıcalıklı işlemleri (cgroup yazma, önbellek boşaltma) gerçekleştirir. Kullanıcı asla parola girmez.

### Lisans

GPL-3

## English

A hardware monitoring and system optimization tool for Pardus. It has two separate components: a desktop app (shows live hardware usage) and a background system service (optimizes the system by throttling backgrounded user sessions and clearing caches).

### How it works

1. **Optimization Control Center** updates CPU, RAM, Disk, GPU and Network usage every second. Clicking any tile opens a CPU-Z-style detail window (model, core/thread count, cache levels, clock speeds, filesystems, network interfaces, and more).
2. The top 5 RAM-heavy processes are shown in a separate list; each can be killed (SIGTERM), deprioritized (renice 19), or have its containing folder opened in the file manager.
3. The **pardus-optimizer** service watches user sessions in the background via `systemd-logind`. When a user switches away ("Switch User"), it throttles that user's cgroup slice (20% CPU, a reduced memory.high) — and lifts the restriction the instant they switch back. Reclaimed memory is compressed into ZRAM, so no app or tab is ever closed.
4. The service also periodically (every 10 minutes by default) drops disk caches and compacts memory. The "Optimize System" button triggers this sweep immediately and reports how much RAM was freed.

The interface supports Turkish and English, with instant language switching (no restart needed).

### Project layout

- `app/` — the desktop app (hardware monitoring, CPU-Z detail windows, optimize button)
- `daemon/` — the background `pardus-optimizer` system service
- `app/fonts/`, `app/images/` — shared font and icon assets

### Building

Required packages:

```
sudo apt install cmake ninja-build git clang pkg-config qt6-base-dev qt6-declarative-dev qt6-tools-dev libsystemd-dev
```

Build:

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Building the .deb

```
cd build
cpack
sudo dpkg -i pardushw_1.0.0_amd64.deb
sudo apt install -f
```

After install, the `pardus-optimizer` service is automatically enabled and started (via systemd, as root). The desktop app never requires root.

### Running (from a source build)

To test the service without installing it, root is required:

```
sudo ./build/PardusOptimizer
```

To open the app:

```
./build/"Optimization Control Center"
```

### Architecture note

The desktop app never runs as root. The "Optimize System" button writes a single byte to a world-writable named pipe at `/run/pardus-optimizer.trigger`; only the `pardus-optimizer` service, running as root, reads that pipe and performs the privileged work (writing cgroup files, dropping caches). The user is never prompted for a password.

### License

GPL-3
