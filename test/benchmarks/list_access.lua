local values = {3, 1, 4, 1, 5, 9, 2, 6}

local started = os.clock()
local total = 0

for index = 0, 499999 do
    total = total + values[index % 8 + 1]
end

print(total)
print(math.floor((os.clock() - started) * 1e9 + 0.5))
