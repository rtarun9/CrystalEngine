workspace("nether-engine")
configurations({ "Debug", "Release" })

project("nether-engine")
kind("WindowedApp")
language("C++")
cppdialect("C++20")
targetdir("bin/%{cfg.buildcfg}")

files({ "src/**.hpp", "src/**.cpp" })
links({ "d3d12.lib", "dxgi.lib" })

filter("configurations:Debug")
defines({ "DEF_NETHER_DEBUG" })
symbols("On")

filter("configurations:Release")
optimize("On")
