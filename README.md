# Alat Terapi Traksi 🏥

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Status](https://img.shields.io/badge/status-Development-yellow.svg)]()
[![University](https://img.shields.io/badge/Institution-ITS-red.svg)]()

## 📖 Deskripsi

Alat Terapi Traksi adalah perangkat medis yang dikembangkan sebagai bagian dari penelitian **Prof. I Made Londen** dari Departemen Teknik Mesin, Institut Teknologi Sepuluh Nopember (ITS). Proyek ini bertujuan untuk mengembangkan sistem traksi otomatis yang dapat membantu dalam terapi rehabilitasi medis dengan kontrol yang presisi dan aman.

Traksi terapi adalah metode pengobatan yang menggunakan tarikan atau gaya untuk memperbaiki posisi tulang, mengurangi tekanan pada saraf, dan mempercepat proses penyembuhan pada cedera muskuloskeletal.

## 🎯 Tujuan Proyek

- Mengembangkan alat terapi traksi yang aman dan efektif
- Memberikan kontrol presisi dalam pemberian gaya traksi
- Meningkatkan kenyamanan pasien selama terapi
- Menyediakan monitoring real-time pada parameter terapi
- Mengintegrasikan sistem otomatis dengan safety features

## ⚡ Fitur Utama

- **Kontrol Otomatis**: Sistem kontrol berbasis mikrokontroler untuk pengaturan gaya traksi
- **Monitoring Real-time**: Display parameter terapi seperti gaya, waktu, dan status sistem
- **Safety System**: Sistem keamanan berlapis untuk melindungi pasien
- **Interface User-Friendly**: Panel kontrol yang mudah digunakan oleh tenaga medis
- **Data Logging**: Pencatatan data sesi terapi untuk analisis
- **Adjustable Parameters**: Pengaturan parameter terapi sesuai kebutuhan pasien

## 🔧 Komponen Sistem

### Hardware
- Mikrokontroler (Arduino/ESP32)
- Motor servo/stepper untuk kontrol gaya
- Load cell untuk sensor gaya
- Display LCD/OLED
- Push button dan rotary encoder
- Safety switch dan emergency stop
- Power supply dan driver motor

### Software
- Firmware mikrokontroler (C/C++)
- Interface monitoring (Python/GUI)
- Sistem logging data
- Algoritma kontrol PID

## 📱 Dokumentasi Visual

### Skematik Rangkaian
![Skematik Elektronik](images/schematic.png)
*Diagram skematik sistem elektronik alat terapi traksi*

### PCB Design
![PCB Layout](images/pcb_layout.png)
*Layout PCB untuk sistem kontrol*

![PCB 3D View](images/pcb_3d.png)
*Visualisasi 3D PCB*

### Mechanical Design
![Mechanical Assembly](images/mechanical_design.png)
*Desain mekanik alat terapi traksi*

### Prototype
![Prototype Image](images/prototype.png)
*Foto prototype alat terapi traksi*

## 🚀 Instalasi dan Setup

### Persyaratan
- Arduino IDE atau PlatformIO
- Library yang diperlukan (lihat `requirements.txt`)
- Komponen hardware sesuai BOM (Bill of Materials)

### Langkah Instalasi

1. **Clone Repository**
   ```bash
   git clone https://github.com/BKManufaktur-18/Mesin_Traksi.git
   cd Mesin_Traksi
   ```

2. **Install Dependencies**
   ```bash
   # Untuk Arduino IDE
   # Install library melalui Library Manager:
   # - AccelStepper
   # - LiquidCrystal_I2C
   # - HX711 (untuk load cell)
   ```

3. **Upload Firmware**
   ```bash
   # Buka file main.ino di Arduino IDE
   # Pilih board dan port yang sesuai
   # Upload ke mikrokontroler
   ```

4. **Assembly Hardware**
   - Ikuti skematik rangkaian
   - Pasang komponen sesuai layout PCB
   - Lakukan kalibrasi sensor

## 📋 Cara Penggunaan

### Persiapan
1. Pastikan semua koneksi hardware sudah benar
2. Lakukan kalibrasi sistem sebelum penggunaan
3. Periksa safety system berfungsi dengan baik

### Operasi
1. **Power On**: Nyalakan sistem dan tunggu inisialisasi
2. **Setup Parameter**: Atur parameter terapi (gaya, waktu, mode)
3. **Safety Check**: Verifikasi semua safety system aktif
4. **Start Therapy**: Mulai sesi terapi dengan monitoring kontinyu
5. **Monitor**: Pantau parameter selama terapi berlangsung
6. **Stop/Emergency**: Gunakan emergency stop jika diperlukan

### Parameter Terapi
- **Gaya Traksi**: 0-50 N (dapat disesuaikan)
- **Durasi**: 1-60 menit
- **Mode**: Kontinu, Intermiten, atau Custom
- **Safety Limit**: Batas maksimum gaya dan waktu

## 📊 Spesifikasi Teknis

| Parameter | Spesifikasi |
|-----------|-------------|
| Input Voltage | 12V DC |
| Konsumsi Daya | < 50W |
| Akurasi Gaya | ±0.5 N |
| Range Gaya | 0-50 N |
| Interface | LCD 20x4, Push Button |
| Komunikasi | Serial, Bluetooth (opsional) |
| Dimensi | 400x300x200 mm |
| Berat | ~5 kg |

## 🔬 Penelitian dan Testing

### Metodologi Testing
- Pengujian akurasi sensor gaya
- Testing safety system
- Evaluasi performa motor control
- Analisis stabilitas sistem
- User acceptance testing dengan tenaga medis

### Hasil Penelitian
Dokumentasi hasil penelitian tersedia di folder `/research/` meliputi:
- Data pengujian laboratory
- Analisis performa sistem
- Feedback dari praktisi medis
- Rekomendasi pengembangan

## 📁 Struktur Project

```
Mesin_Traksi/
├── src/
│   ├── main.ino              # Main firmware
│   ├── control.cpp           # Motor control functions
│   ├── sensor.cpp            # Sensor reading functions
│   └── display.cpp           # Display functions
├── hardware/
│   ├── schematic/            # File skematik (Eagle/KiCad)
│   ├── pcb/                  # Design PCB
│   ├── bom.xlsx              # Bill of Materials
│   └── assembly_guide.pdf    # Panduan assembly
├── mechanical/
│   ├── cad_files/            # File CAD (SolidWorks/Fusion360)
│   └── drawings/             # Technical drawings
├── docs/
│   ├── user_manual.pdf       # Manual pengguna
│   ├── technical_spec.pdf    # Spesifikasi teknis
│   └── safety_guide.pdf      # Panduan keamanan
├── images/
│   ├── schematic.png
│   ├── pcb_layout.png
│   ├── pcb_3d.png
│   ├── mechanical_design.png
│   └── prototype.png
├── research/
│   ├── test_data/            # Data hasil pengujian
│   ├── analysis/             # Analisis hasil
│   └── publications/         # Paper dan publikasi
└── README.md
```

## ⚠️ Keamanan dan Peringatan

**PENTING**: Alat ini adalah perangkat medis yang harus digunakan dengan pengawasan tenaga medis profesional.

### Safety Features
- Emergency stop button
- Force limiting system
- Auto-shutdown pada error
- Visual dan audio alarm
- Backup power system

### Peringatan Penggunaan
- Jangan gunakan tanpa supervisi medis
- Periksa kalibrasi secara berkala
- Lakukan maintenance sesuai jadwal
- Jangan modifikasi tanpa persetujuan peneliti
- Laporkan segera jika ada malfungsi

## 🤝 Kontribusi

Proyek ini terbuka untuk kontribusi dari komunitas, terutama:
- Improvement pada algoritma kontrol
- Enhancement user interface
- Testing dan quality assurance
- Dokumentasi dan tutorial
- Optimization performa

### Guidelines Kontribusi
1. Fork repository
2. Buat feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit perubahan (`git commit -m 'Add some AmazingFeature'`)
4. Push ke branch (`git push origin feature/AmazingFeature`)
5. Buat Pull Request

## 📞 Tim Peneliti

- **Principal Investigator**: Prof. I Made Londen - Teknik Mesin ITS
- **Developer**: BKManufaktur-18 Team
- **Institution**: Institut Teknologi Sepuluh Nopember (ITS)

### Kontak
- Email: [email@its.ac.id](mailto:email@its.ac.id)
- Institution: [Teknik Mesin ITS](https://me.its.ac.id/)
- Research Group: Medical Device Development

## 📄 Lisensi

Project ini dilisensikan under MIT License - lihat file [LICENSE](LICENSE) untuk detail.

## 🙏 Acknowledgments

- Institut Teknologi Sepuluh Nopember (ITS)
- Departemen Teknik Mesin ITS
- Tim peneliti Medical Device Development
- Praktisi medis yang memberikan feedback
- Open source community

## 📚 Referensi

1. Medical Traction Therapy Guidelines
2. International Medical Device Standards (ISO 13485)
3. Arduino Programming Reference
4. Control Systems Engineering Literature
5. Safety Standards for Medical Devices

---

**Catatan**: Proyek ini masih dalam tahap development. Penggunaan untuk aplikasi medis nyata harus melalui sertifikasi dan persetujuan yang sesuai dengan regulasi medis yang berlaku.

**Status**: Development Phase | **Last Updated**: August 2025
