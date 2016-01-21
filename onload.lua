print('onload.lua')

local fs = filesystem
print(fs.ext("测试.exe"))
print(fs.exists([[C:\Users\Tao\Desktop\小萝莉的猴神大叔]]))
print(fs.exe_dir())
print(fs.type("C:\\"))
print(fs.simplify("/a/b/../c"))
local ls = fs.list([[C:\Users\Tao\Desktop]])
for k, v in pairs(ls) do
    print(k, v.isdir, v.file)
end

