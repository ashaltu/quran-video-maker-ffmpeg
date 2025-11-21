class QvmFfmpeg < Formula
  desc "Quran Video Maker (FFmpeg)"
  homepage "https://github.com/ashaltu/quran-video-maker-ffmpeg"
  # Release tarball URL - includes extracted data/ directory
  url "https://github.com/ashaltu/quran-video-maker-ffmpeg/releases/download/v0.0.0-test1-c/qvm-ffmpeg-v0.0.0-test1-c.tar.gz"
  sha256 "e0c29bf9c3d7e6ceec004b485e3b8c0d998af217e4c7e54c14c0ddbed3df8044"

  depends_on "cmake" => :build
  depends_on "pkg-config" => :build
  depends_on "curl"
  depends_on "ffmpeg"
  depends_on "freetype"
  depends_on "harfbuzz"

  def install
    # Extract data.tar if it exists (fallback for older releases)
    system "tar", "-xf", "data.tar" if File.exist?("data.tar")

    # Build and install
    system "cmake", "-S", ".", "-B", "build", *std_cmake_args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end

  def caveats
    <<~EOS
      To use qvm-ffmpeg, you need to specify the config and data paths:

        quran-video-maker --config #{share}/qvm-ffmpeg/config.json <surah> <from> <to>

      Or copy the config to your working directory and modify paths:

        cp #{share}/qvm-ffmpeg/config.json ./config.json

      Then update assetFolderPath and quranWordByWordPath in config.json
      to point to #{share}/qvm-ffmpeg/assets and #{share}/qvm-ffmpeg/data
    EOS
  end

  test do
    system "#{bin}/quran-video-maker", "--help"
  end
end
