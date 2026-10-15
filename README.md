# taktik-manajer-sepak-bola

Prototipe manajemen sepak bola dengan match engine C++ dan viewer replay Unreal Engine 5.8.

## Progres saat ini

- Simulasi headless 22 pemain dengan dual-clock 90 menit pertandingan / 15 menit presentasi.
- Utility AI, fisika bola sederhana, C-API, dan pembungkus Python.
- Ring-buffer replay `.fsim` berisi 15 detik presentasi terakhir.
- Plugin `FootballMatchBridge` untuk membaca replay di Unreal Engine.
- Viewer dengan lapangan, pemain sederhana, bola, skor, play/pause, slider, dan kecepatan 0.5x / 1x.

![Viewer replay](FootballSim_Master/00_Specs_and_Logs/stage2_2_viewer_improved.jpg)

## Struktur

| Folder | Isi |
| --- | --- |
| `00_Specs_and_Logs` | Schema, dokumentasi kontrak data, dan laporan validasi |
| `01_SimulationCore_CPP` | Core C++, C-API, konverter SQLite, tes, dan replay contoh |
| `02_ComfyUI_AssetFactory` | Folder persiapan workflow aset |
| `03_Blender_AutoRig_Mocap` | Folder persiapan rigging dan animasi |
| `04_Exported_GameAssets` | Folder persiapan aset game |
| `05_UE5_MatchViewer_3D` | Proyek Unreal dan plugin pembaca replay |
| `06_Backend_and_ManagementUI` | Folder persiapan backend dan UI manajemen |

Seluruh folder di atas berada di `FootballSim_Master/`.

## Build core C++

Prasyarat Windows: Visual Studio Build Tools 2022 dengan workload C++, Windows SDK, CMake, dan Python 3.

```powershell
.\FootballSim_Master\01_SimulationCore_CPP\build_engine.cmd
```

Skrip mengonfigurasi CMake, membangun core dan DLL, lalu menjalankan CTest.
Kontrak input/output tersedia di `ENGINE_CONTRACT.txt` dan `FSIM_FORMAT.txt` pada folder core.
Konverter SQLite membaca sumber secara read-only; repository dan database lama tidak disertakan.

## Build dan buka viewer

Prasyarat tambahan: Unreal Engine 5.8 dan .NET Framework SDK 4.8 atau lebih baru.
Implementasi ini telah dibangun dengan UE 5.8.3.

```powershell
cd FootballSim_Master\05_UE5_MatchViewer_3D
.\build_viewer.ps1 -EngineRoot 'D:\Epic Games\UE_5.8'
.\setup_assets.ps1 -EngineRoot 'D:\Epic Games\UE_5.8'
.\test_viewer.ps1 -EngineRoot 'D:\Epic Games\UE_5.8'
.\RunViewer.cmd
```

Untuk lokasi engine lain, atur `FSIM_UE_ROOT` sebelum menjalankan `RunViewer.cmd`.
Petunjuk lengkap ada di [README_VIEWER.txt](FootballSim_Master/05_UE5_MatchViewer_3D/README_VIEWER.txt).

Kontrol: **Spasi** untuk putar/jeda, **R** untuk ulang, slider untuk seek,
serta tombol **Klip gol** dan **Akhir laga** untuk mengganti rekaman.

## Batas prototipe

Viewer saat ini memutar file replay offline. Karakter masih berbentuk sederhana;
integrasi simulasi langsung melalui DLL, animasi karakter final, paket Shipping,
dan build Android belum tersedia. Replay contoh berdurasi 15 detik presentasi
(30 detik pemutaran pada 0.5x), bukan rekaman penuh satu pertandingan.
Perilaku AI dan skor pertandingan masih memerlukan penyempurnaan.

Cache Unreal, hasil kompilasi, database lokal, kredensial, dan log mentah diabaikan oleh Git.
Dependensi `nlohmann/json` menyertakan lisensi MIT dan keterangan asal di `third_party/nlohmann/`.
