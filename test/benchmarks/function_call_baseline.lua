local started = os.clock()
local total = 0

for index = 0, 199999 do
    total = total + index % 7
end

print(total)
print(math.floor((os.clock() - started) * 1e9 + 0.5))
