newoption({
	trigger = "platform_backend",
	value = "string",
	description = "Specify the platform backend (options: win32)",
})

newoption({
	trigger = "gpu_api_backend",
	value = "string",
	description = "Specify the GPU API backend (options: dx12)",
})

workspace("nether-engine")
configurations({ "Debug", "Release" })
architecture("x86_64")

include("imgui-premake/premake5.lua")

project("nether-engine")
kind("ConsoleApp")
language("C++")
cppdialect("C++20")
targetdir("bin/%{cfg.buildcfg}")

includedirs({ "imgui-premake" })

files({ "src/**.hpp", "src/**.cpp" })
links({ "ImGui", "d3d12.lib", "dxgi.lib", "d3dcompiler.lib", "dxcompiler.lib" })

filter("configurations:Debug")
defines({ "DEF_NETHER_DEBUG" })
symbols("On")

filter("configurations:Release")
optimize("On")
