local started = os.clock()
local value = 0.5

for _ = 1, 1000000 do
    value = value * 1.00000001 + 0.25
    value = value - 0.125
end

print(math.floor(value))
print(math.floor((os.clock() - started) * 1e9 + 0.5))
