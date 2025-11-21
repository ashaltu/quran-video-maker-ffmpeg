class QvmFfmpeg < Formula
  desc "Quran Video Maker (FFmpeg)"
  homepage "https://github.com/ashaltu/quran-video-maker-ffmpeg"
  url "https://github.com/ashaltu/quran-video-maker-ffmpeg/releases/download/v0.0.0-test1-g/qvm-ffmpeg-v0.0.0-test1-g.tar.gz"
  sha256 "da27a3ccf2e45cba8be2dca5bbf4eefab8971288236adf9a581f94629ba35c39"

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
