local DEBUG = false
local VERBOSE = false
local info = {
   ROOT =   [[..\]];
   SRCDIR = [[..\src\]];
   DSTDIR = [[dstdir\]];

   RM = 'del';
   CP = 'copy /y';
   QUIET = ' >nul 2>nul';
}
info.gcc = {
   CC = 'gcc $CFLAGS $flags -c $input';
   LD = 'gcc $CFLAGS $flags -o $output $input $libs';
   AR = 'ar rcs $output $input';
   RC = 'windres -i $input -o $RCOUT';
   RCOUT = '${output}.o';
   OBJ = '.o';
}
info.gcc_dbg = {
   base = info.gcc;
   CFLAGS = '-std=c99 -ggdb -pipe -O0 -Wall -fno-strict-aliasing';
}
info.gcc_rel = {
   base = info.gcc;
   CFLAGS = '-std=c99 -s -pipe -O3 -Wall -fno-strict-aliasing';
}
info.vs = {
   CC = 'cl /nologo $CFLAGS $flags /c $input';
   LD = 'link /nologo $LDFLAGS $flags /OUT:"$output" $input $libs';
   AR = 'lib /nologo /OUT:$output $input';
   RC = 'rc /nologo /Fo"$RCOUT" $input';
   RCOUT = '${output}.res';
   OBJ = '.obj';
}
info.vs_dbg = {
   base = info.vs;
   CFLAGS = '/W3 /D_CRT_SECURE_NO_DEPRECATE '..
            '/MTd /Zi /Ob0 /Od /RTC1 /D _DEBUG';
   LDFLAGS = '/DEBUG /INCREMENTAL:NO /PDB:"$output.pdb"';
}
info.vs_rel = {
   base = info.vs;
   CFLAGS = '/nologo /W3 /D_CRT_SECURE_NO_DEPRECATE '..
            '/MT /GS- /GL /Gy /Oy- /O2 /Oi /DNDEBUG';
   LDFLAGS = '/OPT:REF /OPT:ICF /INCREMENTAL:NO /LTCG:incremental';
}
info.vs_rel_pdb = {
   base = info.vs;
   CFLAGS = '/nologo /W3 /D_CRT_SECURE_NO_DEPRECATE '..
            '/MT /GS- /GL /Gy /Oy- /O2 /Oi /Zi /DNDEBUG';
   LDFLAGS = '/OPT:REF /OPT:ICF /INCREMENTAL:NO /LTCG:incremental /DEBUG:FASTLINK /PDB:"$output.pdb"';
}
info.vs_rel_min = {
   base = info.vs;
   CFLAGS = '/nologo /W3 /D_CRT_SECURE_NO_DEPRECATE '..
            '/MT /GS- /GL /Gy /O1 /Ob1 /Oi /Oy- /DNDEBUG';
   LDFLAGS = '/OPT:REF /OPT:ICF /INCREMENTAL:NO /LTCG:incremental';
}

