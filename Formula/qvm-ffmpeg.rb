class QvmFfmpeg < Formula
  desc "Quran Video Maker (FFmpeg)"
  homepage "https://github.com/ashaltu/quran-video-maker-ffmpeg"
  url "https://github.com/ashaltu/quran-video-maker-ffmpeg/releases/download/v0.0.0-test4-g/qvm-ffmpeg-v0.0.0-test4-g.tar.gz"
  sha256 "e25421e7b3376a35a1e1531c6940627f3a536e808a0a4d2730e3fe3edcad158c"

  depends_on "cmake" => :build
  depends_on "pkg-config" => :build
  depends_on "ffmpeg"
  depends_on "freetype"
  depends_on "harfbuzz"
  depends_on "cpr"
  depends_on "nlohmann-json"
  depends_on "cxxopts"

  def install
    system "cmake", "-S", ".", "-B", "build", *std_cmake_args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end
end
