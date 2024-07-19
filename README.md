# Lua build for Windows

![AutoBuild](https://github.com/starwing/luabuild/workflows/AutoBuild/badge.svg)

## Build

- Open Visual Studio Developer Prompt
- Build single executable Lua: 
  ```cmd
  cl /Fesrc/lua.exe -I../src src/one.c Advapi32.lib
  ```
- Build dist:
  ```cmd
  src\lua src\luabuild.lua vs_rel_pdb
  ```

## Extensions

call `builtin()` function to load built-in module.

| Module | URL                                   |
| ------ | ------------------------------------- |
| lpath  | https://github.com/starwing/lpath     |
| miniz  | https://github.com/starwing/lua-miniz |

## Build Type

| Cmd        | Description                                          |
| ---------- | ---------------------------------------------------- |
| vs         | Build with Visual Studio                             |
| vs_dbg     | include *.pdb debug symbol                           |
| vs_rel     | using release profile                                |
| vs_rel_pdb | using release profile and include *.pdb debug symbol |
| vs_rel_min | Build minimal release                                |
| gcc        | Build with gcc (MinGW)                               |
| gcc_dbg    | Build with gcc and include debug symbol for gdb      |
| gcc_rel    | Build with gcc and release profile                   |


## Download Links

| Version | Arch | Link                                                         |
| ------- | ---- | ------------------------------------------------------------ |
| 5.1.5   | x86  | https://github.com/starwing/luabuild/releases/download/nightly/Lua-5.1.5-x86.exe |
| 5.1.5   | x64  | https://github.com/starwing/luabuild/releases/download/nightly/Lua-5.1.5-x64.exe |
| 5.2.4   | x86  | https://github.com/starwing/luabuild/releases/download/nightly/Lua-5.2.4-x86.exe |
| 5.2.4   | x64  | https://github.com/starwing/luabuild/releases/download/nightly/Lua-5.2.4-x64.exe |
| 5.3.6   | x86  | https://github.com/starwing/luabuild/releases/download/nightly/Lua-5.3.6-x86.exe |
| 5.3.6   | x64  | https://github.com/starwing/luabuild/releases/download/nightly/Lua-5.3.6-x64.exe |
| 5.4.7   | x86  | https://github.com/starwing/luabuild/releases/download/nightly/Lua-5.4.7-x86.exe |
| 5.4.7   | x64  | https://github.com/starwing/luabuild/releases/download/nightly/Lua-5.4.7-x64.exe |

