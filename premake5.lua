workspace("nether-engine")
configurations({ "Debug", "Release" })
architecture("x86_64")

project("nether-engine")
kind("ConsoleApp")
language("C++")
cppdialect("C++20")
targetdir("bin/%{cfg.buildcfg}")

files({ "src/**.hpp", "src/**.cpp" })
links({ "d3d12.lib", "dxgi.lib", "d3dcompiler.lib", "dxcompiler.lib" })

filter("configurations:Debug")
defines({ "DEF_NETHER_DEBUG" })
symbols("On")

filter("configurations:Release")
optimize("On")
