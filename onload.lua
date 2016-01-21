print('onload.lua')

local fs = filesystem
local f = fs.open([[C:\Users\Tao\Desktop\rc.txt]], "r")
print(f:size())


