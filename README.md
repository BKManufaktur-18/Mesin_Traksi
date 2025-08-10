# Proyek Alat Traksi Medis

Selamat datang di repositori proyek Alat Traksi Medis. Proyek ini mengembangkan sistem traksi yang dikendalikan melalui antarmuka web, dirancang untuk aplikasi medis. Sistem ini menggunakan ESP32 sebagai mikrokontroler utama untuk mengendalikan motor stepper dan membaca data dari berbagai sensor, serta menyediakan antarmuka pengguna berbasis web yang intuitif.

## Daftar Isi
- [Fitur Utama](#fitur-utama)
- [Perbedaan Versi Program](#perbedaan-versi-program)
- [Komponen Hardware](#komponen-hardware)
- [Persyaratan Software](#persyaratan-software)
- [Instalasi dan Setup](#instalasi-dan-setup)
- [Penggunaan](#penggunaan)
- [Struktur Proyek](#struktur-proyek)
- [Gambar Skematik](#gambar-skematik)
- [Gambar PCB](#gambar-pcb)
- [Kontribusi](#kontribusi)
- [Lisensi](#lisensi)

## Fitur Utama

## Perbedaan Versi Program

Proyek ini menyediakan dua versi program utama, masing-masing dengan fokus yang berbeda:

### 1. `traksi_web_server_mm_ilustrasi_sensor.ino` (Versi Simulasi dengan Ilustrasi Sensor)

Versi ini dirancang untuk tujuan demonstrasi dan pengembangan awal. Meskipun motor fisik dikendalikan secara *real-time*, pembacaan sensor (beban dan arus) disimulasikan secara realistis. Ini memungkinkan pengujian antarmuka web dan logika kontrol tanpa memerlukan semua sensor fisik terhubung. Grafik sensor akan menampilkan data yang diilustrasikan, memberikan gambaran bagaimana sistem akan berperilaku dengan sensor sungguhan.

### 2. `traksi_web_server_mm.ino` (Versi Aplikasi Real dengan Sensor Sungguhan)

Ini adalah versi aplikasi penuh yang dirancang untuk penggunaan praktis. Semua pembacaan sensor (beban dari load cell HX711 dan arus dari ACS712) dilakukan dari sensor fisik yang terhubung. Versi ini memberikan data yang akurat dan *real-time* dari sistem traksi, ideal untuk implementasi akhir dan pengukuran yang presisi.


Skematik proyek ini dibuat menggunakan **EasyEDA**. Anda dapat menempatkan gambar skematik di sini.

<!-- Tempat untuk gambar skematik. Ganti placeholder ini dengan path gambar Anda. -->

![Gambar Skematik](images/schematic_image.png)
## Gambar PCB

<!-- Tempat untuk gambar PCB. Ganti placeholder ini dengan path gambar Anda. -->

![Gambar PCB](images/pcb_image.png)


### Fitur Utama

Proyek Alat Traksi Medis ini menawarkan fungsionalitas komprehensif untuk mengendalikan dan memantau perangkat traksi:

- **Kontrol Motor Stepper**: Menggunakan dua motor stepper (Motor Kepala dan Motor Pinggang) untuk menggerakkan mekanisme traksi dengan presisi. Kontrol posisi dalam satuan milimeter (mm).
- **Antarmuka Web Responsif**: Menyediakan dashboard berbasis web yang dapat diakses melalui browser di perangkat apa pun (komputer, tablet, smartphone) yang terhubung ke jaringan Wi-Fi ESP32. Antarmuka ini menampilkan data sensor secara *real-time* dan memungkinkan kontrol motor.
- **Pemantauan Sensor *Real-time***: Menampilkan data dari sensor beban (load cell) dan sensor arus (ACS712) secara grafis pada antarmuka web, memungkinkan pemantauan kondisi traksi secara langsung.
- **Kalibrasi Otomatis (Homing)**: Fitur homing otomatis untuk kedua motor, memastikan posisi awal yang akurat dan konsisten.
- **Kontrol Jarak Jauh**: Mendukung kontrol motor melalui tombol fisik eksternal (remote control) selain kontrol melalui antarmuka web.
- **Konversi Posisi ke mm**: Posisi motor dikonversi dan ditampilkan dalam satuan milimeter untuk pembacaan yang lebih intuitif dan relevan secara medis.
- **Manajemen Jaringan Wi-Fi**: ESP32 berfungsi sebagai Access Point (AP), menciptakan jaringan Wi-Fi sendiri untuk konektivitas lokal.




## Komponen Hardware

Proyek ini membutuhkan komponen hardware berikut:

- **ESP32 Development Board**: Sebagai mikrokontroler utama yang menjalankan server web dan mengendalikan semua periferal.
- **Stepper Motor (HBS57)**: Dua unit motor stepper, satu untuk 'Kepala' dan satu untuk 'Pinggang'.
- **Driver Motor Stepper**: Driver yang kompatibel dengan motor HBS57 (misalnya, HBS57 atau sejenisnya).
- **Limit Switch**: Dua unit limit switch, satu untuk masing-masing motor (LS_K dan LS_P).
- **Load Cell (dengan HX711 Amplifier)**: Dua unit load cell untuk mengukur beban, masing-masing terhubung ke modul HX711.
- **ACS712 Current Sensor**: Dua unit sensor arus ACS712 untuk memantau konsumsi arus motor.
- **Tombol Remote Control**: Dua tombol push button untuk kontrol jarak jauh (REMOTE_LEFT dan REMOTE_RIGHT).
- **LED**: Satu LED indikator.
- **Kabel Jumper dan Breadboard (opsional)**: Untuk koneksi antar komponen.
- **Power Supply**: Catu daya yang sesuai untuk ESP32, motor stepper, dan sensor.

### Konfigurasi Pin (Berdasarkan `traksi_web_server_mm_ilustrasi_sensor.ino` dan `traksi_web_server_mm.ino`)

| Komponen               | Pin ESP32 (traksi_web_server_mm_ilustrasi_sensor.ino) | Pin ESP32 (traksi_web_server_mm.ino) |
|------------------------|-------------------------------------------------------|--------------------------------------|
| LED_PIN                | 2                                                     | 2                                    |
| **Stepper Motor Kepala** |                                                       |                                      |
| Dir Pin                | 33                                                    | 33                                   |
| Step Pin               | 32                                                    | 32                                   |
| Enable Pin             | 25                                                    | 25                                   |
| **Stepper Motor Pinggang**|                                                       |                                      |
| Dir Pin                | 27                                                    | 27                                   |
| Step Pin               | 26                                                    | 26                                   |
| Enable Pin             | 14                                                    | 14                                   |
| **Limit Switch**       |                                                       |                                      |
| LS_K (Kepala)          | 13                                                    | 13                                   |
| LS_P (Pinggang)        | 15                                                    | 15                                   |
| **Remote Control**     |                                                       |                                      |
| REMOTE_LEFT            | 4                                                     | 4                                    |
| REMOTE_RIGHT           | 21                                                    | 21                                   |
| **HX711 Load Cell 1**  |                                                       |                                      |
| DT                     | 18                                                    | 18                                   |
| SCK                    | 19                                                    | 19                                   |
| **HX711 Load Cell 2**  |                                                       |                                      |
| DT                     | 22                                                    | 22                                   |
| SCK                    | 23                                                    | 23                                   |
| **ACS712 Current Sensor 1**| 35                                                    | 35                                   |
| **ACS712 Current Sensor 2**| 34                                                    | 34                                   |




## Persyaratan Software

Untuk mengkompilasi dan mengunggah program ke ESP32, Anda memerlukan:

- **Arduino IDE**: Lingkungan pengembangan terintegrasi untuk Arduino dan ESP32.
- **ESP32 Board Package**: Instal paket board ESP32 untuk Arduino IDE melalui Board Manager.
- **Library Tambahan**: Instal library berikut melalui Library Manager di Arduino IDE:
    - `WiFi` (biasanya sudah termasuk dalam ESP32 Board Package)
    - `WebServer` (biasanya sudah termasuk dalam ESP32 Board Package)
    - `AccelStepper` by Mike McCauley
    - `ACS712` by Rob Tillaart
    - `HX711` by Bogde
    - `ArduinoJson` by Benoit Blanchon (versi 6 atau lebih baru)




## Instalasi dan Setup

1.  **Instal Arduino IDE**: Unduh dan instal Arduino IDE dari [situs resmi Arduino](https://www.arduino.cc/en/software).
2.  **Instal ESP32 Board Package**: 
    - Buka Arduino IDE.
    - Pergi ke `File > Preferences`.
    - Di kolom `Additional Board Manager URLs`, tambahkan URL berikut: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
    - Klik `OK`.
    - Pergi ke `Tools > Board > Board Manager...`.
    - Cari "esp32" dan instal `esp32 by Espressif Systems`.
3.  **Instal Library yang Dibutuhkan**: 
    - Buka Arduino IDE.
    - Pergi ke `Sketch > Include Library > Manage Libraries...`.
    - Cari dan instal library berikut:
        - `AccelStepper`
        - `ACS712`
        - `HX711`
        - `ArduinoJson` (pastikan versi 6 atau lebih baru)
4.  **Buka Proyek**: 
    - Unduh repositori ini ke komputer Anda.
    - Buka salah satu file `.ino` (misalnya, `traksi_web_server_mm.ino`) di Arduino IDE.
5.  **Konfigurasi Wi-Fi**: 
    - Anda dapat mengubah SSID dan password Access Point di awal file `.ino` jika diperlukan:
    ```cpp
    const char* ssid = "TraksiMonitor";
    const char* password = "12345678";
    ```
6.  **Konfigurasi Kalibrasi Motor**: 
    - Sesuaikan konstanta kalibrasi motor (`STEPS_PER_MM_KEPALA`, `STEPS_PER_MM_PINGGANG`, `MAX_TRAVEL_MM_KEPALA`, `MAX_TRAVEL_MM_PINGGANG`) sesuai dengan spesifikasi mekanik sistem traksi Anda.
    ```cpp
    const float STEPS_PER_MM_KEPALA = 84.8;     // Steps per mm untuk motor kepala
    const float STEPS_PER_MM_PINGGANG = 84.8;   // Steps per mm untuk motor pinggang
    const float MAX_TRAVEL_MM_KEPALA = 200.0;   // Max travel dalam mm untuk kepala
    const float MAX_TRAVEL_MM_PINGGANG = 150.0; // Max travel dalam mm untuk pinggang
    ```
7.  **Hubungkan ESP32**: Sambungkan ESP32 Anda ke komputer menggunakan kabel USB.
8.  **Pilih Board dan Port**: Di Arduino IDE, pergi ke `Tools > Board` dan pilih `ESP32 Dev Module` (atau board ESP32 yang sesuai). Kemudian, pergi ke `Tools > Port` dan pilih port serial yang terhubung dengan ESP32 Anda.
9.  **Unggah Program**: Klik tombol `Upload` (panah kanan) di Arduino IDE untuk mengkompilasi dan mengunggah program ke ESP32.




## Penggunaan

Setelah program berhasil diunggah ke ESP32 dan perangkat keras terhubung dengan benar, ikuti langkah-langkah berikut untuk menggunakan sistem:

1.  **Nyalakan Perangkat**: Pastikan ESP32 dan semua komponen terhubung dan mendapatkan daya yang cukup.
2.  **Koneksi Wi-Fi**: 
    - Dari perangkat Anda (komputer, smartphone, tablet), cari jaringan Wi-Fi dengan nama `TraksiMonitor` (atau SSID yang Anda konfigurasikan).
    - Hubungkan ke jaringan tersebut menggunakan password `12345678` (atau password yang Anda konfigurasikan).
3.  **Akses Antarmuka Web**: 
    - Buka browser web Anda.
    - Ketik `192.168.4.1` di bilah alamat dan tekan Enter. Ini akan membuka dashboard kontrol traksi.
4.  **Homing Otomatis**: 
    - Setelah antarmuka web terbuka, sistem akan mencoba melakukan homing otomatis untuk kedua motor. Pastikan tidak ada halangan di jalur motor dan limit switch dapat dijangkau.
    - Proses homing akan menggerakkan motor hingga menyentuh limit switch untuk menentukan posisi `0mm`.
    - Status homing akan ditampilkan di dashboard.
5.  **Kontrol Motor**: 
    - Di dashboard, Anda dapat memilih motor yang ingin dikendalikan (Kepala atau Pinggang) menggunakan tombol radio.
    - Gunakan tombol `+10mm`, `-10mm`, `+1mm`, `-1mm`, `+0.1mm`, `-0.1mm` untuk menggerakkan motor ke posisi yang diinginkan.
    - Tombol `STOP` akan menghentikan gerakan motor saat ini.
    - Tombol `HOME` akan mengembalikan motor ke posisi `0mm`.
    - Anda juga dapat menggunakan tombol remote control fisik yang terhubung ke pin `REMOTE_LEFT` dan `REMOTE_RIGHT` untuk menggerakkan motor.
6.  **Pemantauan Data Sensor**: 
    - Grafik `Force (Kg)` akan menampilkan pembacaan beban dari load cell secara *real-time*.
    - Grafik `Current (A)` akan menampilkan pembacaan arus motor secara *real-time*.
    - Nilai posisi motor (dalam mm), status limit switch, dan status motor juga akan diperbarui secara dinamis di dashboard.

### Perbedaan Penggunaan antara Dua Program:

-   **`traksi_web_server_mm_ilustrasi_sensor.ino`**: Saat menggunakan program ini, grafik `Force (Kg)` dan `Current (A)` akan menampilkan data yang disimulasikan. Ini berguna untuk memahami fungsionalitas antarmuka web dan kontrol motor tanpa perlu mengintegrasikan sensor fisik.
-   **`traksi_web_server_mm.ino`**: Saat menggunakan program ini, grafik `Force (Kg)` dan `Current (A)` akan menampilkan data aktual dari sensor load cell dan sensor arus yang terhubung ke ESP32 Anda. Pastikan sensor terkalibrasi dengan benar untuk mendapatkan pembacaan yang akurat.




## Struktur Proyek

Repositori ini berisi file-file utama berikut:

-   `traksi_web_server_mm_ilustrasi_sensor.ino`: Kode program untuk ESP32 dengan simulasi data sensor.
-   `traksi_web_server_mm.ino`: Kode program untuk ESP32 dengan pembacaan sensor *real-time*.
-   `README.md`: File ini, berisi dokumentasi proyek.

File `.ino` berisi kode C++ untuk mikrokontroler ESP32, termasuk konfigurasi Wi-Fi, kontrol motor stepper, pembacaan sensor (simulasi atau real), dan implementasi server web untuk antarmuka pengguna.




## Kontribusi

Kontribusi sangat dihargai! Jika Anda memiliki saran, perbaikan, atau fitur baru, silakan buka *issue* atau kirim *pull request*.

## Lisensi

Proyek ini dilisensikan di bawah [MIT License](https://opensource.org/licenses/MIT).



## Komponen Hardware

Proyek ini membutuhkan komponen hardware berikut untuk fungsionalitas penuh:

-   **ESP32 Development Board**: Berfungsi sebagai otak sistem, mengelola koneksi Wi-Fi, mengendalikan motor, dan memproses data sensor. ESP32 dipilih karena kapabilitas Wi-Fi terintegrasi dan performa yang memadai untuk aplikasi ini.
-   **Stepper Motor (HBS57)**: Dua unit motor stepper HBS57 digunakan untuk menggerakkan mekanisme traksi. Satu motor didedikasikan untuk pergerakan 'Kepala' dan motor lainnya untuk pergerakan 'Pinggang'. Motor stepper ini dipilih karena presisi dalam kontrol posisi, yang krusial untuk aplikasi traksi medis.
-   **Driver Motor Stepper**: Driver yang kompatibel dengan motor HBS57 (misalnya, driver HBS57 yang sesuai) diperlukan untuk mengendalikan motor stepper. Driver ini menerjemahkan sinyal langkah dan arah dari ESP32 menjadi gerakan motor yang presisi.
-   **Limit Switch**: Dua unit limit switch digunakan sebagai sensor posisi home untuk masing-masing motor (LS_K untuk motor Kepala dan LS_P untuk motor Pinggang). Limit switch ini memastikan motor dapat kembali ke posisi awal yang diketahui dan aman, mencegah kerusakan akibat pergerakan berlebihan.
-   **Load Cell (dengan HX711 Amplifier)**: Dua unit load cell digunakan untuk mengukur gaya atau beban yang diterapkan selama proses traksi. Setiap load cell terhubung ke modul amplifier HX711, yang mengubah perubahan resistansi kecil pada load cell menjadi sinyal digital yang dapat dibaca oleh ESP32. Sensor ini vital untuk memantau kekuatan traksi secara akurat.
-   **ACS712 Current Sensor**: Dua unit sensor arus ACS712 digunakan untuk memantau konsumsi arus motor. Sensor ini membantu dalam mendeteksi anomali atau beban berlebih pada motor, yang dapat mengindikasikan masalah mekanis atau operasional. Sensor ACS712 memberikan pembacaan arus yang akurat dan terisolasi.
-   **Tombol Remote Control**: Dua tombol push button digunakan sebagai opsi kontrol jarak jauh (REMOTE_LEFT dan REMOTE_RIGHT). Tombol-tombol ini memungkinkan operator untuk menggerakkan motor secara manual tanpa harus berinteraksi langsung dengan antarmuka web, memberikan fleksibilitas dalam penggunaan.
-   **LED**: Satu LED indikator digunakan untuk memberikan umpan balik visual sederhana mengenai status perangkat atau operasi tertentu.
-   **Kabel Jumper dan Breadboard (opsional)**: Digunakan untuk memfasilitasi koneksi antar komponen selama fase prototipe dan pengembangan.
-   **Power Supply**: Catu daya yang sesuai dan stabil sangat penting untuk menyediakan daya yang cukup bagi ESP32, motor stepper, dan semua sensor. Pemilihan catu daya harus mempertimbangkan kebutuhan daya puncak motor untuk menghindari masalah performa.

### Konfigurasi Pin Hardware

Berikut adalah tabel yang merinci konfigurasi pin antara ESP32 dan berbagai komponen hardware yang digunakan dalam proyek ini. Konfigurasi ini konsisten di kedua versi program (`traksi_web_server_mm_ilustrasi_sensor.ino` dan `traksi_web_server_mm.ino`).

| Komponen                     | Pin ESP32 |
|:-----------------------------|:----------|
| `LED_PIN`                    | 2         |
| **Stepper Motor Kepala**     |           |
| `dirPin_Kepala` (Direction)  | 33        |
| `stepPin_Kepala` (Step)      | 32        |
| `enablePin_Kepala` (Enable)  | 25        |
| **Stepper Motor Pinggang**   |           |
| `dirSpPin` (Direction)       | 27        |
| `stepSpPin` (Step)           | 26        |
| `enableSpPin` (Enable)       | 14        |
| **Limit Switch**             |           |
| `LS_K` (Kepala)              | 13        |
| `LS_P` (Pinggang)            | 15        |
| **Remote Control**           |           |
| `REMOTE_LEFT`                | 4         |
| `REMOTE_RIGHT`               | 21        |
| **HX711 Load Cell 1**        |           |
| `HX711_DT_1` (Data)          | 18        |
| `HX711_SCK_1` (Clock)        | 19        |
| **HX711 Load Cell 2**        |           |
| `HX711_DT_2` (Data)          | 22        |
| `HX711_SCK_2` (Clock)        | 23        |
| **ACS712 Current Sensor 1**  | 35        |
| **ACS712 Current Sensor 2**  | 34        |

Pastikan semua koneksi dilakukan dengan benar sesuai dengan tabel ini untuk memastikan fungsionalitas sistem yang optimal.




## Persyaratan Software

Untuk dapat mengkompilasi, mengunggah, dan menjalankan program pada ESP32, Anda memerlukan lingkungan pengembangan perangkat lunak tertentu dan beberapa library tambahan. Berikut adalah daftar persyaratan software yang harus dipenuhi:

-   **Arduino IDE**: Lingkungan Pengembangan Terintegrasi (IDE) Arduino adalah platform utama yang digunakan untuk menulis, mengkompilasi, dan mengunggah kode ke board ESP32. Pastikan Anda mengunduh versi terbaru dari [situs resmi Arduino](https://www.arduino.cc/en/software) untuk kompatibilitas terbaik.

-   **ESP32 Board Package**: Agar Arduino IDE dapat mengenali dan berkomunikasi dengan board ESP32, Anda perlu menginstal paket board ESP32. Ini dilakukan melalui Board Manager di Arduino IDE, yang menambahkan definisi board, toolchain kompilasi, dan library inti untuk ESP32. Proses instalasi melibatkan penambahan URL JSON ke preferensi Arduino IDE dan kemudian menginstal paket dari Board Manager.

-   **Library Tambahan**: Proyek ini bergantung pada beberapa library pihak ketiga yang menyediakan fungsionalitas spesifik untuk mengendalikan hardware dan mengelola komunikasi. Library-library ini harus diinstal melalui Library Manager di Arduino IDE:
    -   `WiFi`: Library standar untuk mengelola konektivitas Wi-Fi pada ESP32, termasuk mode Access Point yang digunakan dalam proyek ini. Library ini biasanya sudah termasuk dalam instalasi ESP32 Board Package.
    -   `WebServer`: Library ini memungkinkan ESP32 untuk bertindak sebagai server web, melayani halaman HTML dan menangani permintaan HTTP dari klien. Ini adalah komponen kunci untuk antarmuka pengguna berbasis web. Library ini juga umumnya sudah termasuk dalam ESP32 Board Package.
    -   `AccelStepper` by Mike McCauley: Library ini menyediakan kontrol yang canggih dan presisi untuk motor stepper, termasuk akselerasi dan deselerasi yang mulus, serta kontrol posisi absolut. Ini sangat penting untuk pergerakan motor traksi yang akurat.
    -   `ACS712` by Rob Tillaart: Library ini memfasilitasi pembacaan data dari sensor arus ACS712. Ini menyederhanakan proses konversi tegangan analog dari sensor menjadi nilai arus yang dapat digunakan.
    -   `HX711` by Bogde: Library ini dirancang khusus untuk berinteraksi dengan modul amplifier HX711 yang digunakan bersama load cell. Ini menangani komunikasi data serial dari HX711 dan mengkonversi pembacaan mentah menjadi nilai berat yang terkalibrasi.
    -   `ArduinoJson` by Benoit Blanchon: Library ini digunakan untuk memparsing dan menghasilkan data dalam format JSON. Dalam proyek ini, JSON digunakan untuk komunikasi data antara ESP32 dan antarmuka web, memungkinkan pertukaran data sensor dan perintah kontrol yang efisien. Pastikan untuk menginstal versi 6 atau yang lebih baru untuk kompatibilitas dan fitur terbaru.

Memastikan semua persyaratan software ini terpenuhi akan memungkinkan Anda untuk mengkompilasi kode tanpa kesalahan dan mengunggahnya ke ESP32 dengan sukses.



