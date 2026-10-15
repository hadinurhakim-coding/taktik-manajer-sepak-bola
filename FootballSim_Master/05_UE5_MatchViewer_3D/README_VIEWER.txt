FootballSim Replay Viewer - Tahap 2.2

Membuka viewer
Jalankan RunViewer.cmd. Jendela game memakai Unreal Engine 5.8.3 terpasang di
D:\Epic Games\UE_5.8. Proyek: FootballReplayViewer.uproject.
Klip gol pertama dimuat, mulai dalam keadaan jeda, dengan kecepatan 0.5x.
Putar/Jeda: tombol layar atau SPASI. Ulang dari awal: tombol Ulang atau R.
Slider: pilih posisi waktu klip. Tombol 0.5x/1.0x: ganti kecepatan.
Klip gol / Akhir laga: pilih salah satu dari dua rekaman contoh.
Replay berhenti di ujung klip; tekan Putar atau Ulang untuk memulai lagi.
Tutup jendela untuk keluar. Tidak menjalankan backend atau mengubah database.

File lain
RunViewer.cmd -ReplayFile="D:\path\rekaman.fsim"
Tambahkan -ReplayAutoplay untuk memulai otomatis.
FSIM_UE_ROOT dapat diatur pada sesi terminal untuk lokasi engine lain.
Saat memakai Unreal Editor penuh, buka .uproject lalu tekan Play.
Scene dibuat saat BeginPlay; viewport edit map Entry kosong sebelum Play.

Tampilan
Lapangan 105x68 m, garis, dua gawang, kamera keseluruhan lapangan.
Kamera menyesuaikan ukuran jendela agar lapangan besar dan tetap utuh di antara
panel skor dan kontrol. Garis putih 24 cm memakai material tanpa pencahayaan
di atas rumput hijau gelap untuk menjaga kontras pada sudut kamera jauh.
22 pemain sederhana: home biru, away merah; kiper kuning dan ungu.
Tubuh, nomor, dan alas pemain diperbesar; alas warna tim diberi tepi gelap.
Nomor di atas pemain menunjukkan slot lineup 1..11, bukan nomor jersey sumber
karena metadata FSIM v1 hanya menyimpan ID dan indeks lineup.
Bola putih dengan panel gelap berdiameter visual 70 cm, dengan cincin kuning
di tanah yang mengikuti posisi horizontal bola, termasuk saat bola melambung.
Pembesaran objek hanya untuk keterbacaan; koordinat replay tetap sama.
Skor dan jam pertandingan berasal dari frame, terpisah dari waktu klip.
Indikator GOL muncul berdasarkan penanda kejadian dalam rekaman.
Tidak ada model karakter final, rig, animasi kaki, atau stadion produksi.

Waktu dan interpolasi
Input setiap 0.05 detik presentasi; klip 301 frame mencakup 15 detik.
Pada 0.5x klip tersebut berlangsung 30 detik waktu pemutaran.
Ini replay jendela 15 detik terakhir dari simulasi, bukan video seluruh laga.
Posisi pemain/bola diinterpolasi linear. Skor mengikuti frame secara diskret.
Pada flag restart atau pergantian babak, posisi ditahan hingga frame baru lalu
berpindah, sehingga tidak diinterpolasi melintasi reset posisi.
FSIM metre x/y-up/z -> Unreal cm X/Y/Z-up = 100*(x,z,y).

Struktur
Plugins/FootballMatchBridge/: plugin Runtime, reader binary dan playback model.
  Public/FSimReplay.h dan Private/FSimReplay.cpp mendefinisikan kontrak reader.
  Private/FSimReplayTests.cpp memuat tes native Unreal Automation.
Source/FootballReplayViewer/: scene, kamera, Slate HUD, kontrol input.
Content/Replays/: salinan klip .fsim dari tahap 2.1; sumber asli dipertahankan.
Content/Viewer/M_ViewerColor.uasset: material berparameter Tint.
Content/Viewer/M_ViewerFlat.uasset: material tanpa pencahayaan untuk kontras
rumput, garis, dan penanda. Eksposur kamera tetap dan bloom dimatikan.
Reader tidak memerlukan match_bridge.dll/.lib. Integrasi live engine C-API
dapat ditambahkan ke plugin yang sama pada tahap berikutnya.

Build ulang
1. powershell -File build_viewer.ps1
2. powershell -File setup_assets.ps1  (membuat kedua material yang belum ada)
3. powershell -File test_viewer.ps1
4. RunViewer.cmd
Prasyarat: UE5.8, MSVC 2022 C++ x64, Windows SDK, .NET Framework SDK 4.8+
(SwarmInterface editor), bundled .NET runtime dari instalasi Unreal.
Gunakan -EngineRoot pada ketiga skrip PowerShell bila engine berada di lokasi lain.
Hasil build Editor ada di Binaries/Win64 dan Plugins/FootballMatchBridge/Binaries.
Belum dibuat paket game shipping atau build Android.

Validasi dan log
Saved/Stage2_2_UBT.log: hasil kompilasi UnrealBuildTool.
Saved/Stage2_2_Assets.log: setup material.
Saved/Stage2_2_Tests.log dan Saved/Automation/: hasil Unreal Automation.
Tes: fixture FSIM asli, file korup, layout endian, identitas, transform sumbu,
0.5x, jeda, seek, batas akhir, interpolasi, dan diskontinuitas restart.
Laporan akhir dan bukti tampilan disimpan di ../00_Specs_and_Logs/.
