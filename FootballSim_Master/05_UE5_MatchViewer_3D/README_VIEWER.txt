FootballSim Broadcast Replay Viewer - Tahap 3.2

Menjalankan viewer
RunViewer.cmd membuka game windowed UE5.8.3. Highlight 900 detik dimuat dalam
keadaan jeda. Putar/Jeda: tombol layar atau Spasi. Ulang: tombol atau R.
Slider memilih waktu highlight. Tombol 1x/0.5x mengubah laju pemutaran.
Segmen replay otomatis mempunyai laju 0.5x dan kamera rendah.
RunViewer.cmd -ReplayAutoplay memulai pemutaran otomatis.
RunViewer.cmd -ReplayFile="D:\path\rekaman.fsim" memuat rekaman lain.
FSIM_UE_ROOT menentukan lokasi engine jika berbeda dari D:\Epic Games\UE_5.8.

Kamera dan scene
Kamera utama perspektif FOV 35, target X=0.75*X bola, Y=-5000cm, Z=1650cm.
Interpolasi posisi dan rotasi mengikuti bola; pitch dibatasi -23 sampai -26.
Koreksi proyeksi vertikal menjaga bola masuk frame meskipun bola di sisi jauh.
Replay memakai FOV 50, Z=240cm, dan jarak 800-1200cm dari bola, di sisi aksi
sehingga jaring tidak menghalangi pandangan. Kamera melakukan cut saat ganti
segmen dan beralih kembali ke broadcast setelah replay.
22 skeletal mesh memakai basis NicoPaz (tinggi bind pose 180cm), tekstur PBR,
warna kit per tim, serta animasi gerak/aksi/kiper yang tersedia. Posisi dari
FSIM tetap menjadi otoritas; perpindahan Hips bawaan animasi dinetralkan di
bidang horizontal agar mesh dan indikator penguasaan bola tidak bergeser.
Tidak ada token besar atau angka di atas kepala. Hanya penguasa bola memiliki
cincin tipis. Radar 240x150 unit Slate berada di bawah tengah, skor di kiri
atas, nama pemain dari metadata di kedua pojok bawah.
Stadion prosedural meliputi rumput lit, garis lapangan, gawang berjaring,
adboard, tribun bertingkat, bangku dan penonton sederhana. Matahari dinamis
berpitch -40 derajat menghasilkan bayangan; skylight mengisi sisi gelap.

Data
FSIM v1 tetap didukung untuk klip ring buffer. FSIM v2 menyimpan 18001 frame
sumber 90 menit dengan urutan segmen highlight 900 detik. Segmen normal
memakai detik pertandingan 1x; replay otomatis 0.5x sudah dihitung dalam
anggaran 900 detik. Skor dan jam mengikuti sumber, termasuk lompatan waktu.
Konversi FSIM metre x/y-up/z -> UE centimetre X/Y/Z-up = 100*(x,z,y).
Reader dan playback tetap berada dalam plugin FootballMatchBridge.
Tidak memerlukan DLL simulasi untuk playback offline.

Build dan aset
1. powershell -File build_viewer.ps1
2. powershell -File setup_assets.ps1
3. powershell -File test_viewer.ps1
4. RunViewer.cmd
Prasyarat: UE5.8, MSVC 2022 x64, Windows SDK, .NET Framework SDK 4.8+.
Gunakan -EngineRoot pada skrip PowerShell bila lokasi engine berbeda.
setup_assets.py mengimpor FBX dengan importer legacy, menyimpan skeleton
beserta dependensinya, 27 AnimSequence dan LOD0-LOD2, lalu membuat material
rumput/kit/langit. Tekstur PNG tersedia di 04_Exported_GameAssets/kits_textures.
Untuk mengekspor ulang dari sumber lokal, jalankan script Blender headless
export_assets_to_ue5.py dan export_character_textures.py. Sumber lama hanya
dibaca dan tidak disimpan ulang.

Screenshot tanpa UI
powershell -File capture_stage3_2.ps1
Proses game berjalan tersembunyi dengan -RenderOffscreen; tidak mengirim
input mouse/keyboard dan menutup diri setelah selesai. FScreenshotRequest
menyimpan dua PNG 1600x900 dan diagnostik JSON di ../00_Specs_and_Logs:
stage3_2_broadcast_3d.png dan stage3_2_replay_closeup.png.
Opsional -BroadcastAt / -CloseupAt memilih detik timeline untuk capture.
Skrip menolak screenshot lama, shader gagal kompilasi, atau skeleton hilang.

Validasi
5 tes Unreal: reader/corrupt input, pause/rate/seek, timeline highlight,
framing kamera di kedua touchline/garis gawang, serta skeleton dan animasi
persisten. Log di Saved/Stage2_2_Tests.log dan Saved/Automation/.
Laporan stage3_2_report.json menyimpan hasil verifikasi render dan build.

Batas
Stadion/penonton masih prosedural. Semua pemain memakai mesh dasar yang sama;
nama pada sampel berasal dari roster sintetis, bukan roster pemain asli.
Belum ada integrasi live C-API, build Android, atau paket Shipping.