local function find_version()
   local LUA_VERSION_MAJOR
   local LUA_VERSION_MINOR
   local LUA_VERSION_RELEASE
   local LUA_COPYRIGHT
   local LUA_RELEASE

   io.input(info.SRCDIR .. "lua.h")
   for line in io.lines() do
      local v
      v = line:match "#define%s+LUA_VERSION_MAJOR%s+\"(%d+)\""
      repeat
         if v then LUA_VERSION_MAJOR = v break end
         v = line:match "#define%s+LUA_VERSION_MAJOR_N%s+(%d+)"
         if v then LUA_VERSION_MAJOR = v break end
         v = line:match "#define%s+LUA_VERSION_MINOR%s+\"(%d+)\""
         if v then LUA_VERSION_MINOR = v break end
         v = line:match "#define%s+LUA_VERSION_MINOR_N%s+(%d+)"
         if v then LUA_VERSION_MINOR = v break end
         v = line:match "#define%s+LUA_VERSION_RELEASE%s+\"(%d+)\""
         if v then LUA_VERSION_RELEASE = v break end
         v = line:match "#define%s+LUA_VERSION_RELEASE_N%s+(%d+)"
         if v then LUA_VERSION_RELEASE = v break end
         v = line:match "#define%s+LUA_COPYRIGHT.-\"%s*(.-)\""
         if v then LUA_COPYRIGHT = v break end
         v = line:match "#define%s+LUA_RELEASE%s+\"(.-)\""
         if v then LUA_RELEASE = v break end
      until true
   end
   io.input():close()
   io.input(io.stdin)

   if not LUA_VERSION_MAJOR then
      assert(LUA_RELEASE, "can not find Lua release!!")
      LUA_VERSION_MAJOR,
      LUA_VERSION_MINOR,
      LUA_VERSION_RELEASE = LUA_RELEASE:match "^Lua (%d+)%.(%d+)%.(%d+)"
      assert(LUA_VERSION_MAJOR, "can not find Lua release!!")
   end
   info.LUA_RELEASE         = ("Lua %d.%d.%d"):format(
         LUA_VERSION_MAJOR,
         LUA_VERSION_MINOR,
         LUA_VERSION_RELEASE)
   if not LUA_COPYRIGHT:match "Lua" then
      LUA_COPYRIGHT = LUA_RELEASE..LUA_COPYRIGHT
   end
   print(("find Lua release: %s"):format(info.LUA_RELEASE, LUA_COPYRIGHT))
   info.LUA_VERSION_MAJOR   = LUA_VERSION_MAJOR
   info.LUA_VERSION_MINOR   = LUA_VERSION_MINOR
   info.LUA_VERSION_RELEASE = LUA_VERSION_RELEASE
   info.LUA_COPYRIGHT       = LUA_COPYRIGHT
   info.LUAV                = LUA_VERSION_MAJOR..LUA_VERSION_MINOR
end

local function expand(s, t)
   local count = 0
   local function replace(sv, space)
      sv = t and t[sv] or info[sv]
      if sv then
         if type(sv) == "table" then
            sv = table.concat(sv, " ")
         end
         sv = sv .. (space or "")
         count = count + 1
      end
      return sv or ""
   end
   assert(s, "template expected")
   while true do
      local old = count
      s = s:gsub("$%{([%w_]+)%}", replace)
      s = s:gsub("$([%w_]+)(%s*)", replace)
      if old == count then return s end
   end
end

local function patch_rcfile(file)
   local myInfo = {
      LUA_CSV_RELEASE = ("%d,%d,%d,0"):format(
         info.LUA_VERSION_MAJOR,
         info.LUA_VERSION_MINOR,
         info.LUA_VERSION_RELEASE);
   }

   print("[PATCH]\t"..file..".rc")
   io.input("res/"..file..".rc")
   io.output(file..".rc")

   for line in io.lines() do
      io.write(expand(line, myInfo), "\n")
   end

   io.input():close()
   io.output():close()
   io.input(io.stdin)
   io.output(io.stdout)
end

local function patch_luaconf()
   local LUA_VDIR = info.LUA_VERSION_MAJOR.."."..info.LUA_VERSION_MINOR
   local t = {
      path = [[
#define LUA_PATH_DEFAULT  ".\\?.lua;" ".\\?\\init.lua;" \
		LUA_CDIR "?.lua;" LUA_CDIR "?\\init.lua;" \
		LUA_CDIR "lua\\?.lua;" LUA_CDIR "lua\\?\\init.lua;" \
		LUA_CDIR "clibs\\?.lua;" LUA_CDIR "clibs\\?\\init.lua;" \
		LUA_CDIR "..\\share\\lua\\]]..LUA_VDIR..[[\\?.lua;" \
		LUA_CDIR "..\\share\\lua\\]]..LUA_VDIR..[[\\?\\init.lua"]];
      cpath = [[
#define LUA_CPATH_DEFAULT  ".\\?.dll;" ".\\loadall.dll;" \
		LUA_CDIR "?.dll;" LUA_CDIR "loadall.dll;" \
		LUA_CDIR "clibs\\?.dll;" LUA_CDIR "clibs\\loadall.dll;" \
		LUA_CDIR "..\\lib\\lua\\]]..LUA_VDIR..[[\\?.dll;" \
		LUA_CDIR "..\\lib\\lua\\]]..LUA_VDIR..[[\\loadall.dll"]];
   }

   print("[PATCH]\tluaconf.h")
   io.input(info.SRCDIR.."luaconf.h")
   io.output "src/luaconf.h"
   local patched = 0
   local begin
   for line in io.lines() do
      local cur = line
      if patched < 2 then
         if begin and not line:match "\\$" then
            cur = t[begin]
            patched = patched + 1
            begin = nil
         elseif line:match "#define%s+LUA_PATH_DEFAULT" then
            begin = "path"
         elseif line:match "#define%s+LUA_CPATH_DEFAULT" then
            begin = "cpath"
         end
      end

      if not begin then io.write(cur, "\n") end
   end
   io.input():close()
   io.output():close()
   io.input(io.stdin)
   io.output(io.stdout)
