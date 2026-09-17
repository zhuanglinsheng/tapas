local started = os.clock()
local total = 0

for index = 0, 1999999 do
    if index % 2 ~= 0 then
        total = total + index % 7
        total = total + index % 5
        total = total - index % 3
        total = total + index % 11
    end
end

print(total)
print(math.floor((os.clock() - started) * 1e9 + 0.5))
