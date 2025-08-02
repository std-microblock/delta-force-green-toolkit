set_project("df-green-toolkit")

set_languages("c++2b")
set_warnings("all")
add_rules("mode.releasedbg")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "build"})

add_requires("opencv", {
    ffmpeg = false
}, "cpptrace", "tesseract")

target("df-green-toolkit")
    set_kind("binary")
    add_defines("NOMINMAX")
    set_encodings("utf-8")
    add_packages("opencv", "cpptrace", "tesseract")
    add_files("src/*.cc", "src/*/**.cc")
    add_links("user32", "gdi32", "windowsapp")
    after_build(function (target)
        os.cp("resources/*", target:targetdir())
    end)