end

local function glob(pattern)
   local fh = assert(io.popen("DIR /B /W "..pattern))
   local files = {}
   for line in fh:lines() do
      files[#files+1] = line
   end
   fh:close()
   return files
end

local function map(files, f)
   local t = {}
   for i, v in ipairs(files) do
      local new = f(i, v)
      if new then t[#t+1] = new end
   end
   return t
end

local function tsub(files, pattern, replace)
   local t = {}
   for i, v in ipairs(files) do
      t[i] = v:gsub(pattern, replace)
   end
   return t
end

local function execute(fmt, t)
   local cmdline = expand(fmt, t)
   if VERBOSE then
      print(">>", cmdline)
   end
   return assert(os.execute(cmdline))
end

local function find_toolchain(toolchain)
   if not toolchain then
      --local env = os.getenv "VS120COMNTOOLS" or -- VS2013
                  --os.getenv "VS110COMNTOOLS" or -- VS2012
                  --os.getenv "VS100COMNTOOLS" or -- VS2010
                  --os.getenv "VS90COMNTOOLS"     -- VS2008
      --if env then
         --execute("call "..env.."vsvars32.bat")
      --end
      if os.execute(expand[[cl $QUIET]]) then
         print("find VS toolchain")
         toolchain = "vs"
      elseif os.execute(expand[[gcc --version $QUIET]]) then
         print("find GCC toolchain")
         toolchain = "gcc"
      end
      if not toolchain then
         print("can not find toolchain!!!")
      end
      toolchain = toolchain .. (DEBUG and "_dbg" or "_rel")
   end
   print("use toolchain: "..toolchain)
   info.TOOLCHAIN = toolchain
   local t = info[toolchain]
   repeat
      for k,v in pairs(t) do
         info[k] = v
      end
      t = t.base
   until not t
end

local function compile(file, flags)
   if type(file) == "string" then
      print("[CC]\t"..file)
   end
   return execute("$CC", {
      input = file,
      flags = flags,
   })
end

local function compile_rc(file)
   print("[RC]\t"..file)
   local t = {
      input = file,
      output = file,
   }
   if execute(info.RC, t) then
      return expand(info.RCOUT, t)
   end
end

local function compile_def(name)
   print("[GEN]\t"..name..".def")
   local f = assert(io.popen("DUMPBIN /EXPORTS "..name..".exe"))
   local exports = {
      "LIBRARY "..name..".dll",
      "EXPORTS",
   }
   for line in f:lines() do
      local ordinal, hint, rva, api =
         line:match "(%d+)%s+(%x+)%s+(%x+)%s+([%a_][%w_]+)"
      if ordinal then
         -- exports[#exports + 1] = ("%s=%s.exe.%s @%d"):format(api, name, api, ordinal)
         exports[#exports + 1] = ("%s=%s @%d"):format(api, api, ordinal)
      end
   end
   f:close()
   local def = name .. ".def"
   local f = assert(io.open(def, "w"))
   f:write(table.concat(exports, "\n"))
   f:close()
   return def
end

local function link(target, files, flags, libs)
   print("[LINK]\t"..target)
   return execute(info.LD, {
      flags = flags,
      input = files,
      output = target,
      libs = libs or "Advapi32.lib",
   })
end

local function library(lib, files)
   print("[AR]\t"..lib)
   return execute(info.AR, {
      output = lib,
      input = files,
   })
end

local function buildone_luas()
   patch_rcfile "luas"
   local LUAV = info.LUAV
   local rc = compile_rc "luas.rc"
   local flags = { "-DLUA_BUILD_AS_DLL -DMAKE_LUA -I$SRCDIR" }
   local ldflags = {}
   if tonumber(LUAV) >= 53 then
      flags[#flags+1] = "-DHAVE_LPREFIX"
   end
   if info.TOOLCHAIN:match "^gcc" then
      ldflags[#ldflags+1] = "-Wl,--out-implib,liblua"..LUAV..".exe.a"
   else
      ldflags[#ldflags+1] = "/DELAYLOAD:lua54.dll delayimp.lib"
   end
   compile("src/one.c", flags)
   --if tonumber(LUAV) >= 54 then
   --    link("lua.exe", "one$OBJ "..rc, ldflags)
   -- else
      link("lua"..LUAV..".exe", "one$OBJ "..rc, ldflags)
      if info.TOOLCHAIN:match "^vs" then
         execute[[move /Y lua${LUAV}.lib lua${LUAV}exe.lib $QUIET]]
         execute[[move /Y lua${LUAV}.exp lua${LUAV}exe.exp $QUIET]]
      end
   --end
end

local function buildone_luadll(noproxy)
   local LUAV = info.LUAV
   local ldflags = {}
   local is_gcc = info.TOOLCHAIN:match "^gcc"
   if is_gcc or noproxy then
      patch_rcfile "luadll"
      local rc = compile_rc "luadll.rc"
      local flags = { "-DLUA_BUILD_AS_DLL -DMAKE_LIB -I$SRCDIR" }
      if is_gcc then
         if tonumber(LUAV) >= 53 then
            flags[#flags+1] = "-DHAVE_LPREFIX"
         end
         ldflags[#ldflags+1] = "-mdll"
         ldflags[#ldflags+1] = "-Wl,--out-implib,liblua"..LUAV..".dll.a"
         ldflags[#ldflags+1] = "-Wl,--output-def,lua"..LUAV..".def"
      else
         ldflags[#ldflags+1] = "/DLL"
      end
      compile("src/one.c ", flags)
      link("lua"..LUAV..".dll", "one$OBJ "..rc, ldflags)
   else
      patch_rcfile "luaproxy"
      local rc = compile_rc "luaproxy.rc"
      local def = compile_def("lua"..LUAV)
      ldflags[#ldflags+1] = "/DLL"
      ldflags[#ldflags+1] = "/NOENTRY"
      ldflags[#ldflags+1] = "/DEF:"..def
      ldflags[#ldflags+1] = "/IMPLIB:lua${LUAV}.lib"
      ldflags[#ldflags+1] = "/DELAYLOAD:lua${LUAV}.exe"
      ldflags[#ldflags+1] = "lua${LUAV}exe.lib"
      compile("src/lproxy.c", "-I$SRCDIR")
      link("lua"..LUAV..".dll", "lproxy.obj "..rc, ldflags, "kernel32.lib delayimp.lib")
      execute[[$RM /s/q lua${LUAV}.dll.pdb 2>nul]]
   end
end

local function build_lua()
   patch_rcfile "lua"
   local LUAV = info.LUAV
   local rc = compile_rc "lua.rc"
   local flags = "-DLUA_BUILD_AS_DLL -I$SRCDIR"
   local libs
   if info.TOOLCHAIN:match "^gcc" then
      libs = "-L. -llua"..LUAV..".dll"
      compile("src/lua.c ", flags)
      link("lua.exe", "lua$OBJ "..rc, nil, libs)
   else
      execute("$CP lua${LUAV}.exe lua.exe")
   end
end

local function buildone_luac()
   patch_rcfile "luac"
   local LUAV = info.LUAV
   local rc = compile_rc "luac.rc"
   local flags = "-DMAKE_LUAC -I$SRCDIR"
   if tonumber(LUAV) >= 53 then
      flags = flags .. " -DHAVE_LPREFIX"
   end
   compile("src/one.c ", flags)
   link("luac.exe", "one$OBJ "..rc)
end

local function build_lualib()
   print("[CC]\tlualib")
   local files = map(glob(info.SRCDIR.."*.c"), function(_, v)
      if v ~= "lua.c" and v ~= "luac.c" and v ~= "linit.c" then
         return info.SRCDIR .. v
      end
   end)
   files[#files+1] = "src/lpath.c"
   files[#files+1] = "src/lminiz.c"
   files[#files+1] = "src/linit.c"
   files[#files+1] = "src/lfmt.c"
   files[#files+1] = "src/lmp.c"
   local LUAV = info.LUAV
   compile(files, "-DLUA_BUILD_AS_DLL -I$SRCDIR")
   if info.TOOLCHAIN:match "^gcc" then
      library("liblua"..LUAV..".a",
         tsub(files, ".*[/\\]([^/\\]+).c$", "%1.o"))
   else
      library("lua"..LUAV.."s.lib",
         tsub(files, ".*[/\\]([^/\\]+).c$", "%1.obj"))
   end
   execute("$RM /s /q *.o *.obj $QUIET")
end

local function make_dirs()
   print("[MKDIR]\t"..info.DSTDIR)
   execute [[del /s /q $DSTDIR $QUIET]]
   execute [[mkdir ${DSTDIR}         $QUIET]]
   execute [[mkdir ${DSTDIR}clibs    $QUIET]]
   execute [[mkdir ${DSTDIR}doc      $QUIET]]
   execute [[mkdir ${DSTDIR}lua      $QUIET]]
   execute [[mkdir ${DSTDIR}include  $QUIET]]
   execute [[mkdir ${DSTDIR}lib      $QUIET]]
end

local function install_doc()
   print("[INSTALL]\tdocuments")
   for _, v in ipairs(glob(info.ROOT.."doc")) do
      execute([[$CP ${ROOT}doc\$output ${DSTDIR}doc $QUIET]], { output = v })
   end
end

local function install_headers()
   print "[INSTALL]\theaders"
   execute[[$CP src\luaconf.h      ${DSTDIR}include $QUIET]]
   execute[[$CP ${SRCDIR}lua.h     ${DSTDIR}include $QUIET]]
   execute[[$CP ${SRCDIR}lua.hpp   ${DSTDIR}include $QUIET]]
   execute[[$CP ${SRCDIR}lauxlib.h ${DSTDIR}include $QUIET]]
   execute[[$CP ${SRCDIR}lualib.h  ${DSTDIR}include $QUIET]]
end

local function install_executables()
   print "[INSTALL]\texecutables"
   execute[[$CP lua.exe $DSTDIR $QUIET]]
   execute[[$CP luac.exe $DSTDIR $QUIET]]
   execute[[$CP lua$LUAV.exe $DSTDIR $QUIET]]
   execute[[$CP lua$LUAV.dll $DSTDIR $QUIET]]
   execute[[$RM vc*.pdb]]
   execute[[$CP *.pdb $DSTDIR $QUIET]]
end

local function install_libraries()
   print "[INSTALL]\tlibraries"
   execute[[$CP *.a   ${DSTDIR}lib $QUIET]]
   execute[[$CP *.lib ${DSTDIR}lib $QUIET]]
   execute[[$CP *.def ${DSTDIR}lib $QUIET]]
   execute[[$CP *.exp ${DSTDIR}lib $QUIET]]
end

local function dist()
   local assert = _G.assert
   _G.assert = function(...) return ... end
   info.DSTDIR = expand[[Lua$LUAV$TOOLCHAIN\]]
   print("[INSTALL]\t"..info.DSTDIR)
   make_dirs()
   install_doc()
   install_headers()
   install_executables()
   install_libraries()
   _G.assert = assert
end

local function cleanup()
   local assert = _G.assert
   _G.assert = function(...) return ... end
   print("[CLEANUP]")
   execute[[$RM *.def *.a *.exe *.dll *.rc *.o $QUIET]]
   execute[[$RM *.obj *.lib *.exp *.res *.pdb *.ilk $QUIET]]
   execute[[$RM *.idb *.ipdb *.iobj $QUIET]]
   execute[[$RM src/luaconf.h $QUIET]]
   _G.assert = assert
end

-- begin build
while arg[1] and arg[1]:sub(1,1) == '-' do
   if arg[1] == '-v' then
      VERBOSE = true
   elseif arg[1] == '-d' then
      DEBUG = true
   elseif arg[1] == '-h' or arg[1] == '-?' then
      print(arg[0].." [-v] [-d] [toolchain]")
      print("support toolchain:")
      local tls = {}
      for k,v in pairs(info) do
         if type(v) == 'table' and v.base then
            tls[#tls+1] = k
         end
      end
      table.sort(tls)
      print("    "..table.concat(tls, "\n    "))
      return
   end
   table.remove(arg, 1)
end

find_version()
patch_luaconf()

find_toolchain(arg[1])
buildone_luas()
buildone_luadll(true)
--if tonumber(info.LUAV) < 54 then
   build_lua()
--end
buildone_luac()
build_lualib()
dist()
cleanup()
print "[DONE]"

-- cc: cc='D:\lua53\lua.exe'
